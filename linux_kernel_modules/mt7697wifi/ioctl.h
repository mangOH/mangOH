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

#ifndef _MT7697_IOCTL_H_
#define _MT7697_IOCTL_H_

#include <net/iw_handler.h>     /* New driver API */

static const long freq_list[] = { 2412, 2417, 2422, 2427, 2432, 2437, 2442,
                                  2447, 2452, 2457, 2462, 2467, 2472, 2484 };

#define FREQ_COUNT 			ARRAY_SIZE(freq_list)
#define SUPPORTED_WIRELESS_EXT          19

extern const struct iw_handler_def mt7697_wireless_hndlrs;

#endif
