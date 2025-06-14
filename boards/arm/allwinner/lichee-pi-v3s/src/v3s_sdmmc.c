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

#if  0
 # define mmcdbg       printf
 # define mmc_err      printf
#else 
 # define mmcdbg(fmt...)  do{}while(0)
 # define mmc_err(fmt...)  do{}while(0)
#endif


#define SDMMC_CMD_RESP_TIMEOUT        250
#define SDMMC_BLOCKSIZE               512
#define SDMMC_SINGLE_TRANS_BLOCKS     ((65535 & (~(SDMMC_BLOCKSIZE - 1))) / SDMMC_BLOCKSIZE)
#define DEFUALT_BUFFER_SIZE_PER_DESP         (4 * 1024) // 4KB

#define SDMMC_ACCESS_TYPE_DMA         1
#define SDMMC_ACCESS_TYPE_POLL        0

#if (CONFIG_MMCSD_MULTIBLOCK_LIMIT == 0) || (CONFIG_MMCSD_MULTIBLOCK_LIMIT > SDMMC_SINGLE_TRANS_BLOCKS)
# error "Exceeds the SDMMC-Host's single transmission capacity!"
#endif


void v3s_udelay(unsigned int usec);


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


struct v3s_sdmmc_idma_desp
{
    uint32_t des0;
    uint32_t des1;
    uint32_t des2;
    uint32_t des3;
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
    
    uint8_t access_type;             /* Access type: 0: Polling, 1: DMA */
    uint32_t width;                  /* Bus width */
    uint32_t blocksize;              /* Block size */
    uint32_t response[4];            /* Response register */
    uint8_t *trans_buffer;           /* Buffer address */
    size_t trans_bytes;              /* Buffer length */

    uint32_t desp_num;                /* Descriptor number */
    struct v3s_sdmmc_idma_desp *desp;

    uint32_t event;
    sem_t cmd_sem;
    sem_t trans_sem;
    struct work_s irqwork;           /* For deferring work to the work queue */
};




/****************************************************************************
 * Private Functions
 ****************************************************************************/

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
	    putreg32(0x00000000, sdmmc->mclkbase);
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
	up_udelay(100);
	putreg32(1, &sdmmc->regs->hwrst);
    up_udelay(100);

	return 0;
}


bool v3s_host_wait_busy (struct v3s_sdmmc_dev *sdmmc)
{
    uint32_t regval;
    int timeout = 100000;

    do {
        regval = getreg32(&sdmmc->regs->status);
        v3s_udelay(5);
        if (--timeout <= 0) {
            mmc_err("mmc %d wait busy failed\n", sdmmc->sdmmc_no);
            return true;
        }
    } while (regval & SDMMC_SSR_CARD_BUSY);

    return false;
}


void v3s_host_reset_fifo (struct v3s_sdmmc_dev *sdmmc)
{
    volatile uint32_t reg;
    volatile uint32_t temp;
    
    temp = SDMMC_SGCR_FIFO_RST | SDMMC_SGCR_DMA_RST;

    reg = getreg32(&sdmmc->regs->gctrl)  | temp;
    putreg32(reg, &sdmmc->regs->gctrl);
    
    do {
        reg = getreg32(&sdmmc->regs->gctrl);
    } while(reg & temp);
    mmc_update_clk(sdmmc);
    mmcdbg("mmc:%d reset fifo done\n", sdmmc->sdmmc_no);
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
            timeout = 0xfff;
			while (--timeout && (getreg32(&sdmmc->regs->status)&(1 << 2)))
            {
                v3s_udelay(1);
            }

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
            timeout = 0xfff;
			while (--timeout && (getreg32(&sdmmc->regs->status) & (1 << 3)))
            {
                v3s_udelay(1);
            }

			if (timeout <= 0)
            {
                goto out;
            }

			putreg32(buff[i], &sdmmc->regs->fifo);
		}
	}

out:
	if (timeout <= 0){
		// mmc_err("mmc %d transfer by cpu failed (i==%d)\n", sdmmc->sdmmc_no, i);
		return -1;
	}

	return 0;
}


