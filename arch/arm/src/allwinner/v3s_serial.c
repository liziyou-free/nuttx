/***************************************************************************
 * arch/arm/src/qemu/qemu_serial.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ***************************************************************************/

/***************************************************************************
 * Included Files
 ***************************************************************************/

#include <nuttx/serial/uart_16550.h>

#include "arm_internal.h"

#ifdef CONFIG_16550_UART

/***************************************************************************
 * Public Functions
 ***************************************************************************/


/***************************************************************************
 * Name: arm_earlyserialinit
 *
 * Description:
 *   see arm_internal.h
 *
 ***************************************************************************/

void arm_earlyserialinit(void)
{
  /* Enable the console UART.  The other UARTs will be initialized if and
   * when they are first opened.
   */

  u16550_earlyserialinit();
}

/***************************************************************************
 * Name: arm_serialinit
 *
 * Description:
 *   see arm_internal.h
 *
 ***************************************************************************/
void up_nputs(FAR const char *str, size_t len);
int v3s_timer_init(uint32_t timer_no, uint32_t tick_per_seco);

void arm_serialinit(void)
{
  u16550_serialinit();
  v3s_timer_init(1, 1000);

  up_nputs("\r\nInit arm timer ...", 21);
  up_nputs("\r\nInit 16650 uart ...", 22);
}

#endif /* CONFIG_16550_UART */
