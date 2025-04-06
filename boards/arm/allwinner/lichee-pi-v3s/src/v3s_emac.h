#include "arch/allwinner/v3s_platform.h"

#define GETH_BASIC_CTL0          0x00
#define GETH_BASIC_CTL1          0x04
#define GETH_INT_STA             0x08
# define TX_INT                  (0x01 << 0)
# define TX_DMA_STOPPED_INT      (0x01 << 1)
# define TX_BUF_UA_INT           (0x01 << 2)
# define TX_TIMEOUT_INT          (0x01 << 3)
# define TX_UNDERFLOW_INT        (0x01 << 4)
# define TX_EARLY_INT            (0x01 << 5)
# define RX_INT                  (0x01 << 8)
# define RX_BUF_UA_INT           (0x01 << 9)
# define RX_DMA_STOPPED_INT      (0x01 << 10)
# define RX_TIMEOUT_INT          (0x01 << 11)
# define RX_OVERFLOW_INT         (0x01 << 12)
# define RX_EARLY_INT            (0x01 << 13)       
# define RGMII_LINK_STA_INT      (0x01 << 16)

#define GETH_INT_EN               0x0C
# define RX_EARLY_INT_EN          (1 << 13)
# define RX_OVERFLOW_INT_EN       (1 << 12)
# define RX_TIMEOUT_INT_EN        (1 << 11)
# define RX_DMA_STOPPED_INT_EN    (1 << 10)
# define RX_BUF_UA_INT_EN         (1 <<  9)
# define RX_INT_EN                (1 << 8)
# define TX_EARLY_INT_EN          (1 << 5)
# define TX_UNDERFLOW_INT_EN      (1 << 4)
# define TX_TIMEOUT_INT_EN        (1 << 3)
# define TX_BUF_UA_INT_EN         (1 << 2)
# define TX_DMA_STOPPED_INT_EN    (1 << 1)
# define TX_INT_EN                (1 << 0)

#define GETH_TX_CTL0              0x10
# define TX_EN                    (1 << 31)
# define TX_JABBER_DISABLE        (1 << 30)

#define GETH_TX_CTL1              0x14
# define TX_DMA_START             (1 << 31)
# define TX_DMA_EN                (1 << 30)
# define TX_MD_EN                 (1 << 1)
# define FLUSH_TX_FIFO            (1 << 0)

#define GETH_TX_FLOW_CTL          0x1C
#define GETH_TX_DESC_LIST         0x20
#define GETH_RX_CTL0              0x24
#define GETH_RX_CTL1              0x28
# define RX_DMA_START             (1 << 31)
# define RX_DMA_EN                (1 << 30)

#define GETH_RX_DESC_LIST         0x34
#define GETH_RX_FRM_FLT           0x38
#define GETH_RX_HASH0             0x40
#define GETH_RX_HASH1             0x44
#define GETH_MDIO_ADDR            0x48
#define GETH_MDIO_DATA            0x4C
#define GETH_ADDR_HI(reg)         (0x50 + ((reg) << 3))
#define GETH_ADDR_LO(reg)         (0x54 + ((reg) << 3))
#define GETH_TX_DMA_STA           0xB0
#define GETH_TX_CUR_DESC          0xB4
#define GETH_TX_CUR_BUF           0xB8
#define GETH_RX_DMA_STA           0xC0
#define GETH_RX_CUR_DESC          0xC4
#define GETH_RX_CUR_BUF           0xC8
#define GETH_RGMII_STA            0xD0
#define MII_BUSY                  0x00000001
#define MII_WRITE                 0x00000002


typedef enum {
       PHY_INTERFACE_MODE_MII,
       PHY_INTERFACE_MODE_GMII,
       PHY_INTERFACE_MODE_SGMII,
       PHY_INTERFACE_MODE_TBI,
       PHY_INTERFACE_MODE_RMII,
       PHY_INTERFACE_MODE_RGMII,
       PHY_INTERFACE_MODE_RGMII_ID,
       PHY_INTERFACE_MODE_RGMII_RXID,
       PHY_INTERFACE_MODE_RGMII_TXID,
       PHY_INTERFACE_MODE_RTBI,
       PHY_INTERFACE_MODE_XGMII,
       PHY_INTERFACE_MODE_NONE /* Must be last */
} phy_interface_t;


