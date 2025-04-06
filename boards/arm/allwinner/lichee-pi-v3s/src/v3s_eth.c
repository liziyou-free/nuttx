#include <nuttx/config.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <nuttx/irq.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>
#include <arpa/inet.h>
#include "arch/barriers.h"
#include <nuttx/crc32.h>
#include <nuttx/arch.h>
#include <nuttx/kmalloc.h>
#include <nuttx/wdog.h>
#include <nuttx/wqueue.h>
#include <nuttx/net/net.h>
#include <nuttx/net/ip.h>
#include <nuttx/net/mii.h>
#include <nuttx/net/netdev.h>
#include "arm_internal.h"
#include "v3s_emac.h"

#ifdef CONFIG_NET_PKT
#  include <nuttx/net/pkt.h>
#endif

#ifndef CONFIG_V3S_MAC_NUM
# define CONFIG_V3S_MAC_NUM             1
#endif

#ifndef CONFIG_V3S_MAC_TXDES_NUM
# define CONFIG_V3S_MAC_TXDES_NUM       16
#endif

#ifndef CONFIG_V3S_MAC_RXDES_NUM
# define CONFIG_V3S_MAC_RXDES_NUM       16
#endif

#if !defined(CONFIG_SCHED_WORKQUEUE)
#  error Work queue support is required in this configuration (CONFIG_SCHED_WORKQUEUE)
#endif

/* The low priority work queue is preferred.  If it is not enabled, LPWORK
 * will be the same as HPWORK.
 *
 * NOTE:  However, the network should NEVER run on the high priority work
 * queue!  That queue is intended only to service short back end interrupt
 * processing that never suspends.  Suspending the high priority work queue
 * may bring the system to its knees!
 */
#define V3S_MAC_MAWORK                  HPWORK

/* v3s default phy address */
#define _V3S_PHY_ADDR_                  (0x8)

/* v3s default mac address */
#define _V3S_DEFAULT_MAC_               {0x30, 0x9C, 0x23, 0xB4, 0x77, 0x45}

/* Packet buffer size */
#define PKTBUF_SIZE (MAX_NETDEV_PKTSIZE + CONFIG_NET_GUARDSIZE)

#define DISABLE_ALL_INT                 (0x0000)

#define ENABLE_IT_FLAGS                 (TX_INT_EN | TX_BUF_UA_INT | RX_INT_EN | RX_BUF_UA_INT_EN)

/* A single packet buffer is used */
static uint16_t g_pktbuf[CONFIG_V3S_MAC_NUM] [(PKTBUF_SIZE + 1) / 2];

/* RX/TX buffer alignment */
#define RX_BUF_SIZE   (1 << 11)
#define TX_BUF_SIZE   (1 << 11)

/* This is a helper pointer for accessing the contents of the Ethernet
 * header.
 */
 #define BUF ((FAR struct eth_hdr_s *)priv->net_dev.d_buf)

 #define V3S_MAC_TXTIMEOUT             (60*CLK_TCK)

 #define ETH_ZLEN                      60


/* V3S emac base address table */
const uint32_t v3s_mac_base_table[] = {
    V3S_GMAC_BASE,
};

/* V3S emac irq_no table */
const int v3s_mac_irq_table[] = {
    V3S_EMAC_IRQ,
};

/* V3S default mac address */
const uint8_t v3s_defualt_mac[][6] = {
  {0x30, 0x9C, 0x23, 0xB2, 0x88, 0x85},
};


/* Transmit descriptor, aligned to 16 bytes */
struct aligned_data(16) v3s_mac_txdes_s
{
    uint32_t txdes1;
    uint32_t txdes2;
    uint32_t txdes3;  /* TXBUF_BADR */
    uint32_t txdes4;  /* not used by HW */
};

/* Receive descriptor, aligned to 16 bytes */
struct aligned_data(16) v3s_mac_rxdes_s
{
    uint32_t rxdes1;
    uint32_t rxdes2;
    uint32_t rxdes3;  /* RXBUF_BADR */
    uint32_t rxdes4;  /* not used by HW */
};


struct v3s_mac_dev_s
{
    uint32_t iobase_addr;
    int32_t  irq_no;       // IRQ number
    int32_t phy_addr;     // MDIO PHY-DEV Addr
    
