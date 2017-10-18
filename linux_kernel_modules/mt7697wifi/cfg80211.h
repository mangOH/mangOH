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

#ifndef _MT7697_WL_CFG80211_H_
#define _MT7697_WL_CFG80211_H_

#define MT7697_DEFAULT_BG_SCAN_PERIOD 60

struct mt7697_cfg80211_info;
struct mt7697_vif;

int mt7697_cfg80211_new_sta(struct mt7697_vif*, const u8*);
int mt7697_cfg80211_del_sta(struct mt7697_vif*, const u8*);
int mt7697_cfg80211_connect_event(struct mt7697_vif*, const u8*, u32);
struct mt7697_cfg80211_info *mt7697_cfg80211_create(void);
int mt7697_cfg80211_stop(struct mt7697_vif*);
int mt7697_cfg80211_get_params(struct mt7697_cfg80211_info*);
int mt7697_cfg80211_init(struct mt7697_cfg80211_info*);
void mt7697_cfg80211_cleanup(struct mt7697_cfg80211_info*);
void mt7697_cfg80211_destroy(struct mt7697_cfg80211_info*);

#endif
