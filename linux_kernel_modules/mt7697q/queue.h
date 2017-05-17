/*
 * Copyright (c) 2017 Sierra Wireless Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MT7697_QUEUE_H_
#define _MT7697_QUEUE_H_

#include <linux/types.h>
#include "queue_i.h"

#define MT7697_NUM_QUEUES			7

#define MT7697_QUEUE_FLAGS_IN_USE_OFFSET     	0
#define MT7697_QUEUE_FLAGS_IN_USE_WIDTH      	1

#define MT7697_QUEUE_FLAGS_DIR_OFFSET  		1
#define MT7697_QUEUE_FLAGS_DIR_WIDTH   		1

#define MT7697_QUEUE_FLAGS_NUM_WORDS_OFFSET 	16
#define MT7697_QUEUE_FLAGS_NUM_WORDS_WIDTH  	16

#define MT7697_QUEUE_DEBUG_DUMP_LIMIT 		1024

typedef int (*rx_hndlr)(void*);

struct mt7697q_data {
	u32 				flags;
    	u32 				base_addr;
    	u32 				rd_offset;
    	u32 				wr_offset;
} __attribute__((packed));

struct mt7697q_spec {
	struct mt7697q_data		data;
	struct mt7697q_info             *qinfo;
	void				*priv;
	rx_hndlr			rx_fcn;
	u8				ch;
};

struct mt7697q_info {
	struct mt7697q_spec 		queues[MT7697_NUM_QUEUES];
	u8 				txBuffer[sizeof(u32)];
	u8 				rxBuffer[sizeof(u16)];
	
	struct device			*dev;
	void 				*hw_priv;
        const struct mt7697spi_hw_ops   *hw_ops;

	struct mutex                    mutex;
	struct workqueue_struct 	*irq_workq;
	
//	struct work_struct              irq_work;
	struct delayed_work		irq_work;
	int 				irq;
	u8 				s2m_mbox;
	bool				slave_busy;
};

u8 mt7697q_busy(u16);
u8 mt7697q_get_s2m_mbox(u16);
u16 mt7697q_set_s2m_mbox(u8);
u8 mt7697q_get_m2s_mbox(u16);
u32 mt7697q_flags_get_in_use(u32);
u32 mt7697q_flags_get_dir(u32);

void* mt7697q_init(u8, void*, rx_hndlr);

size_t mt7697q_read(void*, u32*, size_t);
size_t mt7697q_write(void*, const u32*, size_t);

int mt7697q_pull_rd_ptr(void*);
int mt7697q_pull_wr_ptr(void*);

size_t mt7697q_get_capacity(const void*);
size_t mt7697q_get_num_words(const void*);
size_t mt7697q_get_free_words(const void*);

#endif
