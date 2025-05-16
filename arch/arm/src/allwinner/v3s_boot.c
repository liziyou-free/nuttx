/****************************************************************************
 * arch/arm/src/qemu/qemu_boot.c
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
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include "arm_internal.h"
#include "v3s_boot.h"
#include "v3s_irq.h"
#include <nuttx/arch.h>
#include "arm_timer.h"
#include "v3s_memorymap.h"

#ifdef CONFIG_DEVICE_TREE
#  include <nuttx/fdt.h>
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static void v3s_arch_gpt_init (uint32_t freq)
{
  CP15_SET(CNTFRQ, freq);
  UP_ISB();
}


/****************************************************************************
 * Name: arm_boot
 *
 * Description:
 *   Complete boot operations started in arm_head.S
 *
 ****************************************************************************/
int v3s_setupmappings(void);
void up_nputs(FAR const char *str, size_t len);

void arm_boot(void)
{
  /* Perf init */

  up_perf_init(0);

  /* Init GTM clock frequncy */
  v3s_arch_gpt_init(V3S_GTM_FREQ);

  /* Set the page table for section */
 
  v3s_setupmappings();
  
  arm_fpuconfig();

#if defined(CONFIG_ARCH_HAVE_PSCI)
  // arm_psci_init("hvc");
#endif

#ifdef CONFIG_DEVICE_TREE
  fdt_register((const char *)0x40000000);
#endif

#ifdef USE_EARLYSERIALINIT
  /* Perform early serial initialization if we are going to use the serial
   * driver.
   */

  arm_earlyserialinit();
#endif

  /* Now we can enable all other CPUs.  The enabled CPUs will start execution
   * at __cpuN_start and, after very low-level CPU initialization has been
   * performed, will branch to arm_cpu_boot()
   * (see arch/arm/src/armv7-a/smp.h)
   */

  qemu_cpu_enable();
}
