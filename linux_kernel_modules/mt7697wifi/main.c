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
#include "ioctl.h"
#include "core.h"
#include "cfg80211.h"

static int itf_idx_start = 1;
module_param(itf_idx_start, int, S_IRUGO);
MODULE_PARM_DESC(itf_idx_start, "MT7697 interface start index");

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

	mt7697_cfg80211_stop(vif);
	clear_bit(WLAN_ENABLED, &vif->flags);
	return 0;
}

static struct net_device_stats *mt7697_get_stats(struct net_device *dev)
{
	struct mt7697_vif *vif = netdev_priv(dev);
	return &vif->net_stats;
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
		                 MT7697_MAC80211_QUEUE_RX, cfg, 
                                 mt7697_proc_80211cmd, 
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

failed:
	return;
}

static const struct net_device_ops mt7697_netdev_ops = {
        .ndo_open               = mt7697_open,
        .ndo_stop               = mt7697_stop,
        .ndo_start_xmit         = mt7697_data_tx,
	.ndo_get_stats          = mt7697_get_stats,
        .ndo_set_rx_mode        = mt7697_set_multicast_list,
};

void mt7697_init_netdev(struct net_device *ndev)
{
        ndev->netdev_ops = &mt7697_netdev_ops;
	ndev->wireless_handlers = &mt7697_wireless_hndlrs;
        ndev->destructor = free_netdev;
        ndev->watchdog_timeo = MT7697_TX_TIMEOUT;
        ndev->needed_headroom = sizeof(struct ieee80211_hdr) + 
			        sizeof(struct mt7697_llc_snap_hdr);
        ndev->hw_features |= NETIF_F_IP_CSUM | NETIF_F_RXCSUM;
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

static int __init mt7697_probe(struct platform_device *pdev)
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
	cfg->tx_workq = create_singlethread_workqueue(DRVNAME);
	if (!cfg->tx_workq) {
                dev_err(&pdev->dev, 
			"%s(): mt7697_cfg80211_create() failed()\n",
			__func__);
		err = -ENOMEM;
		goto failed;
        }

	INIT_WORK(&cfg->init_work, mt7697_init_hw_start);
	INIT_WORK(&cfg->tx_work, mt7697_tx_work);

	spin_lock_init(&cfg->vif_list_lock);
	INIT_LIST_HEAD(&cfg->vif_list);

	cfg->hif_ops = &if_ops;
	cfg->dev = &pdev->dev;
	mt7697_cookie_init(cfg);

	cfg->vif_start = itf_idx_start;
	cfg->vif_max = MT7697_MAX_STA;

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

	mt7697_cookie_cleanup(cfg);
	mt7697_cfg80211_cleanup(cfg);
	mt7697_cfg80211_destroy(cfg);
	pr_info(DRVNAME" %s(): removed.\n", __func__);
	return 0;
}

static int mt7697_release(struct platform_device *pdev)
{
	pr_info(DRVNAME" %s(): released.\n", __func__);
	return 0;
}

static struct platform_device mt7697_platform_device = {
    	.name		= DRVNAME,
    	.id		= PLATFORM_DEVID_NONE,
    	.dev 		= {
		.release	= mt7697_release,
	},
};

static struct platform_driver mt7697_platform_driver = {
	.driver = {
		.name = DRVNAME,
		.owner = THIS_MODULE,
	},

	.probe = mt7697_probe,
	.remove	= mt7697_remove,
};

static int __init mt7697_init(void)
{
	int ret;

	pr_info(DRVNAME" init\n");
	ret = platform_device_register(&mt7697_platform_device);
	if (ret) {
		pr_err(DRVNAME" %s(): platform_device_register() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	ret = platform_driver_register(&mt7697_platform_driver);
	if (ret) {
		pr_err(DRVNAME" %s(): platform_driver_register() failed(%d)\n", 
			__func__, ret);
		platform_device_del(pdev);
		goto cleanup;
	}

cleanup:
	return ret;
}

static void __exit mt7697_exit(void)
{
	platform_driver_unregister(&mt7697_platform_driver);
	platform_device_unregister(&mt7697_platform_device);
	pr_info(DRVNAME" exit\n");
}

module_init(mt7697_init);
module_exit(mt7697_exit);

void mt7697_disconnect_timer_hndlr(unsigned long ptr)
{
	struct net_device *ndev = (struct net_device *)ptr;
	struct mt7697_vif *vif = netdev_priv(ndev);

	schedule_work(&vif->disconnect_work);
}

int mt7697_disconnect(struct mt7697_vif *vif)
{
	int ret = 0;

	dev_dbg(vif->cfg->dev, "%s(): disconnect\n", __func__);
	if (test_bit(CONNECTED, &vif->flags) ||
	    test_bit(CONNECT_PEND, &vif->flags)) {
		if (vif->sme_state == SME_CONNECTING)
			cfg80211_connect_result(vif->ndev, vif->bssid, 
						NULL, 0,
						NULL, 0,
						WLAN_STATUS_UNSPECIFIED_FAILURE,
						GFP_KERNEL);
		else if (vif->sme_state == SME_CONNECTED) {
			cfg80211_disconnected(vif->ndev, 0,
				      	      NULL, 0, GFP_KERNEL);
		}

		ret = mt7697_wr_disconnect_req(vif->cfg, vif->fw_vif_idx, 
			NULL);
		if (ret < 0) {
			dev_err(vif->cfg->dev, 
				"%s(): mt7697_wr_disconnect_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		memset(vif->ssid, 0, IEEE80211_MAX_SSID_LEN);
		ret = mt7697_wr_set_ssid_req(vif->cfg, IEEE80211_MAX_SSID_LEN, vif->ssid);
		if (ret < 0) {
			dev_err(vif->cfg->dev, 
				"%s(): mt7697_wr_set_ssid_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		memset(vif->req_bssid, 0, ETH_ALEN);
		ret = mt7697_wr_set_bssid_req(vif->cfg, vif->req_bssid);
		if (ret < 0) {
			dev_err(vif->cfg->dev, 
				"%s(): mt7697_wr_set_channel_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		memset(vif->pmk, 0, MT7697_WIFI_LENGTH_PMK);
		ret = mt7697_wr_set_pmk_req(vif->cfg, vif->pmk);
		if (ret < 0) {
			dev_err(vif->cfg->dev, "%s(): mt7697_wr_set_pmk_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		ret = mt7697_wr_reload_settings_req(vif->cfg, vif->fw_vif_idx);
		if (ret < 0) {
			dev_err(vif->cfg->dev, 
				"%s(): mt7697_wr_reload_settings_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		ret = mt7697q_wr_unused(vif->cfg->txq_hdl, vif->cfg->rxq_hdl);
		if (ret < 0) {
			dev_err(vif->cfg->dev, 
				"%s(): mt7697q_wr_unused() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		/*
		 * Disconnect command is issued, clear the connect pending
		 * flag. The connected flag will be cleared in
		 * disconnect event notification.
		 */
		clear_bit(CONNECT_PEND, &vif->flags);
	}

cleanup:
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