    /* EMAC prive data */
    struct v3s_mac_txdes_s txdes[CONFIG_V3S_MAC_TXDES_NUM];
    struct v3s_mac_rxdes_s rxdes[CONFIG_V3S_MAC_RXDES_NUM];
    int32_t rx_index;
    int32_t tx_index;
    int32_t tx_clean_pointer;
    int32_t tx_pending;

    /* NuttX net data */
    bool v3s_bifup;               /* true:ifup false:ifdown */
    struct wdog_s v3s_txtimeout;  /* TX timeout timer */
    uint32_t status;              /* Last ISR status */
    struct work_s v3s_irqwork;    /* For deferring work to the work queue */
    struct work_s v3s_pollwork;   /* For deferring work to the work queue */

    /* embeded nuttx net structure */
    struct net_driver_s net_dev;

}v3s_mac_priv[CONFIG_V3S_MAC_NUM];


static int v3s_mac_transmit(struct v3s_mac_dev_s *priv);
static void v3s_mac_receive(FAR struct v3s_mac_dev_s *priv);
static int v3s_descripter_init(FAR struct v3s_mac_dev_s *priv);


#undef putreg32
#undef getreg32

static void putreg32(uint32_t v, uint32_t a)
{
    arm_isb();
    *(volatile uint32_t *)(a) = (v);
    arm_isb();
    return;
}

static uint32_t getreg32(uint32_t a)
{
    uint32_t v;
    arm_isb();
    v = *(volatile uint32_t *)(a);
    arm_isb();
    return v;
}


/******************************************************************************
 * PHY  Driver
 *****************************************************************************/
static uint16_t v3s_phy_read(FAR struct v3s_mac_dev_s *priv, uint16_t reg)
{
    int32_t max_try;
    uint32_t reg_val = 0;

    reg_val |= (0x06 << 20);
    reg_val |= (((priv->phy_addr << 12) & (0x0001F000)) |
                    ((reg << 4) & (0x000007F0)) |
                    MII_BUSY);
    putreg32(reg_val, priv->iobase_addr + GETH_MDIO_ADDR);
    max_try = 0xffff;
    do {
        max_try--;
        reg_val = getreg32(priv->iobase_addr + GETH_MDIO_ADDR);
    } while ((reg_val & MII_BUSY) && max_try);
    return  getreg32(priv->iobase_addr + GETH_MDIO_DATA);
}


static void v3s_phy_write(FAR struct v3s_mac_dev_s *priv, 
                          uint8_t reg, uint16_t data)
{   uint32_t max_try;
    uint32_t reg_val = 0;

    reg_val |= (0x06 << 20); // 128 div
    reg_val |= (((priv->phy_addr << 12) & (0x0001F000)) | ((reg << 4) & 
               (0x000007F0)) | MII_WRITE | MII_BUSY);
    putreg32(data, priv->iobase_addr + GETH_MDIO_DATA);
    putreg32(reg_val, priv->iobase_addr + GETH_MDIO_ADDR);

    /* Until operation is complete and exiting */
    max_try = 0xfffff;
    do {
        reg_val = getreg32(priv->iobase_addr + GETH_MDIO_ADDR);
    } while ((reg_val & MII_BUSY) && --max_try);

    return;
}


