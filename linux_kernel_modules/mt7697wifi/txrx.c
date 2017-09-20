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

#include <linux/etherdevice.h>
#include "common.h"
#include "core.h"

int mt7697_data_tx(struct sk_buff *skb, struct net_device *ndev)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	struct mt7697_cookie *cookie;
	int ret = 0;

	dev_dbg(cfg->dev, 
		"%s(): tx cookie cnt(%u) skb(0x%p), data(0x%p), len(%u)\n", 
		__func__, cfg->cookie_count, skb, skb->data, skb->len);

	if (!test_bit(CONNECTED, &vif->flags)) {
		dev_warn(cfg->dev, "%s(): interface not associated\n", 
			__func__);
		ret = -EAGAIN;
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s(): headroom skb/needed(%u/%u)\n", 
		__func__, skb_headroom(skb), ndev->needed_headroom);
	if (skb_headroom(skb) < ndev->needed_headroom) {
		struct sk_buff *tmp_skb = skb;

		skb = skb_realloc_headroom(skb, ndev->needed_headroom);
		kfree_skb(tmp_skb);
		if (skb == NULL) {
			dev_dbg(cfg->dev, "%s(): tx dropped\n", __func__);
			goto cleanup;
		}
	}

	cookie = mt7697_alloc_cookie(cfg);
	if (cookie == NULL) {
		ret = -ENOMEM;
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s(): tx cookie/cnt(0x%p/%u)\n", 
		__func__, cookie, cfg->cookie_count);
	cookie->skb = skb;

	ret = queue_work(cfg->tx_workq, &cfg->tx_work);
	if (ret < 0) {
		dev_err(cfg->dev, "%s(): queue_work() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	return NETDEV_TX_OK;

cleanup:
	vif->net_stats.tx_dropped++;
	vif->net_stats.tx_aborted_errors++;

	dev_kfree_skb(skb);
	return ret;
}

void mt7697_tx_work(struct work_struct *work)
{
        struct mt7697_cfg80211_info *cfg = container_of(work, 
		struct mt7697_cfg80211_info, tx_work);
	struct sk_buff_head skb_queue;
	struct ieee80211_hdr *hdr;
	int ret;

	skb_queue_head_init(&skb_queue);

	dev_dbg(cfg->dev, "%s(): tx cookie cnt(%u)\n", 
		__func__, cfg->cookie_count);
	while (cfg->cookie_count < MT7697_MAX_COOKIE_NUM) {
		struct mt7697_vif *vif;
		struct mt7697_cookie *cookie = &cfg->cookie_mem[cfg->cookie_count];
		if (!cookie->skb)
			break;

		vif = netdev_priv(cookie->skb->dev);
		WARN_ON(!vif);

       		/* validate length for ether packet */
        	if (cookie->skb->len < sizeof(*hdr)) {
			dev_err(cfg->dev, "%s(): invalid skb len(%u < %u)\n", 
				__func__, cookie->skb->len, sizeof(*hdr));

			vif->net_stats.tx_errors++;
                	ret = -EINVAL;
                	goto cleanup;
        	}

		ret = mt7697_wr_tx_raw_packet(cfg, cookie->skb->data, 
			cookie->skb->len);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_wr_tx_raw_packet() failed(%d)\n", 
				__func__, ret);
			vif->net_stats.tx_errors++;
			goto cleanup;
		}

		vif->net_stats.tx_packets++;
		vif->net_stats.tx_bytes += cookie->skb->len;

		__skb_queue_tail(&skb_queue, cookie->skb);
		mt7697_free_cookie(cfg, cookie);

		dev_dbg(cfg->dev, "%s(): tx cookie cnt(%u)\n", 
			__func__, cfg->cookie_count);
		if (cfg->cookie_count < MT7697_MAX_COOKIE_NUM) 
			cookie = &cfg->cookie_mem[cfg->cookie_count];
	}

	__skb_queue_purge(&skb_queue);

cleanup:
	return;
}

int mt7697_rx_data(struct mt7697_cfg80211_info *cfg, u32 len, u32 if_idx)
{
	struct mt7697_vif *vif;
	struct sk_buff *skb = NULL;
	struct ieee80211_hdr *hdr;
	int ret = 0;

	vif = mt7697_get_vif_by_idx(cfg, if_idx);
	if (!vif) {
		dev_err(cfg->dev, "%s(): mt7697_get_vif_by_idx(%u) failed\n",
			    __func__, if_idx);
		ret = -EINVAL;
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s(): vif(%u)\n", __func__, vif->fw_vif_idx);
	if (!(vif->ndev->flags & IFF_UP)) {
		dev_warn(cfg->dev, "%s(): net device NOT up\n", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

	if ((len < sizeof(*hdr)) || (len > IEEE80211_MAX_FRAME_LEN)) {
		dev_warn(cfg->dev, "%s(): invalid Rx frame size(%u)\n", 
			__func__, len);
		vif->net_stats.rx_length_errors++;
		ret = -EINVAL;
		goto cleanup;
	}

	skb = alloc_skb(len, GFP_KERNEL);
	if (!skb) {
		dev_err(cfg->dev, "%s(): alloc_skb() failed\n", __func__);
		ret = -ENOMEM;
		goto cleanup;
	}

	skb_put(skb, len);
        memcpy(skb->data, cfg->rx_data, len);
	skb->dev = vif->ndev;

	vif->net_stats.rx_packets++;
	vif->net_stats.rx_bytes += len;

	skb->protocol = eth_type_trans(skb, skb->dev);
	dev_dbg(cfg->dev, "%s(): rx frame protocol(%u) type(%u)\n", 
		__func__, skb->protocol, skb->pkt_type);

	ret = netif_rx_ni(skb);
	if (ret != NET_RX_SUCCESS) {
		if (ret == NET_RX_DROP) {
			dev_warn(cfg->dev, "%s(): rx frame dropped\n", __func__);
			vif->net_stats.rx_dropped++;
			ret = 0;
			goto cleanup;
		}

		dev_err(cfg->dev, "%s(): netif_rx_ni() failed(%d)\n", __func__, ret);
		goto cleanup;
	}

cleanup:
	if (ret < 0) {
		vif->net_stats.rx_errors++;
		if (skb) dev_kfree_skb(skb);
	}

	return ret;
}
