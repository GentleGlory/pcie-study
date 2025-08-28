#include "frank_e1000e.h"

static void frank_e1000e_link_up(struct frank_e1000e_adapter *adapter)
{
	u32 val;

	val = frank_e1000e_readl(adapter->hw, FRANK_E1000E_CTRL_REG);
	val |= FRANK_E1000E_CTRL_SLU;

	frank_e1000e_writel(adapter->hw, FRANK_E1000E_CTRL_REG, val);
}


static int frank_e1000e_ndo_open(struct net_device *netdev)
{
	struct frank_e1000e_adapter *adapter = netdev->ml_priv;
	struct pci_dev *pdev = adapter->pci;

	pci_info(pdev, "Network interface opened\n");

	frank_e1000e_link_up(adapter);

	/* TODO(human): Implement IRQ handler setup
	 * 1. Create interrupt handler function: frank_e1000e_irq_handler()
	 * 2. Use request_irq() to register the handler
	 * 3. Enable hardware interrupts using IMS register
	 * 4. Handle different interrupt types (link, RX, TX)
	 * Which interrupt types do you want to handle first?
	 */

	netif_start_queue(netdev);

	return 0;
}

static int frank_e1000e_ndo_stop(struct net_device *netdev)
{
	struct frank_e1000e_adapter *adapter = netdev->ml_priv;
	struct pci_dev *pdev = adapter->pci;

	pci_info(pdev, "Network interface stop\n");

	netif_stop_queue(netdev);

	return 0;
} 

static const struct net_device_ops frank_e1000e_netdev_ops = {
	.ndo_open = frank_e1000e_ndo_open,
	.ndo_stop = frank_e1000e_ndo_stop,
};

static int frank_e1000e_read_eeprom_word(struct frank_e1000e_adapter *adapter,
		u16 address, u16 *data) 
{
	int ret;
	u32 val = 0;
	val &= ~FRANK_E1000E_EERD_ADDR_MASK;
	val |= FRANK_E1000E_EERD_ADDR(address);
	val |= FRANK_E1000E_EERD_START;
	
	frank_e1000e_writel(adapter->hw, FRANK_E1000E_EERD_REG, val);
	
	ret = readl_poll_timeout(adapter->hw->hw_addr + FRANK_E1000E_EERD_REG,
					val, val & FRANK_E1000E_EERD_DONE,
					10, FRANK_E1000E_EERD_TIMEOUT);
	if (ret) {
		pci_err(adapter->pci, "Read eeprom time out\n");
		return ret;
	}

	*data = FRANK_E1000E_EERD_DATA(val);

	return 0;
}

static int frank_e1000e_read_mac_addr(struct frank_e1000e_adapter *adapter)
{
	int i;
	int ret;
	u16 word;
	
	for (i = 0; i < 3; i++){
		ret = frank_e1000e_read_eeprom_word(adapter, i , &word);
		if (ret) {
			pci_err(adapter->pci, "Failed to read mac address\n");
			return ret;
		}
		
		adapter->mac_address[i * 2] = word & 0xFF;
		adapter->mac_address[i * 2 + 1] = (word >> 8) & 0xFF;
	}

	pci_info(adapter->pci, "MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
			adapter->mac_address[0], adapter->mac_address[1], 
			adapter->mac_address[2], adapter->mac_address[3],
			adapter->mac_address[4], adapter->mac_address[5]);

	return 0;
} 

static void frank_e1000e_disable_intr(struct frank_e1000e_adapter *adapter)
{
	frank_e1000e_writel(adapter->hw, FRANK_E1000E_IMC_REG, FRANK_E1000E_IMC_ALL);
}

static void frank_e1000e_enable_intr(struct frank_e1000e_adapter *adapter)
{

}

static void frank_e1000e_sw_reset(struct frank_e1000e_adapter *adapter)
{
	u32 val;

	val = frank_e1000e_readl(adapter->hw, FRANK_E1000E_CTRL_REG);
	val |= FRANK_E1000E_CTRL_RST;

	frank_e1000e_writel(adapter->hw, FRANK_E1000E_CTRL_REG, val);

	/* Wait 1 microsecond for reset to complete */
	usleep_range(1, 10);

	val = frank_e1000e_readl(adapter->hw, FRANK_E1000E_GCR_REG);
	val |= FRANK_E1000E_GCR_SW_INIT;

	frank_e1000e_writel(adapter->hw, FRANK_E1000E_GCR_REG, val);
}

//Init the registers for mac and phy
static void frank_e1000e_hw_init(struct frank_e1000e_adapter *adapter)
{
	u32 val;
	
	val = frank_e1000e_readl(adapter->hw, FRANK_E1000E_CTRL_REG);
	val &= ~FRANK_E1000E_CTRL_FRCDPLX;
	val &= ~FRANK_E1000E_CTRL_FRCSPD;
	val &= ~FRANK_E1000E_CTRL_ASDE;

	frank_e1000e_writel(adapter->hw, FRANK_E1000E_CTRL_REG, val);
}

