#include "arch/allwinner/v3s_ccmu.h"
#include <stdint.h>


#define set_wbit(addr, v)   (*((volatile unsigned long  *)(addr)) |=  (unsigned long)(v))
#define clr_wbit(addr, v)   (*((volatile unsigned long  *)(addr)) &= ~(unsigned long)(v))


 void ccm_clock_enable(uint32_t clk_id)
 {
     switch(clk_id>>8) {
         case AXI_BUS:
             set_wbit(CCM_AXI_GATE_CTRL, 0x1U<<(clk_id&0xff));
             break;
         case AHB1_BUS0:
             set_wbit(CCM_AHB1_GATE0_CTRL, 0x1U<<(clk_id&0xff));
             break;
         case AHB1_BUS1:
             set_wbit(CCM_AHB1_GATE1_CTRL, 0x1U<<(clk_id&0xff));
             break;
         case APB1_BUS0:
             set_wbit(CCM_APB1_GATE0_CTRL, 0x1U<<(clk_id&0xff));
             break;
         case APB2_BUS0:
             set_wbit(CCM_APB2_GATE0_CTRL, 0x1U<<(clk_id&0xff));
             break;
     }
 }
 
 void ccm_clock_disable(uint32_t clk_id)
 {
     switch(clk_id>>8) {
         case AXI_BUS:
             clr_wbit(CCM_AXI_GATE_CTRL, 0x1U<<(clk_id&0xff));
             break;
         case AHB1_BUS0:
             clr_wbit(CCM_AHB1_GATE0_CTRL, 0x1U<<(clk_id&0xff));
             break;
         case AHB1_BUS1:
             clr_wbit(CCM_AHB1_GATE1_CTRL, 0x1U<<(clk_id&0xff));
             break;
         case APB1_BUS0:
             clr_wbit(CCM_APB1_GATE0_CTRL, 0x1U<<(clk_id&0xff));
             break;
         case APB2_BUS0:
             clr_wbit(CCM_APB2_GATE0_CTRL, 0x1U<<(clk_id&0xff));
             break;
     }
 }
 
 void ccm_module_enable(uint32_t clk_id)
 {
     switch(clk_id>>8) {
         case AHB1_BUS0:
             set_wbit(CCM_AHB1_RST_REG0, 0x1U<<(clk_id&0xff));
             break;
         case AHB1_BUS1:
             set_wbit(CCM_AHB1_RST_REG1, 0x1U<<(clk_id&0xff));
             break;
         case AHB1_LVDS:
             set_wbit(CCM_AHB1_RST_REG2, 0x1U<<(clk_id&0xff));
             break;
         case APB1_BUS0:
             set_wbit(CCM_APB1_RST_REG, 0x1U<<(clk_id&0xff));
             break;
         case APB2_BUS0:
             set_wbit(CCM_APB2_RST_REG, 0x1U<<(clk_id&0xff));
             break;
     }
 }
 
 void ccm_module_disable(uint32_t clk_id)
 {
     switch(clk_id>>8) {
         case AHB1_BUS0:
             clr_wbit(CCM_AHB1_RST_REG0, 0x1U<<(clk_id&0xff));
             break;
         case AHB1_BUS1:
             clr_wbit(CCM_AHB1_RST_REG1, 0x1U<<(clk_id&0xff));
             break;
         case AHB1_LVDS:
             clr_wbit(CCM_AHB1_RST_REG2, 0x1U<<(clk_id&0xff));
             break;
         case APB1_BUS0:
             clr_wbit(CCM_APB1_RST_REG, 0x1U<<(clk_id&0xff));
             break;
         case APB2_BUS0:
             clr_wbit(CCM_APB2_RST_REG, 0x1U<<(clk_id&0xff));
             break;
     }
 }
 
 void ccm_module_reset(uint32_t clk_id)
 {
     ccm_module_disable(clk_id);
     ccm_module_disable(clk_id);
     ccm_module_enable(clk_id);
 }
 
 void ccm_clock_disable_all(void)
 {
     writel(0, CCM_AXI_GATE_CTRL);
     writel(0, CCM_AHB1_GATE0_CTRL);
     writel(0, CCM_AHB1_GATE1_CTRL);
     writel(0, CCM_APB1_GATE0_CTRL);
     writel(0, CCM_APB2_GATE0_CTRL);
 //	clr_wbit(CCM_AXI_GATE_CTRL, 0xffffffff);
 //	clr_wbit(CCM_AHB1_GATE0_CTRL, 0xffffffff);
 //	clr_wbit(CCM_AHB1_GATE1_CTRL, 0xffffffff);
 //	clr_wbit(CCM_APB1_GATE0_CTRL, 0xffffffff);
 //	clr_wbit(CCM_APB2_GATE0_CTRL, 0xffffffff);
 }
 
 void ccm_reset_all_module(void)
 {
     writel(0, CCM_AHB1_RST_REG0);
     writel(0, CCM_AHB1_RST_REG1);
     writel(0, CCM_AHB1_RST_REG2);
     writel(0, CCM_APB1_RST_REG);
     writel(0, CCM_APB2_RST_REG);
 //	clr_wbit(CCM_AHB1_RST_REG0, 0xffffffff);
 //	clr_wbit(CCM_AHB1_RST_REG1, 0xffffffff);
 //	clr_wbit(CCM_AHB1_RST_REG2, 0xffffffff);
 //	clr_wbit(CCM_APB1_RST_REG, 0xffffffff);
 //	clr_wbit(CCM_APB2_RST_REG, 0xffffffff);
 }
 
 static void ccm_wait_pll_stable(uint32_t pll_base)
 {
 #ifndef FPGA_PLATFORM
     uint32_t rval = 0;
     uint32_t time = 0xffff;
 
     do {
         rval = readl(pll_base);
         time --;
     } while (time && !(rval & CCM_PLL_STABLE_FLAG));
 #endif
 }
 
 /* pll1 = 24*n*k/m */
 uint32_t ccm_setup_pll1_cpux_clk(uint32_t pll_clk)
 {
     uint32_t n, k, m = 1;
     uint32_t rval = 0;
     uint32_t div = 0;
     uint32_t mod2, mod3;
     uint32_t min_mod = 0;
 
 #ifndef FPGA_PLATFORM
     if (pll_clk < 240000000)
         pll_clk = 240000000;
     if (pll_clk > 2000000000)
         pll_clk = 2000000000;
 #endif
 
 #ifdef SYSTEM_SIMULATION
     k = 1;
     n = 15;	//for 360M
     rval = (1U << 31) | ((n-1) << 8) | ((k-1) << 4) | (m-1);
     writel(rval, CCM_PLL1_CPUX_CTRL);
     ccm_wait_pll_stable(CCM_PLL1_CPUX_CTRL);
     return PLL1_CPUX_CLK;
 #else
     div = pll_clk/24000000;
     if (div <= 32) {
         n = div;
         k = 1;
     } else {
         /* when m=1, we cann't get absolutely accurate value for follow clock:
          * 840(816), 888(864),
          * 984(960),
          * 1032(1008),
          * 1128(1104), 1176(1152),
          * 1272(1248)
          * 1320(1296),
          * 1416(1392), 1464(1440),
          * 1560(1536),
          * 1608(1584),
          * 1704(1680), 1752(1728),
          * 1848(1824), 1896(1872),
          * 1992(1968)
          */
         mod2 = div&1;
         mod3 = div%3;
         min_mod = mod2;
         k = 2;
         if (min_mod > mod3) {
             min_mod = mod3;
             k = 3;
         }
         n = div / k;
     }
 
     rval = (1U << 31) | ((n-1) << 8) | ((k-1) << 4) | (m-1);
     writel(rval, CCM_PLL1_CPUX_CTRL);
     ccm_wait_pll_stable(CCM_PLL1_CPUX_CTRL);
 
     return 24000000 * n * k / m;
 #endif
 }
 
 uint32_t ccm_get_pll1_cpux_clk(void)
 {
 #ifdef FPGA_PLATFORM
     return PLL1_CPUX_CLK;
 #else
     uint32_t rval = 0;
     uint32_t n, k, m;
     rval = readl(CCM_PLL1_CPUX_CTRL);
     n = (0x1f & (rval >> 8)) + 1;
     k = (0x3 & (rval >> 4)) + 1;
     m = (0x3 & (rval >> 0)) + 1;
     return 24000000 * n * k / m;
 #endif
 }
 
 /* pll2 = 24*n/p/m */
 uint32_t ccm_setup_pll2_audio_clk(uint32_t pll2_clk)
 {
     return 0;
 }
 
 uint32_t ccm_get_pll2_audio_clk(void)
 {
     return 0;
 }
 
 /* pll = 24 * n / m */
 uint32_t ccm_setup_de_ve_pll_clk(uint32_t base, uint32_t mode_sel, uint32_t pll_clk)
 {
     uint32_t n = 1, m = 8;
     uint32_t frac_clk = 1;
     uint32_t rval;
 
     if (mode_sel) {
         //integer mode
         if (pll_clk <=384000000) {
             n = pll_clk/3000000;
         } else if (pll_clk <=512000000) {
             m = 6;
             n = pll_clk/4000000;
         } else if (pll_clk <=768000000) {
             m = 4;
             n = pll_clk/6000000;
         } else if (pll_clk <=1024000000) {
             m = 3;
             n = pll_clk/8000000;
         } else if (pll_clk <=1536000000) {
             m = 2;
             n = pll_clk/12000000;
         } else {
             m = 1;
             n = pll_clk/24000000;
         }
     } else {
         //fractional mode
         if (pll_clk == 270000000)
             frac_clk = 0;
     }
     rval = (1U << 31) | (frac_clk << 25) | (mode_sel << 24) | ((n-1) << 8) | (m - 1);
     writel(rval, base);
     ccm_wait_pll_stable(base);
     return pll_clk;
 }
 
 uint32_t ccm_get_de_ve_pll_clk(uint32_t base)
 {
     uint32_t rval = readl(base);
     uint32_t mode = 1 & (rval >> 24);
     uint32_t pll_clk;
 
     if (mode) {
         //integer mode;
         uint32_t m = (rval & 0xf) + 1;
         uint32_t n = ((rval >> 8) & 0x7f) + 1;
         pll_clk = 24000000 * n / m;
     } else {
         if (1 & (rval >> 25))
             pll_clk = 297000000;
         else
             pll_clk = 270000000;
     }
     return pll_clk;
 }
 
 /* pll3 = 24*n/m */
 uint32_t ccm_setup_pll3_video0_clk(uint32_t mode_sel, uint32_t pll_clk)
 {
     return ccm_setup_de_ve_pll_clk(CCM_PLL3_VIDEO_CTRL, mode_sel, pll_clk);
 }
 
 uint32_t ccm_get_pll3_video0_clk(void)
 {
 #ifdef FPGA_PLATFORM
     return PLL3_VIDEO0_CLK;
 #else
     return ccm_get_de_ve_pll_clk(CCM_PLL3_VIDEO_CTRL);
 #endif
 }
 
 /* pll4 = 24*n/m */
 uint32_t ccm_setup_pll4_ve_clk(uint32_t mode_sel, uint32_t pll_clk)
 {
     return ccm_setup_de_ve_pll_clk(CCM_PLL4_VE_CTRL, mode_sel, pll_clk);
 }
 
 uint32_t ccm_get_pll4_ve_clk(void)
 {
 #ifdef FPGA_PLATFORM
     return PLL4_VE_CLK;
 #else
     return ccm_get_de_ve_pll_clk(CCM_PLL4_VE_CTRL);
 #endif
 }
 
 /* pll5 = 24*n*k/m */
 uint32_t ccm_setup_pll5_ddr_clk(uint32_t pll_clk)
 {
     uint32_t n, k, m = 1;
     uint32_t mod2, mod3, min_mod;
     uint32_t div = 0;
     uint32_t rval;
 
 #ifndef FPGA_PLATFORM
     if (pll_clk < 24000000)
         pll_clk = 24000000;
     if (pll_clk > 1000000000)
         pll_clk = 1000000000;
 #endif
 
 #ifdef SYSTEM_SIMULATION
     k = 1;
     n = 15;	//for 360M
     rval = (1U << 31) | ((n-1) << 8) | ((k-1) << 4) | (m-1);
     writel(rval, CCM_PLL5_DDR_CTRL);
     writel(rval|(1U << 20), CCM_PLL5_DDR_CTRL);
     ccm_wait_pll_stable(CCM_PLL5_DDR_CTRL);
     return PLL5_DDR_CLK;
 #else
     div = pll_clk/24000000;
     if (div <= 32) {
         n = div;
         k = 1;
     } else {
         /* when m=1, we cann't get absolutely accurate value for follow clock:
          * 840(816), 888(864),
          * 984(960)
          */
         mod2 = div&1;
         mod3 = div%3;
         min_mod = mod2;
         k = 2;
         if (min_mod > mod3) {
             min_mod = mod3;
             k = 3;
         }
         n = div / k;
     }
     rval = (1U << 31)  | ((n-1) << 8) | ((k-1) << 4) | (m-1);
     writel(rval, CCM_PLL5_DDR_CTRL);
     writel(rval|(1U << 20), CCM_PLL5_DDR_CTRL);
     ccm_wait_pll_stable(CCM_PLL5_DDR_CTRL);
 
     return 24000000 * n * k / m;
 #endif
 }
 
 uint32_t ccm_get_pll5_ddr_clk(void)
 {
 #ifdef FPGA_PLATFORM
     return PLL5_DDR_CLK;
 #else
     uint32_t rval = 0;
     uint32_t n, k, m;
     rval = readl(CCM_PLL5_DDR_CTRL);
     n = (0x1f & (rval >> 8)) + 1;
     k = (0x3 & (rval >> 4)) + 1;
     m = (0x3 & (rval >> 0)) + 1;
     return 24000000 * n * k / m;
 #endif
 }
 
 /* pll1 = 24*n*k/2 */
 uint32_t ccm_setup_pll6_dev_clk(uint32_t pll_clk)
 {
     uint32_t n, k;
     uint32_t div;
     uint32_t rval;
 
 #ifndef FPGA_PLATFORM
     if (pll_clk < 240000000)
         pll_clk = 240000000;
     if (pll_clk > 2000000000)
         pll_clk = 2000000000;
 #endif
 
 #ifdef SYSTEM_SIMULATION
     k = 1;
     n = 15;	//for 360M
     rval = (1U << 31) | (1U << 24) | (1U << 18) | ((n-1) << 8) | ((k-1) << 4);
     writel(rval, CCM_PLL6_MOD_CTRL);
     ccm_wait_pll_stable(CCM_PLL6_MOD_CTRL);
     return PLL6_DEV_CLK >> 1;
 #else
     div = pll_clk/24000000;
     if (div <= 32)
         k = 1;
     else if (div <= 64)
         k = 2;
     else if (div <= 96)
         k = 3;
     else
         k = 4;
     n = div / k;
 
     rval = (1U << 31) | (1U << 24) | (1U << 18) | ((n-1) << 8) | ((k-1) << 4);
     writel(rval, CCM_PLL6_MOD_CTRL);
     ccm_wait_pll_stable(CCM_PLL6_MOD_CTRL);
 
     return (24000000 * n * k)>>1;
 #endif
 }
 
 uint32_t ccm_get_pll6_dev_clk(void)
 {
 #ifdef FPGA_PLATFORM
     return PLL6_DEV_CLK >> 1;
 #else
     uint32_t rval = 0;
     uint32_t n, k;
     rval = readl(CCM_PLL6_MOD_CTRL);
     n = (0x1f & (rval >> 8)) + 1;
     k = (0x3 & (rval >> 4)) + 1;
     return (24000000 * n * k)>>1;
 #endif
 }
 
 /* pll7 = 24*n/m */
 uint32_t ccm_setup_pll7_video0_clk(uint32_t mode_sel, uint32_t pll_clk)
 {
     return ccm_setup_de_ve_pll_clk(CCM_PLL7_VIDEO1_CTRL, mode_sel, pll_clk);
 }
 
 uint32_t ccm_get_pll7_video0_clk(void)
 {
 #ifdef FPGA_PLATFORM
     return PLL7_VIDEO1_CLK;
 #else
     return ccm_get_de_ve_pll_clk(CCM_PLL7_VIDEO1_CTRL);
 #endif
 }
 
 /* pll8 = 24*n/m */
 uint32_t ccm_setup_pll8_gpu_clk(uint32_t mode_sel, uint32_t pll_clk)
 {
     return ccm_setup_de_ve_pll_clk(CCM_PLL8_GPU_CTRL, mode_sel, pll_clk);
 }
 
 uint32_t ccm_get_pll8_gpu_clk(void)
 {
 #ifdef FPGA_PLATFORM
     return PLL8_GPU_CLK;
 #else
     return ccm_get_de_ve_pll_clk(CCM_PLL8_GPU_CTRL);
 #endif
 }
 
 void ccm_set_cpu_clk_src(uint32_t src)
 {
     uint32_t rval = readl(CCM_CPU_L2_AXI_CTRL);
     rval &= ~(3 << 16);
     rval |= src << 16;
     writel(rval, CCM_CPU_L2_AXI_CTRL);
 }
 
 uint32_t ccm_set_mbus0_clk(uint32_t src, uint32_t clk)
 {
     uint32_t sclk = 0;
     uint32_t real = 0;
     uint32_t m = 1;
     uint32_t rval;
 
     switch (src) {
         case 1:
             sclk = ccm_get_pll6_dev_clk();
             break;
         case 2:
             sclk = ccm_get_pll5_ddr_clk();
             break;
         default:
             sclk = 24000000;
             break;
     }
     real = sclk;
     while (real > clk) {
         m++;
         real = sclk / m;
     }
     rval = (1U << 31) | (src << 24) | (m-1);
     writel(rval, CCM_MBUS_SCLK_CTRL0);
     return real;
 }
 
 uint32_t ccm_set_mbus1_clk(uint32_t src, uint32_t clk)
 {
     uint32_t sclk = 0;
     uint32_t real = 0;
     uint32_t m = 1;
     uint32_t rval;
 
     switch (src) {
         case 1:
             sclk = ccm_get_pll6_dev_clk();
             break;
         case 2:
             sclk = ccm_get_pll5_ddr_clk();
             break;
         default:
             sclk = 24000000;
             break;
     }
 
     real = sclk;
     while (real > clk) {
         m++;
         real = sclk / m;
     }
     rval = (1U << 31) | (src << 24) | (m-1);
     writel(rval, CCM_MBUS_SCLK_CTRL1);
     return real;
 }
 
 void ccm_set_cpu_l2_axi_div(uint32_t periph_div, uint32_t l2_div, uint32_t axi_div)
 {
     uint32_t rval = readl(CCM_CPU_L2_AXI_CTRL);
     periph_div >>= 2;
     l2_div = l2_div&0x4 ? 4 : l2_div-1;
     axi_div = axi_div&0x4 ? 4 : axi_div-1;
     rval &= ~((1 << 8) | (7 << 4) | (7));
     rval |= (periph_div << 8) | (l2_div << 4) | (axi_div);
     writel(rval, CCM_CPU_L2_AXI_CTRL);
 }
 
 void ccm_set_ahb1_clk_src(uint32_t src)
 {
     uint32_t rval = readl(CCM_AHB1_APB1_CTRL);
     rval &= ~(3 << 12);
     rval |= src << 12;
     writel(rval, CCM_AHB1_APB1_CTRL);
 }
 
 void ccm_set_ahb1_apb1_div(uint32_t prediv, uint32_t ahb1_div, uint32_t apb1_div)
 {
     uint32_t ahb1div;
     uint32_t rval = readl(CCM_AHB1_APB1_CTRL);
 
     switch (ahb1_div) {
         case 1:
             ahb1div = 0;
             break;
         case 2:
             ahb1div = 1;
             break;
         case 4:
             ahb1div = 2;
             break;
         case 8:
             ahb1div = 3;
             break;
         default:
             ahb1div = 1;
             break;
     }
     rval &= ~(0x3f << 4); //(3 << 6) | (3 << 4) | (3 << 8)
     rval |= (apb1_div << 8) | ((prediv-1) << 6) | (ahb1div << 4);
     writel(rval, CCM_AHB1_APB1_CTRL);
 }
 
 int32_t ccm_set_apb2_clk(uint32_t apb2_clk)
 {
     uint32_t rval;
     uint32_t src;
     uint32_t sclk;
     uint32_t div, n, m;
 
     if (apb2_clk <= 32000) {
         writel(0, CCM_APB2_CLK_CTRL);
         return 0;
     }
     if (apb2_clk <= 24000000) {
         writel(1U << 24, CCM_APB2_CLK_CTRL);
         return 0;
     }
 
     src = 2;
     sclk = ccm_get_pll6_dev_clk();
     div = sclk / apb2_clk;
     if (div <= 32)
         n = 0;
     else if (div <= 64)
         n = 1;
     else if (div <= 128)
         n = 2;
     else
         n = 3;
     m = (div / (1 << n)) - 1;
 
     rval = (src << 24) | (n << 16) | m;
     writel(rval, CCM_APB2_CLK_CTRL);
     return rval;
 }
 
 void ccm_set_pll_stable_time(uint32_t time)
 {
     writel(time&0xffff, CCM_PLL_STABLE_REG);
 }
 
 void ccm_set_mclk_stable_time(uint32_t time)
 {
     writel(time&0xffff, CCM_MCLK_STABLE_REG);
 }
 
 uint32_t ccm_get_axi_clk(void)
 {
 
 #ifdef FPGA_PLATFORM
     return AXICLK;
 #else
     uint32_t src;
     uint32_t rval;
     uint32_t axidiv;
     rval = readl(CCM_CPU_L2_AXI_CTRL);
     src = 0x3 & (rval >> 16);
     axidiv = (0x7 & (rval >> 0)) + 1;
     switch (src) {
         case 0:
             src = 32000;
             break;
         case 1:
             src = 24000000;
             break;
         case 2:
         case 3:
             src = ccm_get_pll1_cpux_clk();
             break;
     }
     return src / axidiv;
 #endif
 }
 
 uint32_t ccm_get_ahb1_clk(void)
 {
 
 #ifdef FPGA_PLATFORM
     return AHB1CLK;
 #else
     uint32_t src;
     uint32_t rval;
     uint32_t ahb1pdiv;
     uint32_t ahb1div;
     rval = readl(CCM_AHB1_APB1_CTRL);
     src = 0x3 & (rval >> 12);
     ahb1pdiv = (0x3 & (rval >> 6)) + 1;
     ahb1div = 1 << (0x3 & (rval >> 4));
     switch (src) {
         case 0:
             src = 32000;
             break;
         case 1:
             src = 24000000;
             break;
         case 2:
             src = ccm_get_axi_clk();
             break;
         case 3:
             src = ccm_get_pll6_dev_clk() / ahb1pdiv;
             break;
     }
     return src / ahb1div;
 #endif
 }
 
 uint32_t ccm_get_apb1_clk(void)
 {
 #ifdef FPGA_PLATFORM
     return APB1CLK;
 #else
     uint32_t rval;
     uint32_t apb1div;
 
     rval = readl(CCM_AHB1_APB1_CTRL);
     apb1div = 0x3 & (rval >> 8);
     if (apb1div == 0)
         apb1div = 1;
     apb1div = 1 << apb1div;
     return ccm_get_ahb1_clk() / apb1div;
 #endif
 }
 
 uint32_t ccm_get_apb2_clk(void)
 {
 #ifdef FPGA_PLATFORM
     return APB2CLK;
 #else
     uint32_t src;
     uint32_t rval;
     uint32_t n, m;
 
     rval = readl(CCM_APB2_CLK_CTRL);
     src = 0x3 & (rval >> 24);
     n = 0x3 & (rval >> 16);
     n = 1 << n;
     m = (0x1f & (rval >> 0)) + 1;
     switch (src) {
         case 0:
             src = 32000;
             break;
         case 1:
             src = 24000000;
             break;
         case 2:
         case 3:
             src = ccm_get_pll6_dev_clk();
             break;
     }
     return src / n / m;
 #endif
 }
 /*
 int32_t ccm_module_clk_gate_test(uint32_t clkid, uint32_t addr, uint32_t wr_val, uint32_t expt_val)
 {
     uint32_t rval;
 
     ccm_module_reset(clkid);
     ccm_clock_disable(clkid);
 
     writel(wr_val, addr);
     rval = readl(addr);
     if (rval == wr_val)
         return -1;
 
     ccm_clock_enable(clkid);
     writel(wr_val, addr);
     rval = readl(addr);
     if (rval != expt_val)
         return -1;
 
     return 0;
 }
 
 int32_t ccm_module_reset_test(uint32_t clkid, uint32_t addr, uint32_t default_value)
 {
     uint32_t rval;
 
     ccm_module_reset(clkid);
     ccm_clock_enable(clkid);
     rval = readl(addr);
     if (rval == 0 || rval != default_value)
         return -1;
 
     return 0;
 }
 */
 
 