static int v3s_sdmmc_idma_desp_init (struct v3s_sdmmc_dev *sdmmc)
{
    struct v3s_sdmmc_idma_desp *desp;
    uint32_t max_bytes_per_trans;
    uint32_t max_desp_num;
    uint32_t remainder;
    uint8_t (*buffer)[DEFUALT_BUFFER_SIZE_PER_DESP];
    
    max_bytes_per_trans = SDMMC_SINGLE_TRANS_BLOCKS * SDMMC_BLOCKSIZE;
    max_desp_num = max_bytes_per_trans / DEFUALT_BUFFER_SIZE_PER_DESP;
    remainder = max_bytes_per_trans % DEFUALT_BUFFER_SIZE_PER_DESP;
    max_desp_num += remainder ? 1 : 0;
    
    desp = (struct v3s_sdmmc_idma_desp *) kmm_memalign(4, max_desp_num * sizeof(*desp));
    if (desp == NULL) {
        mmc_err("Failed to allocate memory for DMA descriptor\n");
        return -1;
    }

    memset(desp, 0, max_desp_num * sizeof(*desp));
    sdmmc->desp_num = max_desp_num;
    sdmmc->desp = desp;

    return 0;
}


void v3s_idma_dummp_desp (struct v3s_sdmmc_dev *sdmmc)
{
    struct v3s_sdmmc_idma_desp *desp;
    uint32_t desp_num;

    desp = sdmmc->desp;
    desp_num = sdmmc->desp_num;

    mmcdbg("v3s_idma_dummp_desp: desp_num:%d\n", desp_num);
    for (int j = 0; j < desp_num; j++) {
        mmcdbg("desp[%d]: 0x%x 0x%x 0x%x 0x%x\n", j, desp[j].des0, desp[j].des1, desp[j].des2, desp[j].des3);
    }

    return;
}


static int v3s_sdmmc_setup_idma (struct v3s_sdmmc_dev *sdmmc, void *buffer, size_t nbytes)
{
    uint32_t desp_num;
    uint32_t remainder;
    uint8_t (*pointer)[DEFUALT_BUFFER_SIZE_PER_DESP];
    struct v3s_sdmmc_idma_desp *desp;

    if ((buffer == NULL) || ((uintptr_t)buffer % 4) || (nbytes == 0)) {
        mmc_err("Invalid buffer or size\n");
        return -1;
    }

    sdmmc->access_type = SDMMC_ACCESS_TYPE_DMA;

    pointer = buffer;
    desp = sdmmc->desp;
    desp_num = nbytes / DEFUALT_BUFFER_SIZE_PER_DESP;
    remainder = nbytes % DEFUALT_BUFFER_SIZE_PER_DESP;
    desp_num += remainder ? 1 : 0;

    mmcdbg("v3s_sdmmc_setup_idma(desp_num:%d nbyts:%d buffer:0x%x)...\n", desp_num, nbytes, buffer);

    for (int j = 0; j < desp_num; j++) {
        desp[j].des0 = (V3S_IDMA_DESP0_OWN | V3S_IDMA_DESP0_CHAIN | V3S_IDMA_DESP0_INT_DIS);
        desp[j].des1 = DEFUALT_BUFFER_SIZE_PER_DESP;
        desp[j].des2 = &pointer[j];
        desp[j].des3 = &desp[j + 1];
    }
    desp[0].des0 |= V3S_IDMA_DESP0_FIRST_FLAG;

    if (remainder > 0) {
        desp[desp_num - 1].des1 = remainder;
    }
    desp[desp_num - 1].des0 &= (~V3S_IDMA_DESP0_INT_DIS);
    desp[desp_num - 1].des0 |= V3S_IDMA_DESP0_LAST_FLAG;

    UP_DMB();

    /* Flush the cache */
    up_flush_dcache((uintptr_t)buffer, ((uintptr_t)buffer) + nbytes);
    up_flush_dcache((uintptr_t)desp, (uintptr_t) (desp + desp_num));

    /* Dma enable */
    uint32_t rval = getreg32(&sdmmc->regs->gctrl);
	putreg32(rval | SDMMC_SGCR_DMA_ENB | SDMMC_SGCR_DMA_RST, &sdmmc->regs->gctrl);

    __asm volatile  ("nop; nop; nop;":::"memory");

    putreg32((uint32_t)desp, &sdmmc->regs->dlba);

    /* Reset IDMA */
    putreg32(V3S_IDMA_CTRL_RST, &sdmmc->regs->dmac);

    putreg32(V3S_IDMA_CTRL_ENB | V3S_IDMA_CTRL_BUST, &sdmmc->regs->dmac);

    putreg32((2U << 28) | (7 << 16) | 8, &sdmmc->regs->ftrglevel);

    /* Clear IDMA status */
    putreg32(0x03ff, &sdmmc->regs->idst);

    // v3s_idma_dummp_desp(sdmmc);

    return 0;
}


