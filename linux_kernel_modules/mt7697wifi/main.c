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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <net/cfg80211.h>
#include "queue_i.h"
#include "common.h"
#include "core.h"
#include "cfg80211.h"

static struct platform_device *pdev = NULL;

static int mt7697_open(struct net_device *ndev)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);

	dev_dbg(cfg->dev, "%s(): open net device\n", __func__);

	set_bit(WLAN_ENABLED, &vif->flags);

	if (test_bit(CONNECTED, &vif->flags)) {
		netif_carrier_on(ndev);
		netif_wake_queue(ndev);
	} else
		netif_carrier_off(ndev);

	return 0;
}

static int mt7697_stop(struct net_device *ndev)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);

	dev_dbg(cfg->dev, "%s(): stop net device\n", __func__);

	netif_stop_queue(ndev);
	mt7697_cfg80211_stop(vif);
	clear_bit(WLAN_ENABLED, &vif->flags);

	return 0;
}

static struct net_device_stats *mt7697_get_stats(struct net_device *dev)
{
	struct mt7697_vif *vif = netdev_priv(dev);
	return &vif->net_stats;
}

static int mt7697_set_features(struct net_device *ndev,
			       netdev_features_t features)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s(): net device set features\n", __func__);
	return 0;
}

static void mt7697_set_multicast_list(struct net_device *ndev)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	bool mc_all_on = false;
	int mc_count = netdev_mc_count(ndev);

	dev_dbg(cfg->dev, "%s(): net device set multicast\n", __func__);

	/* Enable multicast-all filter. */
	mc_all_on = !!(ndev->flags & IFF_PROMISC) ||
		    !!(ndev->flags & IFF_ALLMULTI) ||
		    !!(mc_count > MT7697_MAX_MC_FILTERS_PER_LIST);

	if (mc_all_on)
		set_bit(NETDEV_MCAST_ALL_ON, &vif->flags);
	else
		clear_bit(NETDEV_MCAST_ALL_ON, &vif->flags);

	if (!(ndev->flags & IFF_MULTICAST)) {
		mc_all_on = false;
		set_bit(NETDEV_MCAST_ALL_OFF, &vif->flags);
	} else {
		clear_bit(NETDEV_MCAST_ALL_OFF, &vif->flags);
	}

	/* Enable/disable "multicast-all" filter*/
	dev_dbg(cfg->dev, "%s(): %s multicast-all filter\n",
		__func__, mc_all_on ? "enable" : "disable");
}