static int mii_phy_init(FAR struct v3s_mac_dev_s *priv)
{
    volatile int reg_val;
    volatile int i = 0;
    uint16_t phy_val;

    for (i = 1; i < 32; i++) {
        priv->phy_addr = i;
        reg_val = (int)(v3s_phy_read(priv, MII_PHYID1) & 0xffff) << 16;
        reg_val |= (int)(v3s_phy_read(priv, MII_PHYID2) & 0xffff);
        if ((reg_val & 0x1fffffff) == 0x1fffffff)
        {
            continue;
        }
        if (i >= 32) {
            priv->phy_addr = -1;
            return -1;
        }
        break;
    }

    v3s_phy_write(priv, 0x1f, 0x013d);
    v3s_phy_write(priv, 0x10, 0x3ffe);
    v3s_phy_write(priv, 0x1f, 0x063d);
    v3s_phy_write(priv, 0x13, 0x8000);
    v3s_phy_write(priv, 0x1f, 0x023d);
    v3s_phy_write(priv, 0x18, 0x1000);
    v3s_phy_write(priv, 0x1f, 0x063d);
    v3s_phy_write(priv, 0x15, 0x132c);
    v3s_phy_write(priv, 0x1f, 0x013d);
    v3s_phy_write(priv, 0x13, 0xd602);
    v3s_phy_write(priv, 0x17, 0x003b);
    v3s_phy_write(priv, 0x1f, 0x063d);
    v3s_phy_write(priv, 0x14, 0x7088);
    v3s_phy_write(priv, 0x1f, 0x033d);
    v3s_phy_write(priv, 0x11, 0x8530);
    v3s_phy_write(priv, 0x1f, 0x003d);

    /* Reset phy chip */
    phy_val = v3s_phy_read(priv, MII_MCR);
    v3s_phy_write(priv, MII_MCR, phy_val | MII_MCR_RESET);
    do {
        reg_val = v3s_phy_read(priv, MII_MCR);
    } while (reg_val & MII_MCR_RESET);

    phy_val = v3s_phy_read(priv, MII_MCR);
    v3s_phy_write(priv, MII_MCR, (phy_val & ~MII_MCR_PDOWN));
    do {
        reg_val = v3s_phy_read(priv, MII_MCR);
    } while (reg_val & MII_MCR_PDOWN);

    /* Wait BMSR_ANEGCOMPLETE be set */
    i = 0;
    while(!(v3s_phy_read(priv, MII_MSR) & MII_MSR_ANEGCOMPLETE)) {
        if (i > 100) {
            printf("Warning: Auto negotiation timeout!\n");
        }
        do { unsigned long tt = 0xffffff; while (tt--); } while (0);
        i++;
    }

#ifdef DISABLE_AUTOENG
    phy_val = v3s_phy_read(dev, priv->phy_addr, MII_MCR);
    phy_val &= ~BMCR_ANENABLE;
    phy_val &= ~BMCR_SPEED1000;
    phy_val |= BMCR_SPEED100;
    phy_val |= BMCR_FULLDPLX;
    v3s_phy_write(dev, MII_BMCR, phy_val);
#endif

    phy_val = v3s_phy_read(priv, MII_MCR);
    reg_val = getreg32(priv->iobase_addr + GETH_BASIC_CTL0);
    if(phy_val & MII_MCR_FULLDPLX) {
        reg_val |= 0x01;
    } else {
        reg_val &= ~0x01;
    }
        
    /* Default is 100Mbps */
    reg_val &= ~0xc;
    if (phy_val & MII_MCR_SPEED100) {
        reg_val |= 0x0c;
    } else if (!(phy_val & MII_MCR_SPEED1000)) {
        reg_val |= 0x08;
    }
    putreg32(reg_val, priv->iobase_addr + GETH_BASIC_CTL0);

    return 0;
}

/******************************************************************************
 * MAC  Driver
 *****************************************************************************/
static int v3s_mac_early_init(void)
{
    uint32_t value;
    uint32_t reg_val;
    int phy_interface;

    phy_interface = PHY_INTERFACE_MODE_MII;

    /* Enable clk for ephy */
    value = getreg32(V3S_SYS_CTRL + 0x30);
    {
        reg_val = getreg32(V3S_CCMU_BASE + 0x0070);
        reg_val |= (1 << 0);
        putreg32(reg_val, V3S_CCMU_BASE + 0x0070);

        reg_val = getreg32(V3S_CCMU_BASE + 0x02c8);
        reg_val |= (1 << 2);
        putreg32(reg_val, V3S_CCMU_BASE + 0x02c8);

        value |= (1 << 15);
        value &= ~(1 << 16);
        value |= (3 << 17);

    }

    /* Set PHY clock, address, depend on phy mode */
    if (phy_interface == PHY_INTERFACE_MODE_RGMII) {
        value |= 0x00000004;
    }
    else {
        value &= ~0x00000004;
    }
    value &= ~0x00000003;
    value |= _V3S_PHY_ADDR_ << 20;

    if (phy_interface == PHY_INTERFACE_MODE_RGMII || \
        phy_interface == PHY_INTERFACE_MODE_GMII)
    {
        value |= 0x00000002;
    }

    putreg32(value, V3S_SYS_CTRL + 0x30);

    /* enalbe clk for gmac */
    reg_val = getreg32(V3S_CCMU_BASE + AHB1_GATING);
    reg_val |= GMAC_AHB_BIT;
    putreg32(reg_val, V3S_CCMU_BASE + AHB1_GATING);

    reg_val = getreg32(V3S_CCMU_BASE + 0x02C0);
    reg_val |= GMAC_AHB_BIT;
    putreg32(reg_val, V3S_CCMU_BASE + 0x02C0);

    return 0;
}