static v3s_sdmmc_free_desp (struct v3s_sdmmc_dev *sdmmc)
{
    kmm_free(sdmmc->desp);
}


static void v3s_sdmmc_interrupt_work(FAR void *arg)
{
    int ret;
    uint32_t regval;
    struct v3s_sdmmc_dev *sdmmc;
    
    sdmmc = (struct v3s_sdmmc_dev *)arg;

    uint32_t idma_sta = getreg32(&sdmmc->regs->idst);
    mmcdbg("idma-irq... (val:0x%x.)  \n", idma_sta);
    // v3s_idma_dummp_desp(sdmmc);

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

    if (regval & (SDMMC_RISR_AUTO_CMD_OK | SDMMC_RISR_COMMAND_OK)) {
        mmcdbg("Command Complete.\n");
        sdmmc->event |= SDMMC_EVENT_COMMAND_OK;
        /* Realese semp */
        nxsem_post(&sdmmc->cmd_sem);
    }

    if (regval & (SDMMC_RISR_TRANS_OK)) {
        mmcdbg("Transfer complete...\n");
        /* Realese semp */
        nxsem_post(&sdmmc->trans_sem);
    }
        
    if (regval & SDMMC_RISR_RESP_ERR) {
        mmcdbg("!!! Resp no resp / resp err .\n");
    }

    if (sdmmc->access_type == SDMMC_ACCESS_TYPE_DMA) {

        if (idma_sta & V3S_IDMA_STA_TXINT) {
            /* Realese semp */
            nxsem_post(&sdmmc->trans_sem);
        }

        else if (idma_sta & V3S_IDMA_STA_RXINT) {

            /* Flush the cache */
            up_flush_dcache((uintptr_t)sdmmc->desp->des2, ((uintptr_t)sdmmc->desp->des2) + sdmmc->trans_bytes);
            up_flush_dcache((uintptr_t)sdmmc->desp, (uintptr_t) (sdmmc->desp + sdmmc->desp_num));

            /* Realese semp */
            nxsem_post(&sdmmc->trans_sem);

            // printf("IDMA Rx complete...\n");
            // for (int j=0; j<512; j++)
            // {
            //     printf("%x, ", sdmmc->trans_buffer[j]);
            //     if (j%16 == 0)
            //     {
            //         printf("\n");
            //     }
            // }
        }
    }
    else if (sdmmc->access_type == SDMMC_ACCESS_TYPE_POLL) {

        if (regval & SDMMC_RISR_TRANS_REQ) {
            mmcdbg("Tx Resq.\n");
            ret = sdmmc_trans_data_by_poll(sdmmc, 1);
            if (ret != 0) {
                mmc_err("Transfer data to card failed.\n");
            }
        }

        if (regval & SDMMC_RISR_RECV_REQ) {
            mmcdbg("Rx Resq.\n");
            ret = sdmmc_trans_data_by_poll(sdmmc, 0);
            if (ret != 0) {
                mmc_err("Read data to host failed.\n");
            }
        }
    }
    
    /* Clear IDMA interrupt status */
    putreg32(idma_sta, &sdmmc->regs->idst);

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
            mmc_err("SDMMC: Failed to initialize semaphore\n");
            kmm_free(sdmmc);
            return NULL;
        }
    }

    ret = nxsem_init(&sdmmc->trans_sem, 0, 0);
    {
        if (ret < 0) {
            mmc_err("SDMMC: Failed to initialize semaphore\n");
            kmm_free(sdmmc);
            return NULL;
        }
    }

    /* Init SDMMC0 Pin */
    mmcdbg("\nInit Sdmmc Pin...");
    v3s_sdmmc0_pin_cfg();
        
    /* Set sdmmc clock by CCMU */
    v3s_sdmmc_ccmu_int(sdmmc);

    /* IDMA descriptor init */
    v3s_sdmmc_idma_desp_init(sdmmc);

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
#ifdef CONFIG_SDIO_DMA
    caps |= SDIO_CAPS_DMASUPPORTED;
