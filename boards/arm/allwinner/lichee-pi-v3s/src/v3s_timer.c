#include <stdbool.h>
#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/config.h>
#include "arm_internal.h"
#include <nuttx/timers/arch_alarm.h>
#include "arch/allwinner/v3s_platform.h"


enum timer_no { TIM0 = 1, TIM1 = 2, TIM2 = 3};

/* \brief Allwinner V3S Timer Registers Define As Follows: */
#define TIM_IRQ_EN_REG_OFF                 0x00
#define TIM_IRQ_STA_REG_OFF                0x04
#define TIM_CTRL_REG_OFF(TIM_NO)           (0x10 * TIM_NO + 0x00)
#define TIM_INTV_VALUE_REG_OFF(TIM_NO)     (0x10 * TIM_NO + 0x04)
#define TIM_CUR_VALUE_REG_OFF(TIM_NO)      (0x10 * TIM_NO + 0x08)

#define _TIM_CTRL_EN_                      (0x01 << 0)
#define _TIM_CTRL_RELOAD_                  (0x01 << 1)
#define _TIM_CTRL_CLK_SRC_INTERNAL_        (0x00 << 2)
#define _TIM_CTRL_CLK_SRC_OSC24M_          (0x01 << 2)
#define _TIM_CTRL_CLK_PRE_SCALE_1_         (0x00 << 4)
#define _TIM_CTRL_CLK_PRE_SCALE_2_         (0x01 << 4)
#define _TIM_CTRL_CLK_PRE_SCALE_4_         (0x02 << 4)
#define _TIM_CTRL_CLK_PRE_SCALE_8_         (0x03 << 4)
#define _TIM_CTRL_CLK_PRE_SCALE_16_        (0x04 << 4)
#define _TIM_CTRL_CLK_PRE_SCALE_32_        (0x05 << 4)
#define _TIM_CTRL_CLK_PRE_SCALE_64_        (0x06 << 4)
#define _TIM_CTRL_CLK_PRE_SCALE_128_       (0x07 << 4)
#define _TIM_MODE_CONTINUOUS_              (0x00 << 7)
#define _TIM_MODE_SINGLE_                  (0x01 << 7)


/* OSC24M */
#define V3S_EXTEREN_CLOCK_FREQ             (24 * 1000000UL)


void nxsched_process_timer(void);
void up_nputs(FAR const char *str, size_t len);


static inline void v3s_timer_irq(enum timer_no timer_no, bool sta)
{
    uint32_t b;
    uint32_t irq_reg;
    
    b = (1 << (timer_no - 1));
    irq_reg = getreg32(V3S_TIMER_BASE + TIM_IRQ_EN_REG_OFF);
    if (sta) {
        /* enable irq */
        irq_reg |= b;
    } else {
        /* disable irq */
        irq_reg &= ~b;
    }
    putreg32(irq_reg, V3S_TIMER_BASE + TIM_IRQ_EN_REG_OFF);
    return;
}


static inline void v3s_timer_clear_irq(enum timer_no timer_type)
{
    uint32_t b;
    uint32_t irq_reg;
    
    b = (1 << (timer_type - 1));
    irq_reg = getreg32(V3S_TIMER_BASE + TIM_IRQ_STA_REG_OFF);
    /* clear irqpending bit */
    irq_reg |= b;
    putreg32(irq_reg, V3S_TIMER_BASE + TIM_IRQ_STA_REG_OFF);
    return;
}


static int v3s_timer_irq_handler(int irq, void *context, void *arg)
{
    static unsigned long tick = 0;
    enum timer_no timer_no;

    tick++;
    if (tick % 1000 == 0) {
        // up_nputs("\ntick", 5);
    }

    nxsched_process_timer();

    switch (irq) {
        case V3S_TIM0_IRQ: timer_no = TIM0; break;
        case V3S_TIM1_IRQ: timer_no = TIM1; break;
        case V3S_TIM2_IRQ: timer_no = TIM2; break;
    }

    v3s_timer_clear_irq(timer_no);

    return 0;
}



int v3s_timer_init(enum timer_no timer_no, uint32_t tick_per_seco)
{
    uint8_t irq;
    uint32_t count;
    volatile uint32_t ctrl_reg = 0;
    
    v3s_timer_clear_irq(timer_no);

    /* 重置定时器 */
    putreg32(0, V3S_TIMER_BASE + TIM_CTRL_REG_OFF(timer_no));

    count = V3S_EXTEREN_CLOCK_FREQ / 8 / tick_per_seco; //上面配置的8分频
    putreg32(count, V3S_TIMER_BASE + TIM_INTV_VALUE_REG_OFF(timer_no));

    ctrl_reg = _TIM_MODE_CONTINUOUS_ | _TIM_CTRL_CLK_PRE_SCALE_8_ |
               _TIM_CTRL_CLK_SRC_OSC24M_ | _TIM_CTRL_RELOAD_;
    putreg32(ctrl_reg, V3S_TIMER_BASE + TIM_CTRL_REG_OFF(timer_no));

    switch (timer_no) {
        case TIM0: irq = V3S_TIM0_IRQ; break;
        case TIM1: irq = V3S_TIM1_IRQ; break;
        case TIM2: irq = V3S_TIM2_IRQ; break;
    }

    v3s_timer_irq(timer_no, true);

    irq_attach(irq, v3s_timer_irq_handler, NULL);
    up_enable_irq(irq);

    /* Enable timer */
    ctrl_reg |= _TIM_CTRL_EN_;
    putreg32(ctrl_reg, V3S_TIMER_BASE + TIM_CTRL_REG_OFF(timer_no));

    return 0;
}