static int v3s_mac_txpoll(struct net_driver_s *dev)
{
    struct v3s_mac_dev_s *priv = \
            (struct v3s_mac_dev_s *)dev->d_private;

    /* Send the packet */
    v3s_mac_transmit(priv);

    /* If zero is returned, the polling will continue until all connections
    * have been examined.
    */
    return 0;
}


static void v3s_mac_txtimeout_work(FAR void *arg)
{
    FAR struct v3s_mac_dev_s *priv = (FAR struct v3s_mac_dev_s *)arg;

    /* Process pending Ethernet interrupts */
    net_lock();

    /* Then poll the network for new XMIT data */
    devif_poll(&priv->net_dev, v3s_mac_txpoll);
    net_unlock();

    /* Re-enable Ethernet interrupts */
    up_enable_irq(priv->irq_no);
}


static inline void v3s_mac_txtimeout_expiry(wdparm_t arg)
{
  FAR struct v3s_mac_dev_s *priv = (FAR struct v3s_mac_dev_s *)arg;

  /* Disable further Ethernet interrupts.  This will prevent some race
   * conditions with interrupt work.  There is still a potential race
   * condition with interrupt work that is already queued and in progress.
   */
  up_disable_irq(priv->irq_no);

  /* Schedule to perform the TX timeout processing on the worker thread. */
  work_queue(V3S_MAC_MAWORK, &priv->v3s_irqwork, v3s_mac_txtimeout_work, priv, 0);
}


static inline struct v3s_mac_txdes_s *
v3s_mac_current_clean_txdes(FAR struct v3s_mac_dev_s *priv)
{
  return &priv->txdes[priv->tx_clean_pointer];
}


static void v3s_mac_txdone(FAR struct v3s_mac_dev_s *priv)
{
    FAR struct v3s_mac_txdes_s *txdes;

    /* Check if a Tx was pending */
    while (priv->tx_pending) {
        txdes = v3s_mac_current_clean_txdes(priv);
        up_invalidate_dcache((uintptr_t)txdes, (uintptr_t)txdes + sizeof(*txdes));
        /* txdes owned by dma */
        if (txdes->txdes1 & TXDES_1ST_OWN) {
            break;
        }
        /* TODO: check for excessive and late collisions */
        /* txdes reset */
        txdes->txdes1 = 0;
        txdes->txdes2 = (1 << 24);
        txdes->txdes3 = 0;
        priv->tx_clean_pointer = (priv->tx_clean_pointer + 1) &
                                (CONFIG_V3S_MAC_TXDES_NUM - 1);
        priv->tx_pending--;
        up_clean_dcache((uintptr_t)txdes, (uintptr_t)txdes + sizeof(*txdes));
    }

    /* If no further xmits are pending, then cancel the TX timeout and
    * disable further Tx interrupts.
    */
    ninfo("txpending=%d\n", priv->tx_pending);

    /* Cancel the TX timeout */
    wd_cancel(&priv->v3s_txtimeout);

    /* Then poll the network for new XMIT data */
    devif_poll(&priv->net_dev, v3s_mac_txpoll);
}


static inline struct v3s_mac_txdes_s *
v3s_mac_current_txdes(struct v3s_mac_dev_s *priv)
{
  return &priv->txdes[priv->tx_index];
}


