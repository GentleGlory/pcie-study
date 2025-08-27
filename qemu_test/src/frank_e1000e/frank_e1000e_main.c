#include "frank_e1000e.h"

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

	/* TODO(human): Hardware initialization strategy 
	 * Decide what to do next in the probe function:
	 * 1. Reset the hardware to known state
	 * 2. Read MAC address from EEPROM  
	 * 3. Allocate and initialize net_device structure
	 * 4. Set up TX/RX descriptor rings
	 * 5. Configure interrupts
	 * Which approach would you prefer to start with?
	 */

	return 0;

error:
	return ret;	
}

static void frank_e1000e_remove(struct pci_dev *pdev)
{
	pci_info(pdev, "In Frank e1000e test driver remove function\n");
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