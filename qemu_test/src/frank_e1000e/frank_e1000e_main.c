#include "frank_e1000e.h"

static void frank_e1000e_disable_intr(struct frank_e1000e_adapter *adapter)
{
	frank_e1000e_writel(adapter->hw, FRANK_E1000E_IMC_REG, FRANK_E1000E_INT_ALL);
}

static void frank_e1000e_enable_intr(struct frank_e1000e_adapter *adapter)
{
	frank_e1000e_writel(adapter->hw, FRANK_E1000E_IMS_REG, FRANK_E1000E_INT_ALL);
}

static void frank_e1000e_set_link_state(struct frank_e1000e_adapter *adapter, int up)
{
	u32 val;

	val = frank_e1000e_readl(adapter->hw, FRANK_E1000E_CTRL_REG);
	if (up) 
		val |= FRANK_E1000E_CTRL_SLU;
	else
		val &= ~FRANK_E1000E_CTRL_SLU;

	frank_e1000e_writel(adapter->hw, FRANK_E1000E_CTRL_REG, val);
}


static int frank_e1000e_ndo_open(struct net_device *netdev)
{
	struct frank_e1000e_adapter *adapter = netdev->ml_priv;
	struct pci_dev *pdev = adapter->pci;

	pci_info(pdev, "Network interface opened\n");

	frank_e1000e_enable_intr(adapter);

	frank_e1000e_set_link_state(adapter, 1);

	netif_carrier_on(netdev);
	netif_start_queue(netdev);

	return 0;
}

static int frank_e1000e_ndo_stop(struct net_device *netdev)
{
	struct frank_e1000e_adapter *adapter = netdev->ml_priv;
	struct pci_dev *pdev = adapter->pci;

	pci_info(pdev, "Network interface stop\n");

	frank_e1000e_set_link_state(adapter, 0);

	netif_carrier_off(netdev);
	frank_e1000e_disable_intr(adapter);

	netif_stop_queue(netdev);

	return 0;
} 

static netdev_tx_t frank_e1000e_ndo_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct frank_e1000e_adapter *adapter = netdev->ml_priv;
	struct pci_dev *pdev = adapter->pci;
	struct frank_e1000e_tx_desc *tx_desc;
	dma_addr_t dma_addr;
	unsigned int tx_tail;
	u32 cmd_flags;

	pci_info(pdev, "ndo_start_xmit called: skb len=%d\n", skb->len);

	tx_tail = adapter->tx_tail;
	if (((tx_tail + 1) % adapter->tx_ring_size) == adapter->tx_head ) {
		netif_stop_queue(netdev);
		pci_info(pdev, "TX ring full, stopping queue\n");
		return NETDEV_TX_BUSY;
	}

	if (skb->len <= 0) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;	
	}

	if (skb_put_padto(skb, ETH_ZLEN)) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	tx_tail = adapter->tx_tail;
	tx_desc = &adapter->tx_ring[tx_tail];

	dma_addr = dma_map_single(&pdev->dev,
						skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(&pdev->dev, dma_addr)) {
		dev_kfree_skb_any(skb);
		netdev->stats.tx_errors ++;
		pci_info(pdev, "Failed to mapping skb\n");
		return NETDEV_TX_OK;
	}

	tx_desc->buffer_addr = cpu_to_le64(dma_addr);

	cmd_flags = FRANK_E1000E_TXD_CMD_EOP | FRANK_E1000E_TXD_CMD_IFCS |
				FRANK_E1000E_TXD_CMD_RS;
	tx_desc->lower.flags.length = cpu_to_le16(skb->len);
	tx_desc->lower.flags.cmd = cmd_flags;
	tx_desc->upper.data = 0;

	adapter->tx_skb[tx_tail] = skb;

	adapter->tx_tail = (tx_tail + 1) % adapter->tx_ring_size;

	wmb();

	frank_e1000e_writel(adapter->hw, FRANK_E1000E_TDT_REG, adapter->tx_tail);

	return NETDEV_TX_OK;
 } 