#endif

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
    uint32_t is_transfer = 0;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }

    sdmmc_host->event = 0;
    nxsem_reset(&sdmmc_host->cmd_sem, 0);
    nxsem_reset(&sdmmc_host->trans_sem, 0);

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

    is_transfer = (cmd & MMCSD_DATAXFR_MASK);
    if (is_transfer) {
        
        switch (is_transfer)
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
    }
    else {
        sdmmc_host->access_type = SDMMC_ACCESS_TYPE_POLL;
    }

        /* POLL or DMA */
        regval = getreg32(&sdmmc_host->regs->gctrl);

    if (sdmmc_host->access_type == SDMMC_ACCESS_TYPE_DMA) {

        uint32_t imask = SDMMC_SIMR_RESP_ERR | SDMMC_SIMR_COMMAND_OK  | \
                            SDMMC_SIMR_DATA_CRC_ERR | SDMMC_SIMR_RESP_TIMEOUT | SDMMC_SIMR_DATA_TIMEOUT | \
                            SDMMC_SIMR_SWITCH_OK | SDMMC_SIMR_FIFO_OVERFLOW | SDMMC_SIMR_CMD_BUSY | \
                            SDMMC_SIMR_START_ERR | SDMMC_SIMR_AUTO_CMD_OK | SDMMC_SIMR_END_ERR | \
                            SDMMC_SIMR_CARD_INSERTED | SDMMC_SIMR_CARD_REMOVED | SDMMC_RISR_DATA_CRC_ERR;
        putreg32(imask, &sdmmc_host->regs->simr);

        putreg32(V3S_IDMA_IE_TXINT | V3S_IDMA_IE_RXINT | 
                    V3S_IDMA_IE_BUS_ERR | V3S_IDMA_IE_DESP_ERR | 
                    V3S_IDMA_IE_ERR_SUM | V3S_IDMA_IE_INT_SUM | V3S_IDMA_IE_ABN_INT, 
                    &sdmmc_host->regs->idie);
        regval &= ~SDMMC_SGCR_AC_MODE;
        regval |= SDMMC_SGCR_DMA_ENB;
    }
    else if (sdmmc_host->access_type == SDMMC_ACCESS_TYPE_POLL) {

        uint32_t imask = SDMMC_SIMR_RESP_ERR | SDMMC_SIMR_COMMAND_OK | SDMMC_SIMR_TRANS_OK | \
                            SDMMC_SIMR_TRANS_REQ | SDMMC_SIMR_RECV_REQ | SDMMC_SIMR_DATA_CRC_ERR | \
                            SDMMC_SIMR_RESP_TIMEOUT | SDMMC_SIMR_DATA_TIMEOUT | SDMMC_SIMR_SWITCH_OK | \
                            SDMMC_SIMR_FIFO_OVERFLOW | SDMMC_SIMR_CMD_BUSY | SDMMC_SIMR_START_ERR | \
                            SDMMC_SIMR_AUTO_CMD_OK | SDMMC_SIMR_END_ERR | SDMMC_SIMR_CARD_INSERTED |\
                            SDMMC_SIMR_CARD_REMOVED | SDMMC_RISR_DATA_CRC_ERR;
        putreg32(imask, &sdmmc_host->regs->simr);

        regval |= SDMMC_SGCR_AC_MODE;
        regval &= ~SDMMC_SGCR_DMA_ENB;
    }

    putreg32(regval, &sdmmc_host->regs->gctrl);

    putreg32(arg, &sdmmc_host->regs->arg);
    cmdval |= (cmdidx | SDMMC_WAIT_PRE_OVER | SDMMC_CMD_LOAD);
    putreg32(cmdval, &sdmmc_host->regs->cmd);

    /* Unmask IRQ */
    regval = getreg32(&sdmmc_host->regs->gctrl);
    regval |= (SDMMC_SGCR_INT_ENB | SDMMC_SGCR_DBC_ENB);
    putreg32(regval, &sdmmc_host->regs->gctrl);

    /* The framework identifies SDcard and MMC/eMMC cards by judging whether 
    to respond to the CMD1 command. Even if the card device does not respond, 
    this command will first trigger the command completion interrupt and then 
    the timeout interrupt. Therefore, the delay here ensures that the timeout 
    interrupt is not missed and the card type is correctly identified. */
    if (cmdidx == MMC_CMDIDX1)
    {
        v3s_udelay(5000);
    }

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
        mmc_err("mmc %d wait response failed\n", sdmmc_host->sdmmc_no);
        return -ETIMEDOUT;
    }

    return ret;
}


