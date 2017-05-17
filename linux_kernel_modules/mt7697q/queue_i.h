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

#ifndef _MT7697_QUEUE_I_H_
#define _MT7697_QUEUE_I_H_

#define MT7697_QUEUE_LEN_TO_WORD(x)		((x) / sizeof(u32) + \
						 ((x) % sizeof(u32) ? 1:0))

enum mt7697q_dir
{
    MT7697_QUEUE_DIR_MASTER_TO_SLAVE = 0,
    MT7697_QUEUE_DIR_SLAVE_TO_MASTER,
};

typedef int (*rx_hndlr)(void*);

struct mt7697q_if_ops {
	void* (*init)(u8, void*, rx_hndlr);
	size_t (*read)(void*, u32*, size_t);
	size_t (*write)(void*, const u32*, size_t);
	int (*pull_rd_ptr)(void*);
	int (*pull_wr_ptr)(void*);
	size_t (*capacity)(const void*);
	size_t (*num_wr)(const void*);
	size_t (*num_rd)(const void*);
	void (*reset)(void*);
	void (*enable_irq)(void*);
	void (*disable_irq)(void*);
};

u8 mt7697q_busy(u16);
u8 mt7697q_get_s2m_mbox(u16);
u16 mt7697q_set_s2m_mbox(u8);
u8 mt7697q_get_m2s_mbox(u16);
u16 mt7697q_set_m2s_mbox(u8);
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