static int frank_e1000e_init_netdev(struct frank_e1000e_adapter *adapter)
{	
	int ret;
	struct net_device *netdev;
	struct pci_dev *pdev = adapter->pci;

	netdev = alloc_etherdev(0);
	if (!netdev) {
		pci_err(pdev, "Failed to alloc netdev\n");
		ret = -ENOMEM;
		goto alloc_etherdev_error;
	}

	netdev->netdev_ops = &frank_e1000e_netdev_ops;
	adapter->netdev = netdev;
	netdev->ml_priv = adapter;

	SET_NETDEV_DEV(netdev, &pdev->dev);
	netdev->dev.parent = &pdev->dev;
	
	eth_hw_addr_set(netdev, adapter->mac_address);

	ret = register_netdev(netdev);
	if (ret) {
		pci_err(pdev, "Failed to register netdev\n");
		goto register_netdev_error;
	}

	return 0;

register_netdev_error:
	free_netdev(netdev);
	adapter->netdev = NULL;
alloc_etherdev_error:
	return ret;
}

static int frank_e1000e_init(struct frank_e1000e_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pci;
	int ret;
	
	pci_info(pdev, "Init frank e1000e\n");

	/* TODO(human): Hardware initialization strategy 
	 * Decide what to do next in the probe function:
	 * 1. Reset the hardware to known state
	 * 2. Read MAC address from EEPROM  
	 * 3. Allocate and initialize net_device structure
	 * 4. Set up TX/RX descriptor rings
	 * 5. Configure interrupts
	 * Which approach would you prefer to start with?
	 */

	frank_e1000e_disable_intr(adapter);
	
	frank_e1000e_sw_reset(adapter);
	
	frank_e1000e_hw_init(adapter);

	ret = frank_e1000e_read_mac_addr(adapter);
	if (ret) {
		goto error;
	}

	ret = frank_e1000e_init_netdev(adapter);
	if (ret) {
		goto error;
	}

	frank_e1000e_enable_intr(adapter);

	return 0;

error:
	return ret;
}

static int frank_e1000e_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct frank_e1000e_adapter *adapter;
	struct frank_e1000e_hw *hw;
	struct device *dev = &pdev->dev;
	int ret;

	pci_info(pdev, "In Frank e1000e test driver probe function\n");

	adapter = devm_kzalloc(dev, sizeof(struct frank_e1000e_adapter), GFP_KERNEL);
	if (!adapter) {
		pci_err(pdev, "Failed to alloc private data\n");
		ret = -ENOMEM;
		goto error;
	}

	hw = devm_kzalloc(dev, sizeof(struct frank_e1000e_hw), GFP_KERNEL);
	if (!hw) {
		pci_err(pdev, "Failed to alloc hardware data\n");
		ret = -ENOMEM;
		goto error;
	}

	adapter->pci = pdev;
	adapter->hw = hw;
	hw->adapter = adapter;
	hw->device_id = id->device;
	hw->vendor_id = id->vendor;

	pci_set_drvdata(pdev, adapter);

	ret = pcim_enable_device(pdev);
	if (ret) {
		pci_err(pdev, "Failed to enable pci dev\n");
		goto error;
	}

	ret = pcim_request_all_regions(pdev, DRIVER_NAME);
	if (ret) {
		pci_err(pdev, "Failed to request regions\n");
		goto error;
	}

	hw->hw_addr = pcim_iomap(pdev, 0, 0);
	if (!hw->hw_addr) {
		pci_err(pdev, "Failed to map BAR 0\n");
		ret = -ENOMEM;
		goto error;
	}

	ret = frank_e1000e_init(adapter);
	if (ret) {
		pci_err(pdev, "Failed to init frank e1000e\n");
		goto error;
	}

	return 0;

error:
	return ret;
}

static void frank_e1000e_remove(struct pci_dev *pdev)
{
	struct frank_e1000e_adapter *adapter = pci_get_drvdata(pdev); 

	pci_info(pdev, "In Frank e1000e test driver remove function\n");

	if (adapter && adapter->netdev) {
		unregister_netdev(adapter->netdev);
		free_netdev(adapter->netdev);
		adapter->netdev = NULL;
	}
}

static const struct pci_device_id frank_e1000e_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, FRANK_E1000E_DEV_ID_82540EM)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, FRANK_E1000E_DEV_ID_82545EM)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, FRANK_E1000E_DEV_ID_82571EB)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, FRANK_E1000E_DEV_ID_82574L)},
	{},
};
MODULE_DEVICE_TABLE(pci, frank_e1000e_pci_tbl);

static struct pci_driver frank_e1000e_driver = {
	.name = DRIVER_NAME,
	.id_table = frank_e1000e_pci_tbl,
	.probe = frank_e1000e_probe,
	.remove = frank_e1000e_remove,
};

module_pci_driver(frank_e1000e_driver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank");
MODULE_DESCRIPTION("Frank e1000e Test Driver");
MODULE_VERSION("1.0");