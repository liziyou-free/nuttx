#include <nuttx/config.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <nuttx/irq.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>
#include "arch/barriers.h"
#include <nuttx/arch.h>
#include <nuttx/nuttx.h>
#include <nuttx/kmalloc.h>
#include <nuttx/wqueue.h>
#include <nuttx/semaphore.h>
#include "arm_internal.h"
#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>
#include "v3s_sdmmc.h"
#include "v3s_gpio.h"



/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

 #define mmcdbg       // printf
 #define mmc_err      printf

 #define SDMMC_BLOCKSIZE        512
#define SDMMC_CMD_RESP_TIMEOUT  1000


 static const v3s_sdmmc_irq_table[] = {
    V3S_SDMMC0_IRQ,
    V3S_SDMMC1_IRQ,
    V3S_SDMMC2_IRQ
};

static const v3s_regbase_table[] = {
    V3S_MMC0_BASE,
    V3S_MMC1_BASE,
    V3S_MMC2_BASE
};

static const v3s_clkbase_table [] = {
    CCM_SDC0_SCLK_CTRL,
    CCM_SDC1_SCLK_CTRL,
    CCM_SDC2_SCLK_CTRL
};


 struct v3s_sdmmc_reg {
    volatile uint32_t gctrl;         /* (0x00) SMC Global Control Register */
    volatile uint32_t clkcr;         /* (0x04) SMC Clock Control Register */
    volatile uint32_t timeout;       /* (0x08) SMC Time Out Register */
    volatile uint32_t width;         /* (0x0C) SMC Bus Width Register */
    volatile uint32_t blksz;         /* (0x10) SMC Block Size Register */
    volatile uint32_t bytecnt;       /* (0x14) SMC Byte Count Register */
    volatile uint32_t cmd;           /* (0x18) SMC Command Register */
    volatile uint32_t arg;           /* (0x1C) SMC Argument Register */
    volatile uint32_t resp0;         /* (0x20) SMC Response Register 0 */
    volatile uint32_t resp1;         /* (0x24) SMC Response Register 1 */
    volatile uint32_t resp2;         /* (0x28) SMC Response Register 2 */
    volatile uint32_t resp3;         /* (0x2C) SMC Response Register 3 */
    volatile uint32_t simr;          /* (0x30) SMC Interrupt Mask Register */
    volatile uint32_t isr;           /* (0x34) SMC Masked Interrupt Status Register */
    volatile uint32_t risr;          /* (0x38) SMC Raw Interrupt Status Register */
    volatile uint32_t status;        /* (0x3C) SMC Status Register */
    volatile uint32_t ftrglevel;     /* (0x40) SMC FIFO Threshold Watermark Register */
    volatile uint32_t funcsel;       /* (0x44) SMC Function Select Register */
    volatile uint32_t reserve0[4];   /* 0x48 ~ 0x54 */
    volatile uint32_t a12a;          /* (0x58)Auto command 12 argument*/
    volatile uint32_t ntsr;          /* (0x5c)SMC2 Newtiming Set Register */
    volatile uint32_t sdbg;          /* (0x60) SMC Debug Register */
    volatile uint32_t reserve1[5];   /* (0x64 ~ 0x74) */
    volatile uint32_t hwrst;         /* (0x78) SMC eMMC Hardware Reset Register */
    volatile uint32_t reserve2[1];   /*  (0x7c) */
    volatile uint32_t dmac;          /*  (0x80) SMC IDMAC Control Register */
    volatile uint32_t dlba;          /*  (0x84) SMC IDMAC Descriptor List Base Address Register */
    volatile uint32_t idst;          /*  (0x88) SMC IDMAC Status Register */
    volatile uint32_t idie;          /*  (0x8C) SMC IDMAC Interrupt Enable Register */
    volatile uint32_t reserve3[28];  /*  (0x98~0xff) */
    volatile uint32_t thldc;         /*  (0x100) Card Threshold Control Register */
    volatile uint32_t res4[2];       /*  (0x104~0x10b) */
    volatile uint32_t dsbd;          /* (0x10c) eMMC4.5 DDR Start Bit Detection Control */
    volatile uint32_t respcrc;       /* (0x110) SD Response CRC*/
    volatile uint32_t data7crc;      /* (0x114) SD data7 CRC*/
    volatile uint32_t data6crc;      /* (0x118) SD data6 CRC*/
    volatile uint32_t data5crc;      /* (0x11c) SD data5 CRC*/
    volatile uint32_t data4crc;      /* (0x120) SD data4 CRC*/
    volatile uint32_t data3crc;      /* (0x124) SD data3 CRC*/
    volatile uint32_t data2crc;      /* (0x128) SD data2 CRC*/
    volatile uint32_t data1crc;      /* (0x12c) SD data1 CRC*/
    volatile uint32_t data0crc;      /* (0x130) SD data0 CRC*/
    volatile uint32_t crcsta;        /* (0x134) SD CRC status*/
    volatile uint32_t res5[50];      /* (0x138~0x1ff) */
    volatile uint32_t fifo;          /* (0x200) SMC FIFO Access Address */
};