static int v3s_mac_transmit(struct v3s_mac_dev_s *priv)
{
    uint32_t reg;
    struct v3s_mac_txdes_s *txdes;

    int len = priv->net_dev.d_len;

    up_invalidate_dcache((uintptr_t)(priv->txdes), (uintptr_t)(priv->txdes) + sizeof(priv->txdes));

    txdes = v3s_mac_current_txdes(priv);
    arm_isb();
    
    /* Verify that the hardware is ready to send another packet.  If we get
    * here, then we are committed to sending a packet; Higher level logic
    * must have assured that there is no transmission in progress.
    */
    len = len < ETH_ZLEN ? ETH_ZLEN : len;
    
    up_clean_dcache((uint32_t)priv->net_dev.d_buf, (uint32_t)priv->net_dev.d_buf + len);

    /* Send the packet: address=priv->net_dev.d_buf, length=priv->net_dev.d_len */
    txdes->txdes3 = (uint32_t)priv->net_dev.d_buf;
    txdes->txdes2 = TXDES_2ST_INT_CTRL | TXDES_2ST_LAST_DESC | \
                    TXDES_2ST_FIR_DESC | V3S_SET_TXDES_BUF_SIZE(len) | (1 << 24);
    txdes->txdes1 = TXDES_1ST_OWN;
    arm_isb();

    priv->tx_index = (priv->tx_index + 1) & (CONFIG_V3S_MAC_TXDES_NUM - 1);
    priv->tx_pending++;
    
    up_clean_dcache((uintptr_t)txdes, (uintptr_t)txdes + sizeof(*txdes));

    /* Start TX */
    reg = getreg32(priv->iobase_addr + GETH_TX_CTL1);
    arm_isb();
    reg |= TX_DMA_START | TX_DMA_EN;
    putreg32(reg, priv->iobase_addr + GETH_TX_CTL1);
    arm_isb();

    /* Setup the TX timeout watchdog (perhaps restarting the timer) */
    wd_start(&priv->v3s_txtimeout, V3S_MAC_TXTIMEOUT,
            v3s_mac_txtimeout_expiry, (wdparm_t)priv);

    return OK;
}


static inline struct v3s_mac_rxdes_s *
v3s_mac_current_rxdes(FAR struct v3s_mac_dev_s *priv) {
    return &(priv->rxdes[priv->rx_index]);
}


static uint32_t rx_cnt = 0;
#define ERR_FLAGS (RXDES_1ST_PAYLOAD_ERR | RXDES_1ST_CRC_ERR | RXDES_1ST_PHY_ERR | \
    RXDES_1ST_LENGTH_ERR | RXDES_1ST_COL_ERR | RXDES_1ST_HEADER_ERR| \
    RXDES_1ST_OVERFLOW_ERR | RXDES_1ST_SAF_FAIL | RXDES_1ST_NO_ENOUGH_BUF_ERR)
    
static void v3s_mac_receive(FAR struct v3s_mac_dev_s *priv)
{
    volatile uint32_t loop;
    volatile uint32_t index_s;
    volatile uint32_t index_e;
    volatile uint32_t len;
    volatile uint8_t *data;
    volatile int32_t rx_descp_num;
    volatile struct v3s_mac_rxdes_s *rxdes;
    
    rx_descp_num = CONFIG_V3S_MAC_RXDES_NUM;
    index_s = 0;
    index_e = 0;
    index_s = priv->rx_index;
    arm_isb();
    while (rx_descp_num--) {
        rxdes = v3s_mac_current_rxdes(priv);
        arm_isb();
        up_invalidate_dcache((uintptr_t)rxdes, (uintptr_t)rxdes + sizeof(*rxdes));
        if (rxdes->rxdes1 & RXDES_1ST_OWN) {
            index_e = priv->rx_index;
            arm_isb();
            break;
        }
        len = GET_RX_LEN_FROM_DESC(rxdes->rxdes1);
        data = (uint8_t *)rxdes->rxdes3;
        arm_isb();
        up_invalidate_dcache((uintptr_t)data, (uintptr_t)data + len);

        /* Copy the data data from the hardware to priv->net_dev.d_buf.  Set
        * amount of data in priv->net_dev.d_len
        */
        memcpy(priv->net_dev.d_buf, data, len);
        priv->net_dev.d_len = len;

#ifdef CONFIG_NET_PKT
        pkt_input(&priv->net_dev);
#endif

#ifdef CONFIG_NET_IPv4
        if (BUF->type == HTONS(ETHTYPE_IP)) {
            /* Receive an IPv4 packet from the network device */
            ipv4_input(&priv->net_dev);
            
            if (priv->net_dev.d_len > 0) {
                /* And send the packet */
                v3s_mac_transmit(priv);
            }
        }
        else
#endif
#ifdef CONFIG_NET_IPv6
        if (BUF->type == HTONS(ETHTYPE_IP6)) {

            /* Give the IPv6 packet to the network layer */
            ipv6_input(&priv->net_dev);

            /* If the above function invocation resulted in data that should be
            * sent out on the network, the field  d_len will set to a value
            * > 0.
            */
            if (priv->net_dev.d_len > 0) {
                /* And send the packet */
                v3s_mac_transmit(priv);
            }
        }
        else
#endif
#ifdef CONFIG_NET_ARP
        if (BUF->type == HTONS(ETHTYPE_ARP)) {
            arp_input(&priv->net_dev);
            /* If the above function invocation resulted in data that should be
            * sent out on the network, the field  d_len will set to a value
            * > 0.
            */
            if (priv->net_dev.d_len > 0) {
                v3s_mac_transmit(priv);
            }
        }
#endif
        priv->rx_index = (priv->rx_index + 1) & (CONFIG_V3S_MAC_RXDES_NUM - 1);
        arm_isb();
    }

    loop = (index_s == index_e) ? CONFIG_V3S_MAC_RXDES_NUM : 
           (index_e > index_s) ? (index_e - index_s) : (CONFIG_V3S_MAC_RXDES_NUM - index_s + index_e);
    arm_isb();
    while (loop--) {
        rxdes = &priv->rxdes[index_s];
        rxdes->rxdes1 = RXDES_1ST_OWN;
        arm_isb();
        up_clean_dcache((uintptr_t)rxdes, (uintptr_t)rxdes + sizeof(rxdes));
        index_s = (index_s + 1) & (CONFIG_V3S_MAC_RXDES_NUM - 1);
        arm_isb();
    }
    
    return;
}


