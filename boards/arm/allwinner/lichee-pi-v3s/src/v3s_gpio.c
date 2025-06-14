#include <nuttx/config.h>
#include <stdbool.h>
#include "v3s-armv7a.h"
#include "v3s_gpio.h"
#include "arm_internal.h"
#include <nuttx/board.h>
#include <nuttx/config.h>
#include <nuttx/ioexpander/gpio.h>


#define _V3S_GET_CFG_REG_(GROUP, INDEX) \
                            ((GROUP * 0x24 + INDEX * 0X04) + V3S_PIO_BASE)
#define _V3S_GET_DAT_REG_(GROUP) \
                            ((GROUP * 0x24 + 0x10) + V3S_PIO_BASE)
#define _V3S_GET_DRV_REG_(GROUP, INDEX) \
                            ((GROUP * 0x24 + 0x14 + INDEX * 0X04) + V3S_PIO_BASE)
#define _V3S_GET_PULL_REG_(GROUP, INDEX) \
                            ((GROUP * 0x24 + 0x1C + INDEX * 0X04) + V3S_PIO_BASE)
#define _V3S_GET_INT_CFG_REG(GROUP, INDEX) \
                            ((0x200 + GROUP * 0x20 + INDEX * 0X04) + V3S_PIO_BASE)
#define _V3S_GET_INT_CTL_(GROUP)    ((0x200 + GROUP * 0x20 + 0x14) + V3S_PIO_BASE)
#define _V3S_GET_INT_STA_(GROUP)    ((0x200 + GROUP * 0x20 + 0x18) + V3S_PIO_BASE)
#define _V3S_GET_INT_DEB_(GROUP)    ((0x200 + GROUP * 0x20 + 0x1C) + V3S_PIO_BASE)


#define GET_ARRAY_ELEMENTS(x) (sizeof(x) / sizeof(x[0]))


/* 
   31   29    24   23  21   18  16             0
    |   |     |    |   |    |   |              |
    +--------------------------------------------+
    |   |     |    |   |    |   |    RESERVE     |
    +--------------------------------------------+
        |     |    |   |    |   |              
      GROUP  PIN   |  PULL  |   |
                   |       MUX  |
                 LEVEL         DRVING
\brief: parameter as follow:
    GROUP:  V3S_GPIOB...V3S_GPIOG
    PIN:    pin number
    LEVEL:  high level or low level
    PULL:   pullup or pulldown or disable-pull
    MUX:    pin multiplex function
    DRV:    drving
*/
#define GEN_PIN_CFG(GROUP, PIN, LEVEL, PULL, MUX, DRV) ((GROUP << 29) | (PIN << 24) | \
                   (LEVEL << 23) | (PULL << 21) | (MUX << 18) | (DRV << 16))

#define _GET_GROUP_FROM_CFG_(CFG)    ((CFG >> 29) & 0x07)
#define _GET_PIN_FROM_CFG_(CFG)      ((CFG >> 24) & 0x1F)
#define _GET_LEVEL_FROM_CFG_(CFG)    ((CFG >> 23) & 0x01)
#define _GET_PULL_FROM_CFG_(CFG)     ((CFG >> 21) & 0x03)
#define _GET_MUX_FROM_CFG_(CFG)      ((CFG >> 18) & 0x07)
#define _GET_DRV_FROM_CFG_(CFG)      ((CFG >> 16) & 0x03)


bool get_gpio_level(uint8_t gpio, uint8_t pin)
{
    uint32_t reg_dat;
    uint32_t reg_adr;

    reg_adr = _V3S_GET_DAT_REG_(gpio);
    reg_dat = getreg32(reg_adr);
    reg_dat &= (1 << pin);
    return reg_dat ? true : false;
}


void set_gpio_level(uint8_t gpio, uint8_t pin, bool level)
{
    uint32_t reg_dat;
    uint32_t reg_adr;

    reg_adr = _V3S_GET_DAT_REG_(gpio);
    reg_dat = getreg32(reg_adr);
    reg_dat &= ~(1 << pin);
    if (level) {
        reg_dat |= (1 << pin);
    } else {
        reg_dat &= ~(1 << pin);
    }
    putreg32(reg_dat, reg_adr);
    return;
}


void set_gpio_pull(uint8_t gpio, uint8_t pin, uint8_t pull)
{
    uint32_t offset;
    uint32_t reg_dat;
    uint32_t reg_adr;
    uint32_t reg_index;

    reg_index = pin / 8;
    reg_adr = _V3S_GET_PULL_REG_(gpio, reg_index);
    offset = (pin % 8) * 2;
    reg_dat = getreg32(reg_adr);
    reg_dat &= ~(0x03 << offset);
    reg_dat |= (pull << offset);
    putreg32(reg_dat, reg_adr);
    return;
}


void set_gpio_driv(uint8_t gpio, uint8_t pin, uint8_t driv)
{
    uint32_t offset;
    uint32_t reg_dat;
    uint32_t reg_adr;
    uint32_t reg_index;

    reg_index = pin / 8;
    reg_adr = _V3S_GET_DRV_REG_(gpio, reg_index);
    offset = (pin % 8) * 2;
    reg_dat = getreg32(reg_adr);
    reg_dat &= ~(0x03 << offset);
    reg_dat |= (driv << offset);
    putreg32(reg_dat, reg_adr);
    return;
}