struct v3s_sdmmc_des {
    uint32_t des0;
    uint32_t des1;
    uint32_t des2;
    uint32_t des3;
};

enum cardtype {
    CARD_UNKNOWN = 0,
    CARD_SD,
    CARD_MMC,
    CARD_EMMC
};


enum sdmmc_host_event {
    SDMMC_EVENT_NONE_RESPONSE    = 0x01,
    SDMMC_EVENT_SHORT_RESPONSE   = 0x02,
    SDMMC_EVENT_LONG_RESPONSE    = 0x04,
    SDMMC_EVENT_TRANS_READ       = 0x08,
    SDMMC_EVENT_TRANS_WRITE      = 0x10,
    SDMMC_EVENT_COMMAND_OK       = 0x20,
    SDMMC_EVENT_CARD_REMOVED     = 0x40,
    SDMMC_EVENT_CARD_INSERTED    = 0x80,
    SDMMC_EVENT_TRANS_ERR        = 0x100,
    SDMMC_EVENT_RESPONSE_TIMEOUT = 0x200,
};


struct v3s_sdmmc_dev{
    struct sdio_dev_s sdio;          /* SDIO device structure */
    struct v3s_sdmmc_reg *regs;      /* Register base address (Virtual Adress) */
    uint32_t irq;                    /* IRQ number */
    uint32_t sdmmc_no;               /* SDMMC number */
    uint32_t mclk;                   /* Currunt Clock (uint: Hz) */
    uint32_t hclkbase;               /* HCLK base address */
    uint32_t hclkrst;                
    uint32_t mclkbase;               /* Mclock base address */
    enum cardtype card_type;         /* 0: Sdcard  1: MMC  2: eMMC */
    
    uint32_t width;                  /* Bus width */
    uint32_t blocksize;              /* Block size */
    uint8_t *dma_buff;
    uint32_t response[4];            /* Response register */
    uint8_t *trans_buffer;           /* Buffer address */
    size_t trans_bytes;              /* Buffer length */

    uint32_t event;
    sem_t cmd_sem;
    sem_t trans_sem;

    struct work_s irqwork;           /* For deferring work to the work queue */
};




/****************************************************************************
 * Private Functions
 ****************************************************************************/

 static void delay(uint32_t t)
 {
    volatile uint32_t i = 0;
    t = t * 1000;
    for (i = 0; i < t; i++) {
        __asm__ __volatile__ ("nop");
    }
 }


static uintptr_t up_va_to_pa (uintptr_t va)
{
    return va;
}


 static void v3s_sdmmc0_pin_cfg()
 {
    /**\breif Init SDMMC0 Pin */
    set_gpio_multiplex(V3S_GPIOF, PIN(0), GPIOF0_MUX_SDC0D1);
    set_gpio_multiplex(V3S_GPIOF, PIN(1), GPIOF1_MUX_SDC0D0);
    set_gpio_multiplex(V3S_GPIOF, PIN(2), GPIOF2_MUX_SDC0CLK);
    set_gpio_multiplex(V3S_GPIOF, PIN(3), GPIOF3_MUX_SDC0CMD);
    set_gpio_multiplex(V3S_GPIOF, PIN(4), GPIOF4_MUX_SDC0D3);
    set_gpio_multiplex(V3S_GPIOF, PIN(5), GPIOF5_MUX_SDC0D2);
 }


 static int v3s_sdmmc_ccmu_int(struct v3s_sdmmc_dev * sdmmc)
{
	uint32_t rval;

	/* Enable AHB sdmmc clock */
	rval = getreg32(sdmmc->hclkbase);
	rval |= (1 << (8 + sdmmc->sdmmc_no));
	putreg32(rval, sdmmc->hclkbase);

    /* Software reset bus clock */
	rval = getreg32(sdmmc->hclkrst);
	rval |= (1 << (8 + sdmmc->sdmmc_no));
	putreg32(rval, sdmmc->hclkrst);

	/* config mod clock */
	putreg32(0x80000000, sdmmc->mclkbase);

	sdmmc->mclk = 24000000;
	return 0;
}


