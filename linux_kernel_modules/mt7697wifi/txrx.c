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

int mt7697_notify_tx(void* priv, u32 free)
{
	struct mt7697_cfg80211_info *cfg = (struct mt7697_cfg80211_info*)priv;
	int ret = 0;

	spin_lock_bh(&cfg->tx_skb_list_lock);
	if (!list_empty(&cfg->tx_skb_list)) {
		struct mt7697_tx_pkt *tx_pkt = list_entry(&cfg->tx_skb_list,
			struct mt7697_tx_pkt, next);
		if (tx_pkt->skb->len >= free) {
			cfg->hif_ops->unblock_writer(cfg->txq_hdl);
			ret = queue_work(cfg->tx_workq, &cfg->tx_work);
			if (ret < 0) {
				dev_err(cfg->dev,
					"%s(): queue_work() failed(%d)\n",
					__func__, ret);
				goto cleanup;
			}
		}
	}

cleanup:
	spin_unlock_bh(&cfg->tx_skb_list_lock);
	return ret;
}

int mt7697_data_tx(struct sk_buff *skb, struct net_device *ndev)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	struct mt7697_tx_pkt *tx_skb;
	int ret = 0;

	dev_dbg(cfg->dev, "%s(): tx len(%u)\n", __func__, skb->len);

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

	tx_skb = &cfg->tx_skb_pool[atomic_read(&cfg->tx_skb_pool_idx)];
	if (tx_skb->skb) {
		dev_warn(cfg->dev, "%s(): tx pool full\n",
			__func__);
		ret = NETDEV_TX_BUSY;
		goto cleanup;
	}

	tx_skb->skb = skb;
	dev_dbg(cfg->dev, "%s(): tx pkt(%u/%p)\n",
		__func__, atomic_read(&cfg->tx_skb_pool_idx), tx_skb->skb);

	atomic_inc(&cfg->tx_skb_pool_idx);
	if (atomic_read(&cfg->tx_skb_pool_idx) >= MT7697_TX_PKT_POOL_LEN)
		atomic_set(&cfg->tx_skb_pool_idx, 0);

	spin_lock_bh(&cfg->tx_skb_list_lock);
	list_add_tail(&tx_skb->next, &cfg->tx_skb_list);
	spin_unlock_bh(&cfg->tx_skb_list_lock);

	ret = queue_work(cfg->tx_workq, &cfg->tx_work);
	if (ret < 0) {
		dev_err(cfg->dev, "%s(): queue_work() failed(%d)\n",
			__func__, ret);
		ret = NETDEV_TX_BUSY;
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
	struct mt7697_tx_pkt *tx_pkt, *tx_pkt_next = NULL;
	struct ieee80211_hdr *hdr;
	int ret;

	list_for_each_entry_safe(tx_pkt, tx_pkt_next, &cfg->tx_skb_list, next) {
		struct mt7697_vif *vif = netdev_priv(tx_pkt->skb->dev);
		WARN_ON(!vif);

		/* validate length for ether packet */
		if (tx_pkt->skb->len < sizeof(*hdr)) {
			dev_err(cfg->dev, "%s(): invalid skb len(%u < %u)\n",
				__func__, tx_pkt->skb->len, sizeof(*hdr));
			vif->net_stats.tx_errors++;
		}

		ret = mt7697_wr_tx_raw_packet(cfg, tx_pkt->skb->data,
			tx_pkt->skb->len);
		if (ret < 0) {
			dev_dbg(cfg->dev,
				"%s(): mt7697_wr_tx_raw_packet() failed(%d)\n",
				__func__, ret);
			vif->net_stats.tx_errors++;
		}

		dev_dbg(cfg->dev, "%s(): tx complete pkt(%p)\n", __func__,
			tx_pkt->skb);
		vif->net_stats.tx_packets++;
		vif->net_stats.tx_bytes += tx_pkt->skb->len;

		spin_lock_bh(&cfg->tx_skb_list_lock);
		list_del(&tx_pkt->next);
		spin_unlock_bh(&cfg->tx_skb_list_lock);

		__skb_queue_tail(&cfg->tx_skb_queue, tx_pkt->skb);
		tx_pkt->skb = NULL;
	}

	__skb_queue_purge(&cfg->tx_skb_queue);
}

void mt7697_tx_stop(struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_tx_pkt *tx_pkt, *tx_pkt_next = NULL;

	list_for_each_entry_safe(tx_pkt, tx_pkt_next, &cfg->tx_skb_list, next) {
		struct mt7697_vif *vif = netdev_priv(tx_pkt->skb->dev);
		WARN_ON(!vif);

		dev_dbg(cfg->dev, "%s(): tx drop pkt(%p)\n", __func__,
			tx_pkt->skb);
		vif->net_stats.tx_dropped++;

		spin_lock_bh(&cfg->tx_skb_list_lock);
		list_del(&tx_pkt->next);
		spin_unlock_bh(&cfg->tx_skb_list_lock);
	}
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

	skb = dev_alloc_skb(len);
	if (!skb) {
		dev_err(cfg->dev, "%s(): dev_alloc_skb() failed\n", __func__);
		ret = -ENOMEM;
		goto cleanup;
	}

	skb_put(skb, len);
	memcpy(skb->data, cfg->rx_data, len);
	skb->dev = vif->ndev;

	vif->net_stats.rx_packets++;
	vif->net_stats.rx_bytes += len;

	skb->protocol = eth_type_trans(skb, skb->dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY;
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

		dev_err(cfg->dev, "%s(): netif_rx_ni() failed(%d)\n", __func__,
			ret);
		goto cleanup;
	}

cleanup:
	if (ret < 0) {
		vif->net_stats.rx_dropped++;
		vif->net_stats.rx_errors++;
		if (skb) dev_kfree_skb(skb);
	}

	return ret;
}
