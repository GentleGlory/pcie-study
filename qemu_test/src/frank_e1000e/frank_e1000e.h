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
#include <linux/bits.h>
#include <linux/iopoll.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#define DRIVER_NAME		"frank_e1000e"
#define DRIVER_VERSION	"1.0.0"

#define FRANK_E1000E_DEV_ID_82540EM		0x100E
#define FRANK_E1000E_DEV_ID_82545EM		0x100F
#define FRANK_E1000E_DEV_ID_82571EB		0x105E
#define FRANK_E1000E_DEV_ID_82574L		0x10D3

#define FRANK_E1000E_CTRL_REG			0x00000
#define   FRANK_E1000E_CTRL_RST			BIT(26)
#define   FRANK_E1000E_CTRL_FRCDPLX		BIT(12)
#define   FRANK_E1000E_CTRL_FRCSPD		BIT(11)
#define   FRANK_E1000E_CTRL_SLU			BIT(6)
#define   FRANK_E1000E_CTRL_ASDE		BIT(5)

#define FRANK_E1000E_STATUS_REG			0x00008
#define   FRANK_E1000E_STATUS_LU		BIT(1)

#define FRANK_E1000E_EERD_REG			0x00014
#define   FRANK_E1000E_EERD_START				BIT(0)
#define   FRANK_E1000E_EERD_DONE				BIT(1)
#define   FRANK_E1000E_EERD_ADDR_MASK		GENMASK(15, 2)
#define   FRANK_E1000E_EERD_ADDR(addr)		(((addr) & GENMASK(13, 0)) << 2)
#define   FRANK_E1000E_EERD_DATA(val)			(((val) & GENMASK(31,16)) >> 16)
#define   FRANK_E1000E_EERD_TIMEOUT		10000	//10ms

#define FRANK_E1000E_ICR_REG			0x000C0
#define   FRANK_E1000E_ICR_LSC			BIT(2)

#define FRANK_E1000E_IMC_REG			0x000D8
#define FRANK_E1000E_IMS_REG			0x000D0

#define   FRANK_E1000E_INT_TXDW			BIT(0)
#define   FRANK_E1000E_INT_TXQE			BIT(1)
#define   FRANK_E1000E_INT_LSC			BIT(2)
#define   FRANK_E1000E_INT_RXDMT0		BIT(4)
#define   FRANK_E1000E_INT_RXO			BIT(6)
#define   FRANK_E1000E_INT_RXT0			BIT(7)
#define   FRANK_E1000E_INT_MDAC			BIT(9)
#define   FRANK_E1000E_INT_TXD_LOW		BIT(15)
#define   FRANK_E1000E_INT_SRPD			BIT(16)
#define   FRANK_E1000E_INT_ACK			BIT(17)
#define   FRANK_E1000E_INT_MNG			BIT(18)
#define   FRANK_E1000E_INT_RXQ0			BIT(20)
#define   FRANK_E1000E_INT_RXQ1			BIT(21)
#define   FRANK_E1000E_INT_TXQ0			BIT(22)
#define   FRANK_E1000E_INT_TXQ1			BIT(23)
#define   FRANK_E1000E_INT_OTHER		BIT(24)
#define   FRANK_E1000E_INT_ALL			\
		( FRANK_E1000E_INT_TXDW | FRANK_E1000E_INT_TXQE |\
		FRANK_E1000E_INT_LSC | FRANK_E1000E_INT_RXDMT0 |\
		FRANK_E1000E_INT_RXO | FRANK_E1000E_INT_RXT0 |\
		FRANK_E1000E_INT_MDAC | FRANK_E1000E_INT_TXD_LOW |\
		FRANK_E1000E_INT_SRPD | FRANK_E1000E_INT_ACK |\
		FRANK_E1000E_INT_MNG | FRANK_E1000E_INT_RXQ0 |\
		FRANK_E1000E_INT_RXQ1 | FRANK_E1000E_INT_TXQ0 |\
		FRANK_E1000E_INT_TXQ1 | FRANK_E1000E_INT_OTHER)