static int mmc_update_clk(struct v3s_sdmmc_dev *sdmmc)
{
	unsigned int cmd;
	int timeout = 0xfffff;

	cmd = (1U << 31) | (1 << 21) | (1 << 13);
  	putreg32(cmd, &sdmmc->regs->cmd);
	while((getreg32(&sdmmc->regs->cmd)&0x80000000) && timeout--);
	if (timeout<0){
		mmc_err("mmc %d update clk failed\n", sdmmc->sdmmc_no);
		return -1;
    }

	putreg32(getreg32(&sdmmc->regs->risr), &sdmmc->regs->risr);
	return 0;
}


static int v3s_sdmmc_config_clock(struct v3s_sdmmc_dev *sdmmc, uint32_t clk)
{
	unsigned rval = getreg32(&sdmmc->regs->clkcr);
//	unsigned int clkdiv = 0;

	/* Disable Clock */
	rval &= ~(1 << 16);
	putreg32(rval, &sdmmc->regs->clkcr);
	if(mmc_update_clk(sdmmc)){
		mmc_err("mmc %d disable clock failed\n", sdmmc->sdmmc_no);
		return -1;
	}

//	clkdiv = sdmmc->mclk/clk/2;
	//disable mclk first
	putreg32(0,sdmmc->mclkbase);
	if (clk <=400000) {
	    sdmmc->mclk = 400000;
	    putreg32(0x0002000f, sdmmc->mclkbase);
	} else {
	    sdmmc->mclk = 12000000;
	    putreg32(0x00000001, sdmmc->mclkbase);
	}
	//re-enable mclk
	putreg32(getreg32(sdmmc->mclkbase)|(1<<31), sdmmc->mclkbase);

	/*
	 * CLKCREG[7:0]: divider
	 * CLKCREG[16]:  on/off
	 * CLKCREG[17]:  power save
	 */
	/* Change Divider Factor */
	rval &= ~(0xFF);
	putreg32(rval, &sdmmc->regs->clkcr);
	if(mmc_update_clk(sdmmc)){
		mmc_err("mmc %d Change Divider Factor failed\n", sdmmc->sdmmc_no);
		return -1;
    }
	/* Re-enable Clock */
	rval |= (3 << 16);
	putreg32(rval, &sdmmc->regs->clkcr);
	if(mmc_update_clk(sdmmc)){
		mmc_err("mmc %d re-enable clock failed\n", sdmmc->sdmmc_no);
		return -1;
	}
	return 0;
}


 static void v3s_sdmmc_set_width(struct v3s_sdmmc_dev *sdmmc, uint32_t width)
 {
    /* Set bus width */
    sdmmc->width = width;

    /* Set bus width in register */
    if (width == 1) {
        /* Set 4-bit bus width */
        putreg32(0x00, &sdmmc->regs->width);
    } else if (width == 4){
        /* Set 1-bit bus width */
        putreg32(0x01, &sdmmc->regs->width);
    } else if (width == 8){
        /* Set 8-bit bus width */
        putreg32(0x02, &sdmmc->regs->width);
    } else {
        /* Invalid bus width */
        return;
    }
    return;
 }


 static int v3s_sdmmc_hardware_reset(struct v3s_sdmmc_dev *sdmmc)
{
    int32_t try;
    uint32_t reg;

	/* Reset controller */
	putreg32((SDMMC_SGCR_SOFT_RST | SDMMC_SGCR_FIFO_RST | SDMMC_SGCR_DMA_RST), &sdmmc->regs->gctrl);
    try = 0xfff;
    do {
        reg = getreg32(&sdmmc->regs->gctrl) & 0x7;
        if (--try <= 0x0) {
            mmc_err("mmc %d reset controller failed\n", sdmmc->sdmmc_no);
            break;
        }
    } while(reg & (SDMMC_SGCR_SOFT_RST | SDMMC_SGCR_FIFO_RST));

	/* release eMMC reset signal */
	putreg32(1, &sdmmc->regs->hwrst);
	putreg32(0, &sdmmc->regs->hwrst);
	delay(10);
	putreg32(1, &sdmmc->regs->hwrst);
    delay(10);

	return 0;
}


