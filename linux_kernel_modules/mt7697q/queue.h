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
#include <linux/interrupt.h>
#include "queue_i.h"

#define MT7697_NUM_QUEUES			6

#define MT7697_QUEUE_FLAGS_IN_USE_OFFSET     	0
#define MT7697_QUEUE_FLAGS_IN_USE_WIDTH      	1

#define MT7697_QUEUE_FLAGS_DIR_OFFSET  		1
#define MT7697_QUEUE_FLAGS_DIR_WIDTH   		1

#define MT7697_QUEUE_FLAGS_NUM_WORDS_OFFSET 	16
#define MT7697_QUEUE_FLAGS_NUM_WORDS_WIDTH  	16

#define MT7697_QUEUE_DEBUG_DUMP_LIMIT 		1024

struct mt7697q_data {
	u32 flags;
	u32 base_addr;
	u16 rd_offset;
	u16 reserved1;
	u16 wr_offset;
	u16 reserved2;
};

struct mt7697q_spec {
	struct mt7697q_data             data;
	struct mt7697q_info             *qinfo;
	void                            *priv;
	notify_tx_hndlr                 notify_tx_fcn;
	rx_hndlr                        rx_fcn;
	u8                              ch;
};

struct mt7697q_info {
	struct mt7697q_spec             queues[MT7697_NUM_QUEUES];
	struct mt7697_rsp_hdr           rsp;

	struct device                   *dev;
	void                            *hw_priv;
	const struct mt7697spi_hw_ops   *hw_ops;

	struct mutex                    mutex;
	struct workqueue_struct         *irq_workq;

	struct work_struct              irq_work;
	struct delayed_work             irq_delayed_work;
	atomic_t                        blocked_writer;
	int                             gpio_pin;
	int                             irq;
};

void mt7697q_irq_delayed_work(struct work_struct*);
void mt7697q_irq_work(struct work_struct*);
irqreturn_t mt7697q_isr(int, void*);

int mt7697q_blocked_writer(const struct mt7697q_spec*);
size_t mt7697q_get_free_words(const struct mt7697q_spec*);
int mt7697q_proc_data(struct mt7697q_spec*);
int mt7697q_get_s2m_mbx(struct mt7697q_info*, u8*);

#endif
