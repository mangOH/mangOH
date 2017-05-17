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

#include "core.h"

int mt7697_data_tx(struct sk_buff *skb, struct net_device *ndev)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	struct mt7697_cookie *cookie;
	int ret;

	dev_dbg(cfg->dev, "%s: tx cookie cnt(%u) skb(0x%p), data(0x%p), len(%u)\n", 
		__func__, cfg->cookie_count, skb, skb->data, skb->len);

	if (!test_bit(CONNECTED, &vif->flags)) {
		dev_warn(cfg->dev, "%s: interface not associated\n", __func__);
		ret = -EAGAIN;
		goto fail_tx;
	}

	cookie = mt7697_alloc_cookie(cfg);
	if (cookie == NULL) {
		ret = -ENOMEM;
		goto fail_tx;
	}

	dev_dbg(cfg->dev, "%s: tx cookie/cnt(0x%p/%u)\n", 
		__func__, cookie, cfg->cookie_count);
	cookie->skb = skb;
	schedule_work(&cfg->tx_work);
	return 0;

fail_tx:
	dev_kfree_skb(skb);
	return ret;
}

void mt7697_tx_work(struct work_struct *work)
{
        struct mt7697_cfg80211_info *cfg = container_of(work, 
		struct mt7697_cfg80211_info, tx_work);
	struct mt7697_cookie *cookie = &cfg->cookie_mem[cfg->cookie_count];
	int ret;

	dev_dbg(cfg->dev, "%s: tx work cookie/cnt(0x%p/%u)\n", 
		__func__, cookie, cfg->cookie_count);
	while (cfg->cookie_count < MT7697_MAX_COOKIE_NUM) {
		print_hex_dump(KERN_DEBUG, DRVNAME" <-- Tx ", DUMP_PREFIX_OFFSET, 
			16, 1, cookie->skb->data, cookie->skb->len, 0);

		ret = mt7697_send_tx_raw_packet(cfg, cookie->skb->data, 
			cookie->skb->len);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_send_tx_raw_packet() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		dev_kfree_skb(cookie->skb);
		mt7697_free_cookie(cfg, cookie);
		dev_dbg(cfg->dev, "%s: tx cookie cnt(%u)\n", 
			__func__, cfg->cookie_count);
		if (cfg->cookie_count < MT7697_MAX_COOKIE_NUM) 
			cookie = &cfg->cookie_mem[cfg->cookie_count];
	}

cleanup:
	return;
}
