#ifndef _FRANK_E1000E_H
#define _FRANK_E1000E_H

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define DRIVER_NAME		"frank_e1000e"
#define DRIVER_VERSION	"1.0.0"

#define FRANK_E1000E_DEV_ID_82540EM		0x100E
#define FRANK_E1000E_DEV_ID_82545EM		0x100F
#define FRANK_E1000E_DEV_ID_82571EB		0x105E
#define FRANK_E1000E_DEV_ID_82574L		0x10D3

#define FRANK_E1000E_IMC_REG			0x000D8
#define   FRANK_E1000E_IMC_TXDW			BIT(0)
#define   FRANK_E1000E_IMC_TXQE			BIT(1)
#define   FRANK_E1000E_IMC_LSC			BIT(2)
#define   FRANK_E1000E_IMC_RXDMT0		BIT(4)
#define   FRANK_E1000E_IMC_RXO			BIT(6)
#define   FRANK_E1000E_IMC_RXT0			BIT(7)
#define   FRANK_E1000E_IMC_MDAC			BIT(9)
#define   FRANK_E1000E_IMC_TXD_LOW		BIT(15)
#define   FRANK_E1000E_IMC_SRPD			BIT(16)
#define   FRANK_E1000E_IMC_ACK			BIT(17)
#define   FRANK_E1000E_IMC_MNG			BIT(18)
#define   FRANK_E1000E_IMC_RXQ0			BIT(20)
#define   FRANK_E1000E_IMC_RXQ1			BIT(21)
#define   FRANK_E1000E_IMC_TXQ0			BIT(22)
#define   FRANK_E1000E_IMC_TXQ1			BIT(23)
#define   FRANK_E1000E_IMC_OTHER		BIT(24)
#define   FRANK_E1000E_IMC_ALL			\
		( FRANK_E1000E_IMC_TXDW | FRANK_E1000E_IMC_TXQE |\
		FRANK_E1000E_IMC_LSC | FRANK_E1000E_IMC_RXDMT0 |\
		FRANK_E1000E_IMC_RXO | FRANK_E1000E_IMC_RXT0 |\
		FRANK_E1000E_IMC_MDAC | FRANK_E1000E_IMC_TXD_LOW |\
		FRANK_E1000E_IMC_SRPD | FRANK_E1000E_IMC_ACK |\
		FRANK_E1000E_IMC_MNG | FRANK_E1000E_IMC_RXQ0 |\
		FRANK_E1000E_IMC_RXQ1 | FRANK_E1000E_IMC_TXQ0 |\
		FRANK_E1000E_IMC_TXQ1 | FRANK_E1000E_IMC_OTHER)


struct frank_e1000e_adapter; 

struct frank_e1000e_hw {
	void __iomem 					*hw_addr;
	struct frank_e1000e_adapter		*adapter;
	u16		device_id;
	u16		vendor_id;
};

struct frank_e1000e_adapter {
	struct net_device		*netdev;
	struct pci_dev			*pci;
	struct frank_e1000e_hw	*hw;


	u32		msg_enable;
};

static inline void frank_e1000e_writel(struct frank_e1000e_hw *hw, u32 reg, u32 val)
{
	writel(val, hw->hw_addr + reg);
}

static inline u32 frank_e1000e_readl(struct frank_e1000e_hw *hw, u32 reg)
{
	return readl(hw->hw_addr + reg);
}


#endif /*_FRANK_E1000E_H*/