static int v3s_mmc_host_recv_response (struct sdio_dev_s *dev, uint32_t cmd, uint32_t *resp)
{
    int ret = -1;
    bool  busy;
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
        case MMCSD_R1B_RESPONSE:
            // Check busy signal
            busy = v3s_host_wait_busy(sdmmc_host);
            if (busy) {
                mmc_err("mmc %d wait busy failed\n", sdmmc_host->sdmmc_no);
                //return -ETIMEDOUT;
            }
        case MMCSD_R1_RESPONSE:
        case MMCSD_R3_RESPONSE:
        case MMCSD_R4_RESPONSE:
        case MMCSD_R5_RESPONSE:
        case MMCSD_R6_RESPONSE:
        case MMCSD_R7_RESPONSE:
        {
            if (event & SDMMC_EVENT_RESPONSE_TIMEOUT) {
                ret = (-ETIMEDOUT);
            }
            else if (event & SDMMC_EVENT_COMMAND_OK) {
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
    sdmmc_host->access_type = SDMMC_ACCESS_TYPE_POLL;
    sdmmc_host->trans_buffer = (uint8_t *)buffer;
    sdmmc_host->trans_bytes  = nbytes;
    return 0;
}


static int v3s_mmc_host_recv_setup (struct sdio_dev_s *dev, uint8_t *buffer, size_t nbytes)
{
    volatile uint32_t data;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }
    v3s_host_reset_fifo(sdmmc_host);
    sdmmc_host->access_type = SDMMC_ACCESS_TYPE_POLL;
    sdmmc_host->trans_buffer = (uint8_t *)buffer;
    sdmmc_host->trans_bytes  = nbytes;
    return 0;
}


static int v3s_mmc_host_dmapreflight(struct sdio_dev_s *dev, const uint8_t *buffer, size_t buflen)
{
    if ((buffer == NULL) || ((uintptr_t)buffer % 4) || (buflen == 0)) {
        mmc_err("Invalid buffer or size\n");
        return -1;
    }
    return 0;
}


static int v3s_mmc_host_dmasendsetup (struct sdio_dev_s *dev, const uint8_t *buffer, size_t buflen)
{
    int ret;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }

    if (buflen < SDMMC_BLOCKSIZE) {
        ret = v3s_mmc_host_send_setup(dev, buffer, buflen);
    }
    else {
        ret = v3s_sdmmc_setup_idma(sdmmc_host, buffer, buflen);
    }
    return ret;
}


static int v3s_mmc_host_dmarecvsetup (struct sdio_dev_s *dev, uint8_t *buffer, size_t buflen)
{
    int ret;
    struct v3s_sdmmc_dev *sdmmc_host;

    sdmmc_host = container_of(dev, struct v3s_sdmmc_dev, sdio);
    if (sdmmc_host == NULL) {
        return -EINVAL;
    }

    if (buflen < SDMMC_BLOCKSIZE) {
        ret = v3s_mmc_host_recv_setup(dev, buffer, buflen);
    }
    else {
        ret = v3s_sdmmc_setup_idma(sdmmc_host, buffer, buflen);
    }
    
    return ret;
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
    return;
}


static void v3s_mmc_host_waitenable(struct sdio_dev_s *dev,
    sdio_eventset_t eventset, uint32_t timeout)
{
    (void)dev;
    return;
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
        mmc_err("mmc %d wait data transfer failed\n", sdmmc_host->sdmmc_no);
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
    struct v3s_sdmmc_dev *dev;
    struct sdio_dev_s *sdio;

    dev = v3s_sdmmc_probe(minor);
    if (dev == NULL)
    {
        mmc_err("v3s_sdmmc_probe failed\n");
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
# ifdef CONFIG_ARCH_HAVE_SDIO_PREFLIGHT
    sdio->dmapreflight = v3s_mmc_host_dmapreflight;
#endif
    sdio->dmarecvsetup = v3s_mmc_host_dmarecvsetup;
    sdio->dmasendsetup  = v3s_mmc_host_dmasendsetup;
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