static void mt7697_init_hw_start(struct work_struct *work)
{
        struct mt7697_cfg80211_info *cfg = container_of(work, 
		struct mt7697_cfg80211_info, init_work);
	int err;

	dev_dbg(cfg->dev, "%s(): init mt7697 queue(%u/%u)\n", 
		__func__, MT7697_MAC80211_QUEUE_TX, MT7697_MAC80211_QUEUE_RX);
	err = cfg->hif_ops->init(MT7697_MAC80211_QUEUE_TX, 
		MT7697_MAC80211_QUEUE_RX, cfg, mt7697_proc_80211cmd, 
		&cfg->txq_hdl, &cfg->rxq_hdl);
	if (err < 0) {
		dev_err(cfg->dev, "%s(): queue(%u) init() failed(%d)\n",
			__func__, MT7697_MAC80211_QUEUE_TX, err);
		goto failed;
	}

	err = mt7697_wr_cfg_req(cfg);
	if (err < 0) {
		dev_err(cfg->dev, "%s(): mt7697_wr_cfg_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

	err = mt7697_wr_get_wireless_mode_req(cfg, MT7697_PORT_STA);
	if (err < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_get_wireless_mode_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

	err = mt7697_wr_get_rx_filter_req(cfg);
	if (err < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_get_rx_filter_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

	err = mt7697_wr_get_smart_conn_filter_req(cfg);
	if (err < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_get_smart_conn_filter_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

	err = mt7697_wr_get_listen_interval_req(cfg);
	if (err < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_get_listen_interval_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

	err = mt7697_wr_get_radio_state_req(cfg);
	if (err < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_get_radio_state_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

	err = mt7697_wr_get_psk_req(cfg, MT7697_PORT_STA);
	if (err < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_get_psk_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

	err = mt7697_wr_mac_addr_req(cfg, MT7697_PORT_STA);
	if (err < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_mac_addr_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

failed:
	return;
}

static const struct net_device_ops mt7697_netdev_ops = {
        .ndo_open               = mt7697_open,
        .ndo_stop               = mt7697_stop,
        .ndo_start_xmit         = mt7697_data_tx,
	.ndo_get_stats          = mt7697_get_stats,
        .ndo_set_features       = mt7697_set_features,
        .ndo_set_rx_mode        = mt7697_set_multicast_list,
};

void mt7697_init_netdev(struct net_device *dev)
{
        dev->netdev_ops = &mt7697_netdev_ops;
        dev->destructor = free_netdev;
        dev->watchdog_timeo = MT7697_TX_TIMEOUT;
        dev->needed_headroom = sizeof(struct ieee80211_hdr) + sizeof(struct mt7697_llc_snap_hdr);
        dev->hw_features |= NETIF_F_HW_CSUM | NETIF_F_RXCSUM;
}

static const struct mt7697q_if_ops if_ops = {
	.init		= mt7697q_init,
	.read		= mt7697q_read,
	.write		= mt7697q_write,
};

static void mt7697_cookie_init(struct mt7697_cfg80211_info *cfg)
{
	u32 i;

	cfg->cookie_list = NULL;
	cfg->cookie_count = 0;

	memset(cfg->cookie_mem, 0, sizeof(cfg->cookie_mem));

	for (i = 0; i < MT7697_MAX_COOKIE_NUM; i++)
		mt7697_free_cookie(cfg, &cfg->cookie_mem[i]);
}

static void mt7697_cookie_cleanup(struct mt7697_cfg80211_info *cfg)
{
	cfg->cookie_list = NULL;
	cfg->cookie_count = 0;
}

static int mt7697_probe(struct platform_device *pdev)
{	
	struct wiphy *wiphy;
	struct mt7697_cfg80211_info *cfg;
	int err = 0;

	dev_dbg(&pdev->dev, "%s(): probe\n", __func__);
	cfg = mt7697_cfg80211_create();
	if (!cfg) {
                dev_err(&pdev->dev, 
			"%s(): mt7697_cfg80211_create() failed()\n",
			__func__);
		err = -ENOMEM;
		goto failed;
        }

	sema_init(&cfg->sem, 1);
	INIT_WORK(&cfg->init_work, mt7697_init_hw_start);
	INIT_WORK(&cfg->tx_work, mt7697_tx_work);

	spin_lock_init(&cfg->vif_list_lock);
	INIT_LIST_HEAD(&cfg->vif_list);

	cfg->hif_ops = &if_ops;
	cfg->dev = &pdev->dev;
	mt7697_cookie_init(cfg);

	err = mt7697_cfg80211_init(cfg);
	if (err < 0) {
                dev_err(&pdev->dev, 
			"%s(): mt7697_cfg80211_init() failed(%d)\n", 
			__func__, err);
		goto failed;
        }

	platform_set_drvdata(pdev, cfg);
	schedule_work(&cfg->init_work);
		
failed:
	if (err < 0) {
		if (wiphy) wiphy_free(wiphy);
		platform_set_drvdata(pdev, NULL);
	}

	return err;
}

static int mt7697_remove(struct platform_device *pdev)
{
	struct mt7697_cfg80211_info *cfg = platform_get_drvdata(pdev);
	int ret;

	ret = mt7697q_send_reset(cfg->txq_hdl, cfg->rxq_hdl);
	if (ret < 0) {
                dev_err(&pdev->dev, "%s(): mt7697q_send_reset() failed(%d)\n", 
			__func__, ret);
		goto failed;
        }

	mt7697_cfg80211_cleanup(cfg);
	mt7697_cfg80211_destroy(cfg);
	mt7697_cookie_cleanup(cfg);
	dev_dbg(&pdev->dev, "%s(): removed.\n", __func__);

	platform_set_drvdata(pdev, NULL);

failed:
	return ret;
}

static struct platform_driver mt7697_platform_driver = {
	.driver = {
		.name = DRVNAME,
		.owner = THIS_MODULE,
		.bus = &platform_bus_type,
	},

	.probe = mt7697_probe,
	.remove	= mt7697_remove,
};

static int __init mt7697_init(void)
{
	int ret;

	pr_info(DRVNAME" initialize\n");
	ret = platform_driver_register(&mt7697_platform_driver);
	if (ret) {
		pr_err(DRVNAME" %s(): platform_driver_register() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	pdev = platform_device_alloc(DRVNAME, -1);
	if (!pdev) {
		pr_err(DRVNAME" %s(): platform_device_alloc() failed\n", 
			__func__);
		platform_driver_unregister(&mt7697_platform_driver);
		ret = -ENOMEM;
		goto cleanup;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err(DRVNAME" %s(): platform_device_add() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	platform_device_put(pdev);

cleanup:
	return ret;
}

static void __exit mt7697_exit(void)
{
	pr_info(DRVNAME" exit\n");
	platform_device_del(pdev);
	platform_driver_unregister(&mt7697_platform_driver);
}

module_init(mt7697_init);
module_exit(mt7697_exit);

void mt7697_disconnect_timer_hndlr(unsigned long ptr)
{
	struct net_device *ndev = (struct net_device *)ptr;
	struct mt7697_vif *vif = netdev_priv(ndev);

//	ath6kl_init_profile_info(vif);
	schedule_work(&vif->disconnect_work);
}

int mt7697_disconnect(struct mt7697_vif *vif)
{
	int ret = 0;

	dev_dbg(vif->cfg->dev, "%s(): disconnect\n", __func__);
	if (test_bit(CONNECTED, &vif->flags) ||
	    test_bit(CONNECT_PEND, &vif->flags)) {
		if (vif->cfg->reg_rx_hndlr) {
			ret = mt7697_wr_set_op_mode_req(vif->cfg, 
				vif->cfg->wifi_config.opmode);
			if (ret < 0) {
				dev_err(vif->cfg->dev, 
					"%s(): mt7697_wr_set_op_mode_req() failed(%d)\n", 
					__func__, ret);
       				goto failed;
    			}

			ret = mt7697_wr_unreg_rx_hndlr_req(vif->cfg);
			if (ret < 0) {
				dev_err(vif->cfg->dev, 
					"%s(): mt7697_wr_unreg_rx_hndlr_req() failed(%d)\n", 
					__func__, ret);
       				goto failed;
    			}
		}

		ret = mt7697_wr_disconnect_req(vif->cfg, vif->fw_vif_idx, 
			NULL);
		if (ret < 0) {
			dev_err(vif->cfg->dev, 
				"%s(): mt7697_wr_disconnect_req() failed(%d)\n", 
				__func__, ret);
			goto failed;
		}

		/*
		 * Disconnect command is issued, clear the connect pending
		 * flag. The connected flag will be cleared in
		 * disconnect event notification.
		 */
		clear_bit(CONNECT_PEND, &vif->flags);
	}

failed:
	return ret;
}

struct mt7697_cookie *mt7697_alloc_cookie(struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_cookie *cookie;

	cookie = cfg->cookie_list;
	if (cookie != NULL) {
		cfg->cookie_list = cookie->arc_list_next;
		cfg->cookie_count--;
	}

	return cookie;
}

void mt7697_free_cookie(struct mt7697_cfg80211_info *cfg, 
                        struct mt7697_cookie *cookie)
{
	/* Insert first */
	if (!cfg || !cookie)
		return;

	cookie->skb = NULL;
	cookie->arc_list_next = cfg->cookie_list;
	cfg->cookie_list = cookie;
	cfg->cookie_count++;
}

MODULE_AUTHOR("Sierra Wireless Corporation");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek7697 WiFi 80211");