bool v3s_host_wait_busy (struct v3s_sdmmc_dev *sdmmc)
{
    uint32_t regval;
    int timeout = 0xfffff;

    do {
        regval = getreg32(&sdmmc->regs->status);
        if (--timeout <= 0) {
            mmcdbg("mmc %d wait busy failed\n", sdmmc->sdmmc_no);
            return true;
        }
    } while (regval & SDMMC_SSR_CARD_BUSY);

    return false;
}


void v3s_host_reset_fifo (struct v3s_sdmmc_dev *sdmmc)
{
    volatile uint32_t temp;
    
    temp = getreg32(&sdmmc->regs->gctrl) | SDMMC_SGCR_FIFO_RST;
    putreg32(temp, &sdmmc->regs->gctrl);
    
    do {
        temp = getreg32(&sdmmc->regs->gctrl);
    } while(temp & SDMMC_SGCR_FIFO_RST);

    return;
}


/*
 * @param:  dir: direction (0: read, 1: write)
 * @return: 0 on success, -1 on failure
 */

static int sdmmc_trans_data_by_poll(struct v3s_sdmmc_dev *sdmmc, uint8_t dir)
{
	unsigned i;
    uint32_t data;
	ssize_t nbytes;
	uint32_t *buff;
    volatile uint32_t temp;
	volatile int timeout = 0xffff;

    buff = (uint32_t *)sdmmc->trans_buffer;
    nbytes = sdmmc->trans_bytes;

	if (dir == 0) {
		for (i = 0; i < (nbytes >> 2); i++) {

            timeout = 0xffff;

			// while (--timeout && (getreg32(&sdmmc->regs->status)&(1 << 2)))
            // {
            //     up_udelay(10);
            // }

            up_udelay(10);

			if (timeout <= 0)
            {
                goto out;
            }
			buff[i] = getreg32(&sdmmc->regs->fifo);
			
		}
	}
    else
    {
		for (i = 0; i < (nbytes >> 2); i++) {

            timeout = 0xffff;

			// while (--timeout && (getreg32(&sdmmc->regs->status) & (1 << 3)))
            // {
            //     up_udelay(20);
            // }

            up_udelay(10);

			if (timeout <= 0)
            {
                goto out;
            }

			putreg32(buff[i], &sdmmc->regs->fifo);
		}

        // /* Wait the FIFO complete */
        // while( !( getreg32(&sdmmc->regs->status) & SDMMC_SSR_FIFO_RX_EMPTY) )
        // {
        // }
	}

out:
	if (timeout <= 0){
		mmc_err("mmc %d transfer by cpu failed\n", sdmmc->sdmmc_no);
		return -1;
	}

	return 0;
}


static void v3s_sdmmc_interrupt_work(FAR void *arg)
{
    int ret;
    uint32_t regval;
    struct v3s_sdmmc_dev *sdmmc;
    
    sdmmc = (struct v3s_sdmmc_dev *)arg;

    regval = getreg32(&sdmmc->regs->risr);
    mmcdbg("irq... (val:0x%x.)  \n", regval);

    if (regval & SDMMC_RISR_CARD_REMOVED) {
        mmcdbg("Card removed.\n");
    }
    if (regval & SDMMC_RISR_CARD_INSERTED) {
        mmcdbg("Card inserted.\n");
    }
    if (regval & SDMMC_RISR_RESP_TIMEOUT) {
        mmcdbg("Resp timeout.\n");
        sdmmc->event |= SDMMC_EVENT_RESPONSE_TIMEOUT;
        /* Realese semp */
        nxsem_post(&sdmmc->cmd_sem);
    }
    if (regval & SDMMC_RISR_TRANS_REQ) {
        mmcdbg("Tx Resq.\n");
        ret = sdmmc_trans_data_by_poll(sdmmc, 1);
        if (ret != 0) {
            mmcdbg("Transfer data to card failed.\n");
        }
    }
    if (regval & SDMMC_RISR_RECV_REQ) {
        mmcdbg("Rx Resq.\n");
        ret = sdmmc_trans_data_by_poll(sdmmc, 0);
        if (ret != 0) {
            mmcdbg("Read data to host failed.\n");
        }
    }
    if (regval & (SDMMC_RISR_AUTO_CMD_OK | SDMMC_RISR_COMMAND_OK)) {
        mmcdbg("Command Complete.\n");
        sdmmc->event |= SDMMC_EVENT_COMMAND_OK;
        /* Realese semp */
        nxsem_post(&sdmmc->cmd_sem);
    }
    if (regval & SDMMC_RISR_RESP_ERR) {
        mmcdbg("!!! Resp no resp / resp err .\n");
    }
    if (regval & (SDMMC_RISR_TRANS_OK)) {
        mmcdbg("Transfer complete...\n");
        /* Realese semp */
        nxsem_post(&sdmmc->trans_sem);
    }

    /* Clear the interrupt */
    putreg32(regval, &sdmmc->regs->risr);

    /* Unmask IRQ */
    regval = getreg32(&sdmmc->regs->gctrl);
    regval |= SDMMC_SGCR_INT_ENB;
    putreg32(regval, &sdmmc->regs->gctrl);
    
    up_enable_irq(sdmmc->irq);

    return;
}