static void v3s_mac_interrupt_work(FAR void *arg)
{
    uint32_t status;
    struct v3s_mac_dev_s *priv;

    priv = (struct v3s_mac_dev_s *)arg;

    /* Process pending Ethernet interrupts */
    net_lock();

    status = priv->status;
    if (!status) {
        goto out;
    }

    /* RGMII Interrupt */
    // ...

    /* Handle interrupts according to status bit settings */
    if (status & (RX_INT | RX_BUF_UA_INT)) {
        v3s_mac_receive(priv);
    }

    if (status & TX_BUF_UA_INT) {

    }

    if (status & (TX_INT | TX_BUF_UA_INT)) {
        v3s_mac_txdone(priv);
    }

    if (status & RGMII_LINK_STA_INT)
    {
        printf("\r\n---> RGMII Link Status Change!");
    }
out:
    ninfo("ISR-done\n");
    net_unlock();

    putreg32((priv->status & 0x3FFF), priv->iobase_addr + GETH_INT_STA);
    putreg32(ENABLE_IT_FLAGS, priv->iobase_addr + GETH_INT_EN);
    /* Re-enable Ethernet interrupts */
    up_enable_irq(priv->irq_no);

    return;
}


static int v3s_mac_irq_handler(int irq, FAR void *context, FAR void *arg)
{
    struct v3s_mac_dev_s *priv = (struct v3s_mac_dev_s *)arg;

    up_disable_irq(priv->irq_no);

    /* Mask all interrupt */
    putreg32(DISABLE_ALL_INT, priv->iobase_addr + GETH_INT_EN);

    /* Recod status register */
    priv->status = getreg32(priv->iobase_addr + GETH_INT_STA);
    
    if (priv->status & (TX_INT | TX_EARLY_INT)) {
      /* If a TX transfer just completed, then cancel the TX timeout so
       * there will be do race condition between any subsequent timeout
       * expiration and the deferred interrupt processing.
       */
       wd_cancel(&priv->v3s_txtimeout);
    }

    /* Schedule to perform the interrupt processing on the worker thread. */
    work_queue(V3S_MAC_MAWORK, &priv->v3s_irqwork, v3s_mac_interrupt_work, priv, 0);

    return OK;
}


static int v3s_mac_reset(FAR struct v3s_mac_dev_s *priv)
{
    int try;
    long reg;
    /* Reset Emac */
    try = 0xfffffff;
    putreg32(0x01, priv->iobase_addr + GETH_BASIC_CTL1);
    do {
        reg = getreg32(priv->iobase_addr + GETH_BASIC_CTL1) & 0x01;
        --try;
    } while (reg && try > 0);
    return (try == 0x00);
}