#define FRANK_E1000E_TCTL_REG			0x00400
#define   FRANK_E1000E_TCTL_EN			BIT(1)
#define   FRANK_E1000E_TCTL_PSP			BIT(3)
#define   FRANK_E1000E_TCTL_CT_MASK		GENMASK(11, 4)
#define   FRANK_E1000E_TCTL_CT_GET(v)	(((v) & GENMASK(11, 4)) >> 4)
#define   FRANK_E1000E_TCTL_CT_SET(n)	(((n) & GENMASK(7, 0)) << 4)
#define   FRANK_E1000E_TCTL_COLD_GET(v)	(((v) & GENMASK(21, 12)) >> 12)
#define   FRANK_E1000E_TCTL_COLD_SET(n)	(((n) & GENMASK(9, 0)) << 12)
#define   FRANK_E1000E_TCTL_SWXOFF		BIT(22)
#define   FRANK_E1000E_TCTL_PBE			BIT(23)
#define   FRANK_E1000E_TCTL_RTLC		BIT(24)
#define   FRANK_E1000E_TCTL_UNORTX		BIT(25)
#define   FRANK_E1000E_TCTL_TXDSCMT_GET(v)	(((v) & GENMASK(27, 26)) >> 26)
#define   FRANK_E1000E_TCTL_TXDSCMT_SER(n)	(((n) & GENMASK(1, 0)) << 26)
#define   FRANK_E1000E_TCTL_MULR		BIT(28)
#define   FRANK_E1000E_TCTL_RRTHRESH_GET(v)	(((v) & GENMASK(30, 29)) >> 29)
#define   FRANK_E1000E_TCTL_RRTHRESH_SET(n)	(((n) & GENMASK(1, 0)) << 29)
#define   FRANK_E1000E_COLLISION_THRESHOLD 15


#define FRANK_E1000E_TDBAL_REG			0x03800
#define FRANK_E1000E_TDBAH_REG			0x03804
#define FRANK_E1000E_TDLEN_REG			0x03808
#define FRANK_E1000E_TDH_REG			0x03810
#define FRANK_E1000E_TDT_REG			0x03818

#define FRANK_E1000E_GCR_REG			0x05B00
#define   FRANK_E1000E_GCR_SW_INIT		BIT(22)

#define FRANK_E1000E_TX_RING_SIZE	256

#define FRANK_E1000E_TXD_CMD_EOP	BIT(0)	/* End of Packet */
#define FRANK_E1000E_TXD_CMD_IFCS	BIT(1)	/* Insert FCS */
#define FRANK_E1000E_TXD_CMD_IC		BIT(2)	/* Insert Checksum */
#define FRANK_E1000E_TXD_CMD_RS		BIT(3)	/* Report Status */

#define FRANK_E1000E_TXD_STAT_DD	BIT(0) /* Descriptor Done */


struct frank_e1000e_adapter; 
struct frank_e1000e_hw;
struct frank_e1000e_tx_desc; 

struct frank_e1000e_hw {
	void __iomem 					*hw_addr;
	struct frank_e1000e_adapter		*adapter;
	u16		device_id;
	u16		vendor_id;
};

/* TODO(human): TX descriptor ring structures
 * Need to add:
 * 1. TX descriptor structure (frank_e1000e_tx_desc)
 * 2. TX ring structure (frank_e1000e_tx_ring)
 * 3. TX ring management fields in adapter
 * 4. TX hardware register definitions
 */

struct frank_e1000e_tx_desc {
	__le64 buffer_addr;
	union {
		__le32 data;
		struct {
			__le16 length;
			u8 cso;
			u8 cmd;
		} flags;
	} lower;

	union {
		__le32 data;
		struct {
			u8 status;
			u8 css;
			__le16 special;
		} fields;
	} upper;
};

struct frank_e1000e_adapter {
	struct net_device		*netdev;
	struct pci_dev			*pci;
	struct frank_e1000e_hw	*hw;

	u8		mac_address[6];
	u32		msg_enable;

	struct frank_e1000e_tx_desc		*tx_ring;
	dma_addr_t						tx_ring_dma;
	unsigned int					tx_ring_size;
	unsigned int					tx_head;
	unsigned int					tx_tail;
	struct sk_buff					**tx_skb;
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