static void v3s_sdmmc_irq_handler (int irq, FAR void *context, FAR void *arg)
{
    uint32_t regval;
    struct v3s_sdmmc_dev *sdmmc;

    sdmmc = (struct v3s_sdmmc_dev *)arg;
    if (sdmmc == NULL) {
        return;
    }

    up_disable_irq(sdmmc->irq);

    /* Mask IRQ */
    regval = getreg32(&sdmmc->regs->gctrl);
    regval &= (~SDMMC_SGCR_INT_ENB);
    putreg32(regval, &sdmmc->regs->gctrl);

    /* Schedule to perform the interrupt processing on the worker thread. */
    work_queue(HPWORK, &sdmmc->irqwork, v3s_sdmmc_interrupt_work, sdmmc, 0);

    return;
}


 static struct v3s_sdmmc_dev * v3s_sdmmc_probe (uint32_t sdmmc_no)
 {
    int ret;
    uint32_t regval;
    struct v3s_sdmmc_dev *sdmmc;

    sdmmc = (struct v3s_sdmmc_dev *)kmm_malloc(sizeof(struct v3s_sdmmc_dev));
    if (!sdmmc) {
        return NULL;
    }
    memset(sdmmc, 0, sizeof(struct v3s_sdmmc_dev));

    /* Parameter Init */
    sdmmc->sdmmc_no = sdmmc_no;
    sdmmc->width = 1;
    sdmmc->blocksize = SDMMC_BLOCKSIZE;
    sdmmc->irq = v3s_sdmmc_irq_table[sdmmc_no];
    sdmmc->regs = (struct v3s_sdmmc_reg *)up_va_to_pa( v3s_regbase_table[sdmmc_no] );
    sdmmc->hclkrst = up_va_to_pa(CCM_AHB1_RST_REG0);
    sdmmc->hclkbase = up_va_to_pa(CCM_AHB1_GATE0_CTRL);
    sdmmc->mclkbase = up_va_to_pa( v3s_clkbase_table[sdmmc_no] );

    ret = nxsem_init(&sdmmc->cmd_sem, 0, 0);
    {
        if (ret < 0) {
            mmcdbg("SDMMC: Failed to initialize semaphore\n");
            kmm_free(sdmmc);
            return NULL;
        }
    }

    ret = nxsem_init(&sdmmc->trans_sem, 0, 0);
    {
        if (ret < 0) {
            mmcdbg("SDMMC: Failed to initialize semaphore\n");
            kmm_free(sdmmc);
            return NULL;
        }
    }

    /* Init SDMMC0 Pin */
    mmcdbg("\nInit Sdmmc Pin...");
    v3s_sdmmc0_pin_cfg();
        
    /* Set sdmmc clock by CCMU */
    v3s_sdmmc_ccmu_int(sdmmc);

    /* Reset SDMMC hardware */
    v3s_sdmmc_hardware_reset(sdmmc);

    v3s_sdmmc_config_clock(sdmmc, 400000);  // 400KHz

    /* Attach the IRQ to the driver */
    if (irq_attach(sdmmc->irq, v3s_sdmmc_irq_handler, (void *)sdmmc))
    {
        mmcdbg("enable irq...(irq=%d)\n", sdmmc->irq);
        /* We could not attach the ISR to the interrupt */
        sem_destroy(&sdmmc->cmd_sem);
        kmm_free(sdmmc);
        up_disable_irq(sdmmc->irq);
        return NULL;
    } else {
        mmcdbg("enable irq...(irq=%d)\n", sdmmc->irq);
        up_enable_irq(sdmmc->irq);
    }

    v3s_sdmmc_set_width(sdmmc, 1);

    /* Unmask IRQ */
    regval = getreg32(&sdmmc->regs->gctrl);
    regval |= (SDMMC_SGCR_INT_ENB | SDMMC_SGCR_DBC_ENB);
    putreg32(regval, &sdmmc->regs->gctrl);

    uint32_t imask = SDMMC_SIMR_RESP_ERR | SDMMC_SIMR_COMMAND_OK | SDMMC_SIMR_TRANS_OK | \
                     SDMMC_SIMR_TRANS_REQ | SDMMC_SIMR_RECV_REQ | SDMMC_SIMR_DATA_CRC_ERR | \
                     SDMMC_SIMR_RESP_TIMEOUT | SDMMC_SIMR_DATA_TIMEOUT | SDMMC_SIMR_SWITCH_OK | \
                     SDMMC_SIMR_FIFO_OVERFLOW | SDMMC_SIMR_CMD_BUSY | SDMMC_SIMR_START_ERR | \
                     SDMMC_SIMR_AUTO_CMD_OK | SDMMC_SIMR_END_ERR | SDMMC_SIMR_CARD_INSERTED |\
                     SDMMC_SIMR_CARD_REMOVED | SDMMC_RISR_DATA_CRC_ERR;

    putreg32(imask, &sdmmc->regs->simr);
    
    return sdmmc;
 }






 /******************************************************************
  * Adapter to Nuttx
  ******************************************************************/