static int v3s_mac_set_hwaddr(FAR struct v3s_mac_dev_s *priv, const char *mac_adddr)
{
    uint32_t mac;

    mac  = (mac_adddr[5] << 8) | mac_adddr[4];
    putreg32(mac, priv->iobase_addr + GETH_ADDR_HI(0));

    mac  = (mac_adddr[3] << 24) | (mac_adddr[2] << 16) | (mac_adddr[1] << 8) | mac_adddr[0];
    putreg32(mac, priv->iobase_addr + GETH_ADDR_LO(0));

    /* Read the MAC address from the hardware into
    * priv->net_dev.d_mac.ether.ether_addr_octet
    */
    memcpy(priv->net_dev.d_mac.ether.ether_addr_octet, (FAR void *)(mac_adddr), 6);
    return 0;
}


static int v3s_descripter_init(FAR struct v3s_mac_dev_s *priv)
{
    int i;
    unsigned char *rxdesp_kmem = NULL;
    struct v3s_mac_txdes_s *txdes = NULL;
    struct v3s_mac_rxdes_s *rxdes = NULL;

    /* Initialize data structure */
    priv->rx_index = 0;
    priv->tx_index = 0;
    priv->tx_clean_pointer = 0;
    priv->tx_pending = 0;
    txdes = &priv->txdes[0];
    rxdes = &priv->rxdes[0];

    rxdesp_kmem = kmm_memalign(128, CONFIG_V3S_MAC_RXDES_NUM * RX_BUF_SIZE);
    if (!rxdesp_kmem) {
        return -1;
    }
    memset(rxdesp_kmem, 0, CONFIG_V3S_MAC_RXDES_NUM * RX_BUF_SIZE);

    /* Initialize Rx descriptors */
    for (i = 0; i < CONFIG_V3S_MAC_RXDES_NUM; i++)
    {
      /* RXBUF_BADR */
      rxdes[i].rxdes1 = RXDES_1ST_OWN;
      rxdes[i].rxdes2 = V3S_SET_RXDES_BUF_SIZE(RX_BUF_SIZE - 1) | (1 << 24);
      rxdes[i].rxdes3 = (uint32_t)(rxdesp_kmem + i * RX_BUF_SIZE);
      rxdes[i].rxdes4 = (uint32_t)(rxdes + i + 1); /* Next ring */
    }
    rxdes[CONFIG_V3S_MAC_RXDES_NUM - 1].rxdes4 = (uint32_t)rxdes;

    up_invalidate_dcache((uintptr_t)rxdes, (uintptr_t)rxdes + sizeof(priv->rxdes));

    /* Initialize Tx descriptors */
    for (i = 0; i < CONFIG_V3S_MAC_TXDES_NUM; i++)
    {
      /* RXBUF_BADR */
      txdes[i].txdes1 = 0;
      txdes[i].txdes2 = (1 << 24);
      txdes[i].txdes3 = 0;
      txdes[i].txdes4 = (uint32_t)(txdes + i + 1);
    }
    txdes[CONFIG_V3S_MAC_TXDES_NUM - 1].txdes4 = (uint32_t)txdes;

    up_clean_dcache((uintptr_t)txdes, (uintptr_t)txdes + sizeof(priv->txdes));
    
    /* Config descripptionn to registers */
    putreg32((uint32_t)txdes, priv->iobase_addr + GETH_TX_DESC_LIST);
    putreg32((uint32_t)rxdes, priv->iobase_addr + GETH_RX_DESC_LIST);
    
    return 0;
}


