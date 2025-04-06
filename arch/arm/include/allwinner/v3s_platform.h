/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Jerry Wang <wangflord@allwinnertech.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.     See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __ALLWINNER_V3S__
#define __ALLWINNER_V3S__


#define V3S_SRAM_A1_BASE            0X00000000
#define V3S_SRAM_A1_SIZE            (16 * 1024)        /* 16k */

#define V3S_SCU_SPPPACE_BASE        0x01C80000

#define V3S_SRAM_D_BASE             0X01C00000
#define V3S_SRAMC_BASE              0X01C00000
#define V3S_DMA_BASE                0X01C02000
#define V3S_NFC0_BASE               0X01C03000
#define V3S_TS_BASE                 0X01C04000
#define V3S_NFC1_BASE               0X01C05000

#define V3S_LCD0_BASE               0X01C0C000
#define V3S_LCD1_BASE               0X01C0D000
#define V3S_VE_BASE                 0X01C0E000
#define V3S_MMC0_BASE               0X01C0F000
#define V3S_MMC1_BASE               0X01C10000
#define V3S_MMC2_BASE               0X01C11000
#define V3S_MMC3_BASE               0X01C12000

#define V3S_SS_BASE                 0X01C15000
#define V3S_HDMI_BASE               0X01C16000
#define V3S_MSGBOX_BASE             0X01C17000
#define V3S_SPINLOCK_BASE           0X01C18000
#define V3S_USBOTG_BASE             0X01C19000
#define V3S_USBEHCI0_BASE           0X01C1A000
#define V3S_USBEHCI1_BASE           0X01C1B000
#define V3S_USBEOCI2_BASE           0X01C1C000


#define V3S_CCM_BASE                0X01C20000

#define V3S_R_PIO_BASE              0X01F02C00
#define V3S_PIO_BASE                0X01C20800
#define V3S_TIMER_BASE              0X01C20C00
#define V3S_SPDIF_BASE              0X01C21000
#define V3S_PWM_BASE                0X01C21400

#define V3S_DAUDIO0_BASE            0X01C22000
#define V3S_DAUDIO1_BASE            0X01C22400

#define V3S_LRADC_BASE              0X01C22800
#define V3S_CODEC_BASE              0X01C22C00


#define V3S_TP_BASE                 0X01C25000
#define V3S_DMIC_BASE               0X01C25400

#define V3S_UART0_BASE              0X01C28000
#define V3S_UART1_BASE              0X01C28400
#define V3S_UART2_BASE              0X01C28800
#define V3S_UART3_BASE              0X01C28C00
#define V3S_UART4_BASE              0X01C29000
#define V3S_UART5_BASE              0X01C29400

#define V3S_TWI_OFFSET              (0x400)
#define V3S_TWI_COUNT               (4)
#define V3S_TWI0_BASE               0X01C2AC00
#define V3S_TWI1_BASE               0X01C2B000
#define V3S_TWI2_BASE               0X01C2B400
#define V3S_TWI3_BASE               0X01C2B800

#define V3S_GMAC_BASE               0X01C30000
#define V3S_GPU_BASE                0X01C40000

#define V3S_DRAMCOM_BASE            0X01C62000
#define V3S_DRAMCTL0_BASE           0X01C63000
#define V3S_DRAMCTL1_BASE           0X01C64000
#define V3S_DRAMPHY0_BASE           0X01C65000
#define V3S_DRAMPHY1_BASE           0X01C66000


#define V3S_DE_FE0_BASE             0X01E00000
#define V3S_DE_FE1_BASE             0X01E20000
#define V3S_DE_BE0_BASE             0X01E60000
#define V3S_DE_BE1_BASE             0X01E40000
#define V3S_DE_DRC0_BASE            0x01e70000
#define V3S_DE_DRC1_BASE            0x01e50000
#define V3S_DE_DEU0_BASE            0x01eb0000
#define V3S_DE_DEU1_BASE            0x01ea0000
#define V3S_MIPI_DSI0_BASE          0x01ca0000
#define V3S_MIPI_DSI0_DPHY_BASE     0x01ca1000

#define V3S_MP_BASE                 0X01E80000

#define V3S_RTC_BASE                0X01C20400
#define RTC_GENERAL_PURPOSE_REG(n)  (V3S_RTC_BASE + 0x100 + (n) * 0x4)

#define V3S_RPRCM_BASE              0x01f01400

#define V3S_BROM_BASE               0XFFFF0000        /* 32K */

#define V3S_SYS_CTRL                0x01c00000
#define V3S_CCMU_BASE               0x01c20000


/* \brief IRQ Define */
#define V3S_UART0_IRQ               32
#define V3S_UART1_IRQ               33
#define V3S_UART2_IRQ               34
#define V3S_TIM0_IRQ                50
#define V3S_TIM1_IRQ                51
#define V3S_TIM2_IRQ                52
#define V3S_EMAC_IRQ                114

#endif /* __ALLWINNER_V3S__ */