static sdio_capset_t v3s_host_capabilities (FAR struct sdio_dev_s *dev)
{
    sdio_capset_t caps = 0;

    /* Set the capabilities */
    caps |= SDIO_CAPS_4BIT_ONLY;
    caps |= SDIO_CAPS_DMABEFOREWRITE;

    return caps;
}


static void v3s_host_reset (FAR struct sdio_dev_s *dev)
{
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return;
    }

    /* Reset the host controller */
    v3s_sdmmc_hardware_reset(sdmmc_host);

    return;
}


static void v3s_host_set_bus_width (FAR struct sdio_dev_s *dev, bool enable)
{
    int ret;
    int bus_width;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return;
    }

    bus_width = enable ? 4 : 1;
    v3s_sdmmc_set_width(sdmmc_host, bus_width);

    return;
}


static void v3s_host_set_clock (FAR struct sdio_dev_s *dev, enum sdio_clock_e rate)
{
    uint32_t clock;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return;
    }

    switch (rate) {
        case CLOCK_SD_TRANSFER_1BIT:
            clock = 25000000;
            break;
        case CLOCK_SD_TRANSFER_4BIT:
            clock = 50000000;
            break;
        case CLOCK_MMC_TRANSFER:
            clock = 100000000;
            break;
        case CLOCK_IDMODE:  
        default:
            clock = 400000;
            break;
    }
    v3s_sdmmc_config_clock(sdmmc_host, clock);
    return;
}


static int v3s_host_send_cmd (FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t arg)
{
    uint32_t regval = 0;
    uint32_t cmdval = 0;
    uint32_t cmdidx = 0;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }

    sdmmc_host->event = 0;
    nxsem_reset(&sdmmc_host->cmd_sem, 0);
    nxsem_reset(&sdmmc_host->trans_sem, 0);

    bool  busy = v3s_host_wait_busy(sdmmc_host);
    if (busy) {
        mmcdbg("mmc %d wait busy failed\n", sdmmc_host->sdmmc_no);
        return -ETIMEDOUT;
    }

    /* Set CPSMEN and the command index */
    cmdidx  = (cmd & MMCSD_CMDIDX_MASK) >> MMCSD_CMDIDX_SHIFT;
    mmcdbg("send_cmd(cmdidx:%d)...\n", cmdidx);
    switch (cmdidx) {
        case MMCSD_CMDIDX0:
            cmdval |= SDMMC_SEND_INIT_SEQ;
            break;
        case SD_CMDIDX11:
            if (sdmmc_host->card_type == CARD_SD) {
                cmdval |= SDMMC_VOL_SW;
            }
        default:
            break;
    }

    switch (cmd & MMCSD_RESPONSE_MASK) {

        case MMCSD_NO_RESPONSE:
            cmdval &= (~SDMMC_RESP_RCV);
            break;

        case MMCSD_R1_RESPONSE:
        case MMCSD_R1B_RESPONSE:
        case MMCSD_R3_RESPONSE:
        case MMCSD_R4_RESPONSE:
        case MMCSD_R5_RESPONSE:
        case MMCSD_R6_RESPONSE:
        case MMCSD_R7_RESPONSE:
            cmdval |= SDMMC_RESP_RCV;
            cmdval &= ~(SDMMC_LONG_RESP);
            break;

        case MMCSD_R2_RESPONSE:
            cmdval |= SDMMC_RESP_RCV;
            cmdval |= SDMMC_LONG_RESP;
        break;
    }

    switch (cmd & MMCSD_DATAXFR_MASK)
    {
        case MMCSD_RDDATAXFR:
            cmdval |= (SDMMC_DATA_TRANS | SDMMC_WAIT_PRE_OVER);
            break;
        case MMCSD_WRDATAXFR:
            cmdval |= (SDMMC_DATA_TRANS | SDMMC_TRANS_DIR | SDMMC_WAIT_PRE_OVER);
            break;
        default:
            break;
    }

    /* POLL or DMA */
    regval = getreg32(&sdmmc_host->regs->gctrl);