void set_gpio_multiplex(uint32_t gpio, uint32_t pin, uint32_t multiplex)
{
    uint32_t offset;
    uint32_t reg_dat;
    uint32_t reg_adr;
    uint32_t reg_index;

    reg_index = pin / 8;
    reg_adr = _V3S_GET_CFG_REG_(gpio, reg_index);
    offset = (pin % 8) * 4;
    reg_dat = getreg32(reg_adr);
    reg_dat &= ~(0x7 << offset);
    reg_dat |= (multiplex << offset);
    putreg32(reg_dat, reg_adr);
    return;
}


/***********************************************************************
 * Driver Register
 ***********************************************************************/

struct v3s_pin_dev
{
    struct gpio_dev_s dev;
    uint32_t pin_cfg;
};


struct v3s_pin_dev output_pin_dev_s[] = {
    {
        { 0 },
        GEN_PIN_CFG(V3S_GPIOB, PIN(4), GPIO_LEVEL_HIGH, GPIO_PULL_DISABLE, GPIOB4_MUX_OUTPUT, GPIO_DRVING_3),
    },
};


struct v3s_pin_dev input_pin_dev_s[] = {
    {
        {0},
        GEN_PIN_CFG(V3S_GPIOB, PIN(5), GPIO_LEVEL_LOW, GPIO_PULL_UP, GPIOB5_MUX_INPUT, GPIO_DRVING_3)
    },
};


static int pin_read(struct gpio_dev_s *dev, bool *value)
{
    uint8_t pin;
    uint8_t group;
    uint32_t pin_cfg;
    struct v3s_pin_dev *v3s_gpio;

    v3s_gpio = (struct v3s_pin_dev *)dev;
    pin_cfg = v3s_gpio->pin_cfg;
    group = _GET_GROUP_FROM_CFG_(pin_cfg);
    pin = _GET_PIN_FROM_CFG_(pin_cfg);
    *value = get_gpio_level(group, pin);
    return OK;
}


static int pin_write(struct gpio_dev_s *dev, bool value)
{
    uint8_t pin;
    uint8_t group;
    uint32_t pin_cfg;
    struct v3s_pin_dev *v3s_gpio;

    v3s_gpio = (struct v3s_pin_dev *)dev;
    pin_cfg = v3s_gpio->pin_cfg;
    group = _GET_GROUP_FROM_CFG_(pin_cfg);
    pin = _GET_PIN_FROM_CFG_(pin_cfg);
    set_gpio_level(group, pin, value);
  return OK;
}


static const struct gpio_operations_s pin_input_ops =
{
  .go_read   = pin_read,
  .go_write  = NULL,
  .go_attach = NULL,
  .go_enable = NULL,
};


static const struct gpio_operations_s pin_output_ops =
{
  .go_read   = pin_read,
  .go_write  = pin_write,
  .go_attach = NULL,
  .go_enable = NULL,
};


void v3s_gpio_initialize(void)
{
    uint8_t group;
    uint8_t pin;
    uint8_t mux;
    uint8_t pull;
    uint8_t level;
    uint8_t drv;
    uint32_t j;
    uint32_t cfg;
    uint32_t pin_count;

    pin_count = 0;
    drv = GPIO_DRVING_3;

    for (j = 0; j < GET_ARRAY_ELEMENTS(output_pin_dev_s); j++) {

        output_pin_dev_s[j].dev.gp_pintype = GPIO_OUTPUT_PIN;
        output_pin_dev_s[j].dev.gp_ops = &pin_output_ops;
        gpio_pin_register(&output_pin_dev_s[j].dev, pin_count);

        cfg = output_pin_dev_s[j].pin_cfg;
        group = _GET_GROUP_FROM_CFG_(cfg);
        pin = _GET_PIN_FROM_CFG_(cfg);
        mux = _GET_MUX_FROM_CFG_(cfg);
        pull = _GET_PULL_FROM_CFG_(cfg);
        level = _GET_LEVEL_FROM_CFG_(cfg);
        set_gpio_multiplex(group, pin, mux);
        set_gpio_driv(group, pin, drv);
        set_gpio_pull(group, pin, pull);
        set_gpio_level(group, pin, level);
        pin_count++;
    }

    for (j = 0; j < GET_ARRAY_ELEMENTS(input_pin_dev_s); j++) {

        input_pin_dev_s[j].dev.gp_pintype = GPIO_INPUT_PIN;
        input_pin_dev_s[j].dev.gp_ops = &pin_input_ops;
        gpio_pin_register(&input_pin_dev_s[j].dev, pin_count);

        cfg = input_pin_dev_s[j].pin_cfg;
        group = _GET_GROUP_FROM_CFG_(cfg);
        pin = _GET_PIN_FROM_CFG_(cfg);
        mux = _GET_MUX_FROM_CFG_(cfg);
        pull = _GET_PULL_FROM_CFG_(cfg);
        level = _GET_LEVEL_FROM_CFG_(cfg);
        set_gpio_multiplex(group, pin, mux);
        set_gpio_driv(group, pin, drv);
        set_gpio_pull(group, pin, pull);
        set_gpio_level(group, pin, level);
        pin_count++;
    }
}