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

#ifndef _MT7697_UART_H_
#define _MT7697_UART_H_

#include <linux/types.h>
#include <linux/fs.h>
#include "mt7697_i.h"
#include "uart_i.h"

#define MT7697_UART_DRVNAME    "mt7697serial"
#define MT7697_UART_DEVICE     "/dev/ttyHS0"
#define MT7697_UART_INVALID_FD NULL

#define mt7697_uart_shutdown_req mt7697_cmd_hdr
#define mt7697_uart_shutdown_rsp mt7697_rsp_hdr

enum mt7697_uart_cmd_types {
	MT7697_CMD_UART_SHUTDOWN_REQ = 0,
	MT7697_CMD_UART_SHUTDOWN_RSP,
};

struct mt7697_uart_info {
	struct platform_device *pdev;
	struct device	       *dev;

	char                   *dev_file;
	struct file            *fd_hndl;

	struct mutex           mutex;
	struct work_struct     rx_work;
	struct mt7697_rsp_hdr  rsp;
	rx_hndlr	       rx_fcn;
	void		       *rx_hndl;

	wait_queue_head_t      close_wq;
	atomic_t	       close;
};

#endif