#define PLL1_CFG               0x00
#define PLL6_CFG               0x28
#define AHB1_GATING            (0x60)
#define GMAC_AHB_BIT           (1 << 17)


/* PHY address */
#define PHY_DM                 0x0010
#define PHY_AUTO_NEG           0x0020
#define PHY_POWERDOWN          0x0080
#define PHY_NEG_EN             0x1000

/* Tx Descriptor */
#define TXDES_1ST_OWN                   (1 << 31)
#define TXDES_1ST_HEADER_ERR            (1 << 16)
#define TXDES_1ST_LENGHT_ERR            (1 << 14)
#define TXDES_1ST_PAYLOAD_ERR           (1 << 12)
#define TXDES_1ST_CRS_ERR               (1 << 10)
#define TXDES_1ST_COL_ERR_0             (1 <<  9)
#define TXDES_1ST_COL_ERR_1             (1 <<  8)
#define TXDES_1ST_COL_CNT_MASK          (0x0f)
#define TXDES_1ST_COL_CNT_SHIFT         (3)
#define TXDES_1ST_DEFER_ERR             (1 <<  2)
#define TXDES_1ST_UNDERFLOW_ERR         (1 <<  1)
#define TXDES_1ST_DEFER                 (1 <<  0)

#define TXDES_2ST_INT_CTRL              (1 << 31)
#define TXDES_2ST_LAST_DESC             (1 << 30)
#define TXDES_2ST_FIR_DESC              (1 << 29)
#define TXDES_2ST_CRC_CTL               (1 << 26)
#define _TXDES_2ST_BUF_SIZE_MASK_       (0x7ff)
#define _TXDES_2ST_BUF_SIZE_SHIFT_      (0)
#define V3S_SET_TXDES_BUF_SIZE(size)    (((size) & _TXDES_2ST_BUF_SIZE_MASK_) << _TXDES_2ST_BUF_SIZE_SHIFT_)


/* Rx Descriptor */
#define RXDES_1ST_OWN                   (1 << 31)
#define RXDES_1ST_DAF_FAIL              (1 << 30)
#define RXDES_1ST_FRM_LEN_MASK          (0x3fff)
#define RXDES_1ST_FRM_LEN_SHIFT         (16)
#define RXDES_1ST_NO_ENOUGH_BUF_ERR     (1 << 14)
#define RXDES_1ST_SAF_FAIL              (1 << 13)
#define RXDES_1ST_OVERFLOW_ERR          (1 << 11)
#define RXDES_1ST_FIR_DESC              (1 << 9)
#define RXDES_1ST_LAST_DESC             (1 << 8)
#define RXDES_1ST_HEADER_ERR            (1 << 7)
#define RXDES_1ST_COL_ERR               (1 << 6)
#define RXDES_1ST_LENGTH_ERR            (1 << 4)
#define RXDES_1ST_PHY_ERR               (1 << 3)
#define RXDES_1ST_CRC_ERR               (1 << 1)
#define RXDES_1ST_PAYLOAD_ERR           (1 << 0)

#define RXDES_2ST_INT_CTL               (1 << 31)
# define _RXDES_2ST_BUF_SIZE_MASK_      (0x7ff)
# define _RXDES_2ST_BUF_SIZE_SHIFT_     (0)

#define V3S_SET_RXDES_BUF_SIZE(size)    (((size) & _RXDES_2ST_BUF_SIZE_MASK_) << _RXDES_2ST_BUF_SIZE_SHIFT_)


#define GET_RX_LEN_FROM_DESC(des1)      (((des1) >> RXDES_1ST_FRM_LEN_SHIFT) & RXDES_1ST_FRM_LEN_MASK)