static int v3s_mac_hal_init(FAR struct v3s_mac_dev_s *priv)
{
    int32_t ret;
    uint32_t reg_val;
    const char mac[] = _V3S_DEFAULT_MAC_;

    /* Put the interface in the down state.  This usually amounts to resetting
     * the device and/or calling ftmac100_ifdown().
      */
    ret = v3s_mac_reset(priv);
    if (ret) {
        ninfo("\nv3s emac reset fail!");
        return -1;
    }

    /* init phy */
    mii_phy_init(priv);

    /* Init Tx/Rx Descriptor */
    v3s_descripter_init(priv);

    /* Initialize core */
    // reg_val = getreg32(priv->iobase_addr + GETH_TX_CTL0);
    reg_val |= TX_EN | TX_JABBER_DISABLE;   /* Enable transmit component &  Jabber Disable */
    putreg32(reg_val, priv->iobase_addr + GETH_TX_CTL0);

    // reg_val = getreg32(priv->iobase_addr + GETH_TX_CTL1);
    /* Transmit COE type 2 cannot be done in cut-through mode. */
    reg_val = TX_DMA_EN; // | 0x02 | 0x04;
    putreg32(reg_val, priv->iobase_addr + GETH_TX_CTL1);

    reg_val = getreg32(priv->iobase_addr + GETH_RX_CTL0);
#ifdef CONFIG_HARD_CHECKSUM
    reg_val |= (1 << 27);                           /* Enable CRC & IPv4 Header Checksum */
#endif
    reg_val |= (0x0F << 28);                        /* Automatic Pad/CRC Stripping */
    putreg32(reg_val, priv->iobase_addr + GETH_RX_CTL0);

    reg_val = getreg32(priv->iobase_addr + GETH_RX_CTL1);
    reg_val |= (0x03 << 30) | 0x02;
    putreg32(reg_val, priv->iobase_addr + GETH_RX_CTL1);

    /* GMAC frame filter */
    reg_val = getreg32(priv->iobase_addr + GETH_RX_FRM_FLT);
    reg_val |= 0x00000001;
    putreg32(reg_val, priv->iobase_addr + GETH_RX_FRM_FLT);

    /* Burst should be 8 */
    reg_val = getreg32(priv->iobase_addr + GETH_BASIC_CTL1);
    reg_val |= (8 << 24);
    putreg32(reg_val, priv->iobase_addr + GETH_BASIC_CTL1);
    
    /* Set mac address */
    v3s_mac_set_hwaddr(priv, mac);

    /* Enable interrupt */
    reg_val = ENABLE_IT_FLAGS;
    putreg32(reg_val, priv->iobase_addr + GETH_INT_EN);

    return 0;
}

static int v3s_mac_ifup(struct net_driver_s *dev)
{
    struct v3s_mac_dev_s *priv = NULL;

    priv = (FAR struct v3s_mac_dev_s *)dev->d_private;
    v3s_mac_hal_init(priv);
    return 0;
}


static int v3s_mac_ifdown(struct net_driver_s *priv)
{
    return 0;
}


static int v3s_mac_txavail(struct net_driver_s *dev)
{
    return 0;
}


int v3s_mac_device_initialize(int index)
{
    int irq;
    struct v3s_mac_dev_s *priv;

    priv = &v3s_mac_priv[index];
    irq = v3s_mac_irq_table[index];

    up_enable_dcache();
    up_enable_icache();

    /* Initialize the driver structure */
    memset(priv, 0, sizeof(*priv));

    /* Attach the IRQ to the driver */
    if (irq_attach(irq, v3s_mac_irq_handler, (void *)priv))
    {
        priv->irq_no = -1;
        /* We could not attach the ISR to the interrupt */
        return -EAGAIN;
    } else {
        priv->irq_no = irq;
        /* Enable interrupt */
        up_enable_irq(irq);
    }

    priv->net_dev.d_buf     = (FAR uint8_t *)g_pktbuf[index]; /* Single packet buffer */
    priv->net_dev.d_ifup    = v3s_mac_ifup;                 /* I/F up (new IP address) callback */
    priv->net_dev.d_ifdown  = v3s_mac_ifdown;               /* I/F down callback */
    priv->net_dev.d_txavail = v3s_mac_txavail;              /* New TX data callback */
    #ifdef CONFIG_NET_MCASTGROUP
    priv->net_dev.d_addmac  = NULL;               /* Add multicast MAC address */
    priv->net_dev.d_rmmac   = NULL;                /* Remove multicast MAC address */
    #endif
    priv->net_dev.d_private = priv;                    /* Used to recover private state from dev */
    priv->iobase_addr       = v3s_mac_base_table[index];

    /* Read the MAC address from the hardware into
      * priv->net_dev.d_mac.ether.ether_addr_octet
      */
    memcpy(priv->net_dev.d_mac.ether.ether_addr_octet, (FAR void *)v3s_defualt_mac, 6);

    /* enable mac clk, and internal 100M-phy */
    v3s_mac_early_init();

    /* Register the device with the OS so that socket IOCTLs can be performed */
    netdev_register(&priv->net_dev, NET_LL_ETHERNET);
    return OK;
}


void arm_netinitialize(void)
{
  v3s_mac_device_initialize(0);
}