#ifdef CONFIG_SDIO_DMA
    regval &= ~SDMMC_SGCR_AC_MODE;
#else
    regval |= SDMMC_SGCR_AC_MODE;
#endif
    putreg32(regval, &sdmmc_host->regs->gctrl);
    cmdval |= (cmdidx | SDMMC_WAIT_PRE_OVER | SDMMC_CMD_LOAD);
    putreg32(arg, &sdmmc_host->regs->arg);
    putreg32(cmdval, &sdmmc_host->regs->cmd);

    return 0;
}


static int v3s_host_wait_response (struct sdio_dev_s *dev, uint32_t cmd)
{
    int ret;
    uint32_t event;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }

    ret = nxsem_tickwait_uninterruptible(&sdmmc_host->cmd_sem, SDMMC_CMD_RESP_TIMEOUT);
    if (ret < 0) {
        mmcdbg("mmc %d wait response failed\n", sdmmc_host->sdmmc_no);
        return -ETIMEDOUT;
    }

    return ret;
}


static int v3s_mmc_host_recv_response (struct sdio_dev_s *dev, uint32_t cmd, uint32_t *resp)
{
    int ret = -1;
    uint32_t event;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }
    
    event = sdmmc_host->event;

    switch (cmd & MMCSD_RESPONSE_MASK) {

        case MMCSD_NO_RESPONSE:
        {
            if (event & SDMMC_EVENT_RESPONSE_TIMEOUT) {
                ret = 0;
            }
            ret = -1;
        }
        break;
        case MMCSD_R1_RESPONSE:
        case MMCSD_R1B_RESPONSE:
        case MMCSD_R3_RESPONSE:
        case MMCSD_R4_RESPONSE:
        case MMCSD_R5_RESPONSE:
        case MMCSD_R6_RESPONSE:
        case MMCSD_R7_RESPONSE:
        {
            if (event & SDMMC_EVENT_RESPONSE_TIMEOUT) {
                ret = (-ETIMEDOUT);
            }
            if (event & SDMMC_EVENT_COMMAND_OK) {
                *resp = getreg32(&sdmmc_host->regs->resp0);
                ret = 0;
            }
            else {
                ret = (-ETIMEDOUT);
            }
        }
        break;

        case MMCSD_R2_RESPONSE:
        {
            if (event & SDMMC_EVENT_COMMAND_OK) {
                *(resp + 0) = getreg32(&sdmmc_host->regs->resp3);
                *(resp + 1) = getreg32(&sdmmc_host->regs->resp2);
                *(resp + 2) = getreg32(&sdmmc_host->regs->resp1);
                *(resp + 3) = getreg32(&sdmmc_host->regs->resp0);
                ret = 0;
            }
            else {
                ret = -1;
            }
        }
        break;
    }

    up_udelay(500);

    return ret;
}


static void v3s_mmc_host_block_setup(struct sdio_dev_s *dev, unsigned int blocklen, unsigned int nblocks)
{
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }

    /* Set the block size (default: 512 bytes) */

    putreg32(blocklen, &sdmmc_host->regs->blksz);
    // putreg32(512, &sdmmc_host->regs->blksz);
 
    /* Set the byte count */
    putreg32(nblocks * blocklen, &sdmmc_host->regs->bytecnt);

    return;
}


