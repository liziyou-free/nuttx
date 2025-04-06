/****************************************************************************
 * arch/arm/src/qemu/qemu_memorymap.h
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

#ifndef __ARCH_ARM_SRC_QEMU_QEMU_MEMORYMAP_H
#define __ARCH_ARM_SRC_QEMU_QEMU_MEMORYMAP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <arch/chip/chip.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* AllWinner-V3S Memory Map ********************************************/
#define V3S_IO_SECTION_PA            0x00000000  /* 0x00000000-0x3fffffff */
#define V3S_IO_SECTION_VA            0x00000000  /* 0x00000000-0x3fffffff */
#define V3S_IO_SECTION_SIZE          0x40000000

#define V3S_CODE_SECTION_PA          0x40000000  /* 0x40000000-0x407fffff 16MB */
#define V3S_CODE_SECTION_VA          0x40000000  /* 0x40000000-0x407fffff 16MB */
#define V3S_CODE_SECTION_SIZE        0x01000000

#define V3S_DATA_SECTION_PA          0x41000000  /* 0x41000000-0x43ffffff 48MB */
#define V3S_DATA_SECTION_VA          0x41000000  /* 0x41000000-0x43ffffff 48MB */
#define V3S_DATAs_SECTION_SIZE       0x03000000

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

int v3s_setupmappings(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */
#endif /* __ARCH_ARM_SRC_QEMU_QEMU_MEMORYMAP_H */