static const struct net_device_ops frank_e1000e_netdev_ops = {
	.ndo_open = frank_e1000e_ndo_open,
	.ndo_stop = frank_e1000e_ndo_stop,
	.ndo_start_xmit = frank_e1000e_ndo_start_xmit,
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
	val &= ~FRANK_E1000E_CTRL_SLU;

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

static void frank_e1000e_clear_tx_ring(struct frank_e1000e_adapter *adapter)
{
	struct frank_e1000e_tx_desc *desc;
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pci;
	unsigned int tx_head = adapter->tx_head;
	struct sk_buff *skb;
	unsigned int cleaned = 0;

	while((tx_head != adapter->tx_tail)) {
		desc = &adapter->tx_ring[tx_head];

		if ( !(desc->upper.fields.status & FRANK_E1000E_TXD_STAT_DD))
			break;

		skb = adapter->tx_skb[tx_head];
		if (skb) {
			/* Update TX statistics */
			netdev->stats.tx_packets++;
			netdev->stats.tx_bytes += skb->len;
			
			dma_unmap_single(&pdev->dev, le64_to_cpu(desc->buffer_addr),
					le16_to_cpu(desc->lower.flags.length),
					DMA_TO_DEVICE);
			dev_kfree_skb_irq(skb);
			adapter->tx_skb[tx_head] = NULL;
		}

		desc->upper.fields.status = 0;
		tx_head = (tx_head + 1) % adapter->tx_ring_size;
		
		cleaned ++;
	}

	adapter->tx_head = tx_head;

	if (cleaned && netif_queue_stopped(netdev)) {
		netif_wake_queue(netdev);
		pci_info(pdev, "TX queue restarted after cleaning %u desc\n",
			cleaned);
	}
}

static irqreturn_t frank_e1000e_irq_handler(int irq, void *data)
{
	struct frank_e1000e_adapter *adapter = data;
	struct pci_dev *pdev = adapter->pci;
	u32 val, status;

	val = frank_e1000e_readl(adapter->hw, FRANK_E1000E_ICR_REG);
	
	if (val & FRANK_E1000E_INT_LSC) {
		status = frank_e1000e_readl(adapter->hw, FRANK_E1000E_STATUS_REG);
		pci_info(pdev, "Link %s\n", (status & FRANK_E1000E_STATUS_LU) ? 
				"Up" : "Down");
	}

	if (val & (FRANK_E1000E_INT_TXDW | FRANK_E1000E_INT_TXQE) ) {
		frank_e1000e_clear_tx_ring(adapter);
	}
	
	
	frank_e1000e_writel(adapter->hw, FRANK_E1000E_ICR_REG, val);
	return IRQ_HANDLED;
}

static int frank_e1000e_init_irq(struct frank_e1000e_adapter *adapter)
{	
	int ret = 0;
	struct pci_dev *pdev = adapter->pci;

	ret = devm_request_irq(&pdev->dev, pdev->irq, frank_e1000e_irq_handler,
					IRQF_SHARED, DRIVER_NAME, adapter);
	if (ret) {
		pci_err(pdev, "Failed to request irq\n");		
	}

	return ret;
}

static int frank_e1000e_setup_tx_ring(struct frank_e1000e_adapter *adapter)
{
	size_t size;
	u64 tdba;
	u32 val;
	struct pci_dev *pdev = adapter->pci;
	
	adapter->tx_ring_size = FRANK_E1000E_TX_RING_SIZE;
	
	adapter->tx_head = 0;
	adapter->tx_tail = 0;

	size = adapter->tx_ring_size * sizeof(struct frank_e1000e_tx_desc);
	size = ALIGN(size, 4096);

	adapter->tx_ring = dmam_alloc_coherent(&pdev->dev, size, 
						&adapter->tx_ring_dma, GFP_KERNEL);
	if (!adapter->tx_ring) {
		pci_err(pdev, "Failed to alloc tx ring\n");
		return -ENOMEM;
	}

	adapter->tx_skb = devm_kzalloc(&pdev->dev, 
						adapter->tx_ring_size * sizeof(struct sk_buff *), GFP_KERNEL);

	if (!adapter->tx_skb) {
		pci_err(pdev, "Failed to alloc tx_skb\n");
		return -ENOMEM;
	}

	tdba = adapter->tx_ring_dma; 
	size = adapter->tx_ring_size * sizeof(struct frank_e1000e_tx_desc);

	frank_e1000e_writel(adapter->hw, FRANK_E1000E_TDBAL_REG, tdba & DMA_BIT_MASK(32));
	frank_e1000e_writel(adapter->hw, FRANK_E1000E_TDBAH_REG, (tdba >> 32) & 0xFFFFFFFF);
	frank_e1000e_writel(adapter->hw, FRANK_E1000E_TDLEN_REG, size);
	frank_e1000e_writel(adapter->hw, FRANK_E1000E_TDH_REG, adapter->tx_head);
	frank_e1000e_writel(adapter->hw, FRANK_E1000E_TDT_REG, adapter->tx_tail);

	val = frank_e1000e_readl(adapter->hw, FRANK_E1000E_TCTL_REG);
	val &= ~FRANK_E1000E_TCTL_CT_MASK;

	val |= FRANK_E1000E_TCTL_EN;
	val |= FRANK_E1000E_TCTL_PSP | FRANK_E1000E_TCTL_RTLC;
	val |= FRANK_E1000E_TCTL_CT_SET(FRANK_E1000E_COLLISION_THRESHOLD);

	frank_e1000e_writel(adapter->hw, FRANK_E1000E_TCTL_REG, val);

	pci_info(pdev, "TX ring initialize with %u descriptors\n",
			adapter->tx_ring_size);

	return 0; 
}

static int frank_e1000e_init(struct frank_e1000e_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pci;
	int ret;
	
	pci_info(pdev, "Init frank e1000e\n");

	frank_e1000e_disable_intr(adapter);
	
	frank_e1000e_sw_reset(adapter);
	
	frank_e1000e_hw_init(adapter);

	ret = frank_e1000e_init_irq(adapter);
	if (ret) {
		goto error;
	}

	ret = frank_e1000e_setup_tx_ring(adapter);
	if (ret) {
		goto error;
	}

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

	pci_set_master(pdev);

	if ((ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64)))) {
		pci_info(pdev, "DMA configuration 64 bit failed\n");

		if ((ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32)))) {
			pci_err(pdev, "DMA configuration failed\n");
			goto error;
		}
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