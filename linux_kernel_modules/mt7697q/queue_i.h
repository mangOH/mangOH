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

#define LEN32_ALIGNED(x)		(((x) / sizeof(u32) + \
					 ((x) % sizeof(u32) ? 1:0)) * sizeof(u32))
#define LEN_TO_WORD(x)			((x) / sizeof(u32) + \
					 ((x) % sizeof(u32) ? 1:0))

#define mt7697_queue_init_rsp		mt7697q_rsp_hdr
#define mt7697_queue_unused_rsp		mt7697q_rsp_hdr
#define mt7697_queue_reset_rsp		mt7697q_rsp_hdr

enum mt7697q_dir
{
    	MT7697_QUEUE_DIR_M2S = 0,
    	MT7697_QUEUE_DIR_S2M,
};

enum mt7697q_cmd_grp {
	MT7697_CMD_GRP_QUEUE = 0,
	MT7697_CMD_GRP_80211,
	MT7697_CMD_GRP_BT,
};

enum mt7697q_cmd_types {
	MT7697_CMD_QUEUE_INIT = 0,
	MT7697_CMD_QUEUE_INIT_RSP,
	MT7697_CMD_QUEUE_UNUSED,
	MT7697_CMD_QUEUE_UNUSED_RSP,
	MT7697_CMD_QUEUE_RESET,
	MT7697_CMD_QUEUE_RESET_RSP,
};

struct mt7697q_cmd_hdr {
	__be16				len;
	u8				grp;
	u8				type;
} __attribute__((__packed__, aligned(4)));

struct mt7697q_rsp_hdr {
	struct mt7697q_cmd_hdr		cmd;
	__be32				result;
} __attribute__((__packed__, aligned(4)));

struct mt7697_queue_init_req {
	struct mt7697q_cmd_hdr		cmd;
    	__be32				m2s_ch;
    	__be32				s2m_ch;
} __attribute__((packed, aligned(4)));

struct mt7697_queue_unused_req {
	struct mt7697q_cmd_hdr		cmd;
    	__be32				m2s_ch;
    	__be32				s2m_ch;
} __attribute__((packed, aligned(4)));

struct mt7697_queue_reset_req {
	struct mt7697q_cmd_hdr		cmd;
    	__be32				m2s_ch;
    	__be32				s2m_ch;
} __attribute__((packed, aligned(4)));

typedef int (*rx_hndlr)(const struct mt7697q_rsp_hdr*, void*);

struct mt7697q_if_ops {
	int (*init)(u8, u8, void*, rx_hndlr, void**, void**);
	size_t (*read)(void*, u32*, size_t);
	size_t (*write)(void*, const u32*, size_t);
	void (*reset)(void*);
	void (*enable_irq)(void*);
	void (*disable_irq)(void*);
};

u32 mt7697q_flags_get_in_use(u32);
u32 mt7697q_flags_get_dir(u32);

int mt7697q_init(u8, u8, void*, rx_hndlr, void**, void**);
size_t mt7697q_read(void*, u32*, size_t);
size_t mt7697q_write(void*, const u32*, size_t);

int mt7697q_send_reset(void*, void*);

#endif
