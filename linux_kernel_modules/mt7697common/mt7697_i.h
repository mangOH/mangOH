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

#ifndef _MT7697_I_H_
#define _MT7697_I_H_

#define LEN_TO_WORD(x)   ((x) / sizeof(u32) + ((x) % sizeof(u32) ? 1 : 0))
#define LEN32_ALIGNED(x) (LEN_TO_WORD(x) * sizeof(u32))

enum mt7697_cmd_grp {
	MT7697_CMD_GRP_QUEUE = 0,
	MT7697_CMD_GRP_UART,
	MT7697_CMD_GRP_80211,
	MT7697_CMD_GRP_BT,
};

struct mt7697_cmd_hdr {
	__be16			len;
	u8			grp;
	u8			type;
} __attribute__((__packed__, aligned(4)));

struct mt7697_rsp_hdr {
	struct mt7697_cmd_hdr	cmd;
	__be32			result;
} __attribute__((__packed__, aligned(4)));

typedef int (*rx_hndlr)(const struct mt7697_rsp_hdr*, void*);
typedef int (*notify_tx_hndlr)(void*, u32);

struct mt7697_if_ops {
	int (*init)(u8, u8, void*, notify_tx_hndlr, rx_hndlr, void**, void**);
	int (*shutdown)(void**, void**);
	void (*unblock_writer)(void*);
	void* (*open)(rx_hndlr, void*);
	int (*close)(void*);
	size_t (*read)(void*, u32*, size_t);
	size_t (*write)(void*, const u32*, size_t);
};

#endif
