/*
 * This file is part of mt7697
 *
 * Copyright (C) 2017 Sierra Wireless Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __MT7697_IO_H__
#define __MT7697_IO_H__

#include <linux/types.h>

/* Address is based on linker script used to build mt7697 code */
#define MT7697_IO_SLAVE_BUFFER_ADDRESS 			0x20000200

/* The first byte of every SPI write is always a Read/Write indicator */
#define MT7697_IO_CMD_WRITE 				0x80
#define MT7697_IO_CMD_READ 				0x00

/*
 * MediaTek MT7697 SPI slave registers
 */

/* Reg0 */
#define MT7697_IO_SLAVE_REG_READ_DATA_LOW   		0x00
#define MT7697_IO_SLAVE_REG_READ_DATA_HIGH  		0x02
/* Reg1 */
#define MT7697_IO_SLAVE_REG_WRITE_DATA_LOW  		0x04
#define MT7697_IO_SLAVE_REG_WRITE_DATA_HIGH 		0x06
/* Reg2 */
#define MT7697_IO_SLAVE_REG_BUS_ADDR_LOW    		0x08
#define MT7697_IO_SLAVE_REG_BUS_ADDR_HIGH   		0x0A
/* Reg3 */
#define MT7697_IO_SLAVE_REG_COMMAND         		0x0C
/* Reg4 */
#define MT7697_IO_SLAVE_REG_STATUS          		0x10
/* Reg5 */
#define MT7697_IO_SLAVE_REG_IRQ             		0x14
/* Reg6 */
#define MT7697_IO_SLAVE_REG_MAILBOX_S2M     		0x18
/* Reg7 */
#define MT7697_IO_SLAVE_REG_MAILBOX_M2S     		0x1C

/* Register fields */
#define MT7697_IO_COMMAND_REG_BUS_SIZE_OFFSET 		1
#define MT7697_IO_COMMAND_REG_BUS_SIZE_WIDTH 		2
#define MT7697_IO_COMMAND_REG_BUS_SIZE_VAL_WORD 	0x2

#define MT7697_IO_COMMAND_REG_RW_OFFSET 		0
#define MT7697_IO_COMMAND_REG_RW_WIDTH 			1
#define MT7697_IO_COMMAND_REG_RW_VAL_READ 		0
#define MT7697_IO_COMMAND_REG_RW_VAL_WRITE 		1

#define MT7697_IO_STATUS_REG_BUSY_OFFSET 		0
#define MT7697_IO_STATUS_REG_BUSY_WIDTH 		1
#define MT7697_IO_STATUS_REG_BUSY_VAL_IDLE 		0
#define MT7697_IO_STATUS_REG_BUSY_VAL_BUSY 		1

#define MT7697_IO_IRQ_REG_IRQ_STATUS_OFFSET 		0
#define MT7697_IO_IRQ_REG_IRQ_STATUS_WIDTH 		1
#define MT7697_IO_IRQ_REG_IRQ_STATUS_VAL_INACTIVE 	0
#define MT7697_IO_IRQ_REG_IRQ_STATUS_VAL_ACTIVE 	1

#define MT7697_IO_S2M_MAILBOX_REG_MAILBOX_OFFSET 	0
#define MT7697_IO_S2M_MAILBOX_REG_MAILBOX_WIDTH 	6

#define MT7697_IO_M2S_MAILBOX_REG_MAILBOX_OFFSET 	0
#define MT7697_IO_M2S_MAILBOX_REG_MAILBOX_WIDTH 	7

struct mt7697q_info;

int mt7697io_wr_m2s_mbx(struct mt7697q_info*, u8);
int mt7697io_rd_s2m_mbx(struct mt7697q_info*, u8*);
int mt7697io_clr_s2m_mbx(struct mt7697q_info*, u8);

int mt7697io_wr(struct mt7697q_info*, u32, const u32*, size_t);
int mt7697io_rd(struct mt7697q_info*, u32, u32*, size_t);

int mt7697io_trigger_intr(struct mt7697q_info*);

#endif
