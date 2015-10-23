/*
 * Pacifica Cavecreek GPIO LED driver head file
 *
 * Copyright (c) 2011,  Juniper Networks
 *
 * This file is licensed under GPLv2.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
*/

/* Supports:
 * Pacifica Cavecreek GPIO LEDs
 */
#ifndef _PACIFICA_MXC_GPIO_H
#define _PACIFICA_MXC_GPIO_H

#define CC_LPC_CFG_GPIO_BASE 0x48

#define CC_LPC_GPIO_USE_SEL  0x00
#define CC_LPC_GPIO_SEL      0x04
#define CC_LPC_GPIO_LVL      0x0C

#define CC_LPC_GPIO_USE_SEL2 0x30
#define CC_LPC_GPIO_SEL2     0x34
#define CC_LPC_GPIO_LVL2     0x38

#define LPC_DEV_ID           0x2310

#define GPIO_52_SHIFT        20
#define GPIO_50_SHIFT        18

#define GPIO_27_SHIFT        27
#define GPIO_25_SHIFT        25

#endif /* _PACIFICA_MXC_GPIO_H */