static int v3s_mmc_host_send_setup (struct sdio_dev_s *dev, const uint8_t *buffer, size_t nbytes)
{
    uint32_t data;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }

    v3s_host_reset_fifo(sdmmc_host);

    sdmmc_host->trans_buffer = (uint8_t *)buffer;
    sdmmc_host->trans_bytes  = nbytes;
    return 0;
}


static int v3s_mmc_host_recv_setup (struct sdio_dev_s *dev, const uint8_t *buffer, size_t nbytes)
{
    volatile uint32_t data;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }

    v3s_host_reset_fifo(sdmmc_host);

    sdmmc_host->trans_buffer = (uint8_t *)buffer;
    sdmmc_host->trans_bytes  = nbytes;
    return 0;
}


int v3s_mmc_host_attach (FAR struct sdio_dev_s *dev)
{
    (void)dev;
    return 0;
}

static int v3s_mmc_host_registercallback(struct sdio_dev_s *dev,
    worker_t callback, void *arg)
{
    (void)dev;
    return 0;
}


static void v3s_mmc_host_callbackenable(struct sdio_dev_s *dev,
    sdio_eventset_t eventset)
{
    (void)dev;
    return 0;
}


static void v3s_mmc_host_waitenable(struct sdio_dev_s *dev,
    sdio_eventset_t eventset, uint32_t timeout)
{
    (void)dev;
    return 0;
}


static sdio_eventset_t v3s_mmc_host_eventwait(struct sdio_dev_s *dev)
{
    int ret;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }

    ret = nxsem_tickwait_uninterruptible(&sdmmc_host->trans_sem, SDMMC_CMD_RESP_TIMEOUT);
    if (ret < 0) {
        mmcdbg("mmc %d wait data transfer failed\n", sdmmc_host->sdmmc_no);
        return -ETIMEDOUT;
    }

    return 0;
}


static sdio_statset_t v3s_mmc_host_status(struct sdio_dev_s *dev)
{
    (void)dev;

    return SDIO_STATUS_PRESENT;
}


/*
 * * Name: v3s_sdinitialize
 * * param: minor - SDIO device number [0, 2]
 */
int v3s_sdinitialize(int minor)
{
    int ret;
    struct v3s_sdmmc_dev *dev;
    struct sdio_dev_s *sdio;

    dev = v3s_sdmmc_probe(minor);
    if (dev == NULL)
    {
        mmcdbg("v3s_sdmmc_probe failed\n");
        return -1;
    }

    sdio = &dev->sdio;
    sdio->status = v3s_mmc_host_status;
    sdio->reset = v3s_host_reset;
    sdio->widebus = v3s_host_set_bus_width;
    sdio->capabilities = v3s_host_capabilities;
    sdio->clock = v3s_host_set_clock;
    sdio->sendcmd = v3s_host_send_cmd;
    sdio->waitresponse = v3s_host_wait_response;
    sdio->recv_r1 = v3s_mmc_host_recv_response;
    sdio->recv_r2 = v3s_mmc_host_recv_response;
    sdio->recv_r3 = v3s_mmc_host_recv_response;
    sdio->recv_r4 = v3s_mmc_host_recv_response;
    sdio->recv_r5 = v3s_mmc_host_recv_response;
    sdio->recv_r6 = v3s_mmc_host_recv_response;
    sdio->recv_r7 = v3s_mmc_host_recv_response;
    sdio->sendsetup = v3s_mmc_host_send_setup;
    sdio->recvsetup = v3s_mmc_host_recv_setup;
#ifdef CONFIG_SDIO_DMA
#error "DMA not supported"
    .dmapreflight = at32_dmapreflight,
    .dmarecvsetup = at32_dmarecvsetup,
    .dmasendsetup  = at32_dmasendsetup,
#endif
    sdio->cancel = v3s_mmc_host_attach;
    sdio->attach = v3s_mmc_host_attach;
#ifdef CONFIG_SDIO_BLOCKSETUP
    sdio->blocksetup = v3s_mmc_host_block_setup;
#endif
    sdio->waitenable = v3s_mmc_host_waitenable;
    sdio->eventwait = v3s_mmc_host_eventwait;
    sdio->callbackenable = v3s_mmc_host_callbackenable;
    sdio->registercallback = v3s_mmc_host_registercallback;

    mmcsd_slotinitialize(0, sdio);

    return 0;
 }