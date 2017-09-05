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

#include <linux/types.h>
#include <linux/rtnetlink.h>
#include <linux/etherdevice.h>
#include "queue_i.h"
#include "wmi.h"
#include "core.h"
#include "cfg80211.h"
#include "wifi_api.h"

static int mt7697_proc_mac_addr(const struct mt7697q_rsp_hdr* rsp, 
                                struct mt7697_cfg80211_info *cfg)
{
	u8 addr[LEN32_ALIGNED(ETH_ALEN)];
	char iname[MT7697_IFACE_NAME_LEN];
	struct wireless_dev *wdev;
	int ret = 0;

	dev_dbg(cfg->dev, "%s(): --> MAC ADDRESS\n", __func__);
	if (rsp->cmd.len != sizeof(struct mt7697_mac_addr_rsp)) {
		dev_err(cfg->dev, 
			"%s(): invalid MAC address rsp len(%u != %u)\n",
			__func__, rsp->cmd.len, 
			sizeof(struct mt7697_mac_addr_rsp));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&addr, 
		LEN_TO_WORD(sizeof(addr)));
	if (ret != LEN_TO_WORD(sizeof(addr))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(addr)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	print_hex_dump(KERN_DEBUG, DRVNAME" MAC address ", 
		DUMP_PREFIX_OFFSET, 16, 1, addr, ETH_ALEN, 0);
	memcpy(cfg->mac_addr.addr, addr, ETH_ALEN);

	rtnl_lock();

	snprintf(iname, MT7697_IFACE_NAME_LEN, "wlan%d", cfg->vif_start);
	wdev = mt7697_interface_add(cfg, iname, 
		(cfg->wifi_cfg.opmode == MT7697_WIFI_MODE_STA_ONLY) ? 
			NL80211_IFTYPE_STATION : NL80211_IFTYPE_AP, 0);

	rtnl_unlock();

	if (!wdev) {
		dev_err(cfg->dev, "%s(): mt7697_interface_add() failed\n", 
				__func__);
		ret = -ENOMEM;
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s(): name/type('%s'/%u) netdev(0x%p), cfg(0x%p)\n",
		__func__, wdev->netdev->name, wdev->iftype, wdev->netdev, cfg);

cleanup:
	return ret;
}

static int mt7697_proc_get_wireless_mode(const struct mt7697q_rsp_hdr* rsp, 
                                         struct mt7697_cfg80211_info *cfg)
{
	u32 wireless_mode;
	int ret = 0;

	dev_dbg(cfg->dev, "%s(): --> WIRELESS MODE\n", __func__);
	if (rsp->cmd.len - sizeof(struct mt7697q_rsp_hdr) != sizeof(u32)) {
		dev_err(cfg->dev, 
			"%s(): invalid wireless mode rsp len(%u != %u)\n",
			__func__, 
			rsp->cmd.len - sizeof(struct mt7697q_rsp_hdr),
			sizeof(u32));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&wireless_mode, 
		LEN_TO_WORD(sizeof(u32)));
	if (ret != LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, 
			LEN_TO_WORD(sizeof(wireless_mode)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	dev_dbg(cfg->dev, "%s(): wireless mode(%u)\n", 
		__func__, wireless_mode);
	cfg->hw_wireless_mode = wireless_mode;

cleanup:
	return ret;
}

static int mt7697_proc_get_cfg(const struct mt7697q_rsp_hdr* rsp, 
                               struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_wifi_config_t *wifi_cfg;
	u8* rd_buf = NULL;
	int ret = 0;

	dev_dbg(cfg->dev, "%s(): --> CONFIG\n", __func__);
	if (rsp->cmd.len - sizeof(struct mt7697q_rsp_hdr) != 
		LEN32_ALIGNED(sizeof(struct mt7697_wifi_config_t))) {
		dev_err(cfg->dev, "%s(): invalid cfg rsp len(%u != %u)\n", 
			__func__, 
			rsp->cmd.len - sizeof(struct mt7697q_rsp_hdr),
			LEN32_ALIGNED(sizeof(struct mt7697_wifi_config_t)));
		ret = -EINVAL;
       		goto cleanup;
	}

	rd_buf = kzalloc(LEN32_ALIGNED(sizeof(struct mt7697_wifi_config_t)), 
		GFP_KERNEL);
	if (!rd_buf) {
		dev_err(cfg->dev, "%s(): kzalloc() failed\n", __func__);
		ret = -ENOMEM;
       		goto cleanup;
	}
	
	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)rd_buf, 
		LEN_TO_WORD(sizeof(struct mt7697_wifi_config_t)));
	if (ret != LEN_TO_WORD(sizeof(struct mt7697_wifi_config_t))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, 
			LEN_TO_WORD(sizeof(struct mt7697_wifi_config_t)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	wifi_cfg = (struct mt7697_wifi_config_t*)rd_buf;
	dev_dbg(cfg->dev, "%s(): operation mode(%u)\n", 
		__func__, wifi_cfg->opmode);
	if (wifi_cfg->opmode == MT7697_WIFI_MODE_STA_ONLY) 
	    cfg->port_type = MT7697_PORT_STA;
	else if (wifi_cfg->opmode == MT7697_WIFI_MODE_AP_ONLY) 
	    cfg->port_type = MT7697_PORT_AP;
	else {
		dev_err(cfg->dev, 
			"%s(): unsupported operation mode(%d)\n",
			__func__,  wifi_cfg->opmode);
		ret = -EINVAL;
		goto cleanup;
	}

	if ((wifi_cfg->opmode == MT7697_WIFI_MODE_STA_ONLY) || 
	    (wifi_cfg->opmode == MT7697_WIFI_MODE_REPEATER)) {
		dev_dbg(cfg->dev, "%s(): STA ssid len(%u)\n", 
			__func__, wifi_cfg->sta.ssid_len);
		if (wifi_cfg->sta.ssid_len > 0) {
			if (wifi_cfg->sta.ssid_len > IEEE80211_MAX_SSID_LEN) {
				dev_err(cfg->dev, 
					"%s(): invalid STA SSID len(%u > %u)\n",
					__func__,  wifi_cfg->sta.ssid_len, 
					IEEE80211_MAX_SSID_LEN);
				ret = -EINVAL;
				goto cleanup;
			}

			dev_dbg(cfg->dev, "%s(): STA ssid('%s')\n", 
				__func__, wifi_cfg->sta.ssid);
		}
	
		if (wifi_cfg->sta.bssid_present) {
			print_hex_dump(KERN_DEBUG, DRVNAME"STA BSSID ", 
				DUMP_PREFIX_OFFSET, 16, 1, 
				wifi_cfg->sta.bssid, ETH_ALEN, 0);
		}

		dev_dbg(cfg->dev, "%s(): STA passphrase len(%u)\n", 
			__func__, wifi_cfg->sta.pw_len);
		if (wifi_cfg->sta.pw_len > 0) {
			if (wifi_cfg->sta.pw_len > MT7697_WIFI_LENGTH_PASSPHRASE) {
				dev_err(cfg->dev, 
					"%s(): invalid STA passphrase len(%u > %u)\n",
					__func__,  wifi_cfg->sta.pw_len, 
					MT7697_WIFI_LENGTH_PASSPHRASE);
				ret = -EINVAL;
				goto cleanup;
			}

			dev_dbg(cfg->dev, "%s(): STA passphrase('%s')\n", 
				__func__, wifi_cfg->sta.pw);
		}
	}

	if ((wifi_cfg->opmode == MT7697_WIFI_MODE_AP_ONLY) || 
	    (wifi_cfg->opmode == MT7697_WIFI_MODE_REPEATER)) {
		dev_dbg(cfg->dev, "%s(): AP ssid len(%u)\n", 
			__func__, wifi_cfg->ap.ssid_len);
		if (wifi_cfg->ap.ssid_len > 0) {
			if (wifi_cfg->ap.ssid_len > IEEE80211_MAX_SSID_LEN) {
				dev_err(cfg->dev, 
					"%s(): invalid AP SSID len(%u > %u)\n",
					__func__,  wifi_cfg->ap.ssid_len, 
					IEEE80211_MAX_SSID_LEN);
				ret = -EINVAL;
				goto cleanup;
			}

			dev_dbg(cfg->dev, "%s(): AP ssid('%s')\n", 
				__func__, wifi_cfg->ap.ssid);
		}

		dev_dbg(cfg->dev, "%s(): AP passphrase len(%u)\n", 
			__func__, wifi_cfg->ap.pw_len);
		if (wifi_cfg->ap.pw_len > 0) {
			if (wifi_cfg->ap.pw_len > MT7697_WIFI_LENGTH_PASSPHRASE) {
				dev_err(cfg->dev, 
					"%s(): invalid AP passphrase len(%u > %u)\n",
					__func__, wifi_cfg->ap.pw_len, 
					MT7697_WIFI_LENGTH_PASSPHRASE);
				ret = -EINVAL;
				goto cleanup;
			}

			dev_dbg(cfg->dev, "%s(): AP passphrase('%s')\n", 
				__func__, wifi_cfg->ap.pw);
		}

		dev_dbg(cfg->dev, "%s(): AP auth mode(%u) encrypt type(%u)\n", 
			__func__, wifi_cfg->ap.auth_mode, 
			wifi_cfg->ap.encrypt_type);
		dev_dbg(cfg->dev, "%s(): AP channel(%u) bandwidth(%u)\n",
			__func__, wifi_cfg->ap.ch, wifi_cfg->ap.bandwidth);
		if (wifi_cfg->ap.bandwidth == MT7697_WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_40MHZ) {
			dev_dbg(cfg->dev, "%s(): AP bandwidth ext(%u)\n", 
				__func__, wifi_cfg->ap.bandwidth_ext);
		}
	}

	memcpy(&cfg->wifi_cfg, wifi_cfg, sizeof(struct mt7697_wifi_config_t));

	ret = mt7697_cfg80211_get_params(cfg);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_cfg80211_get_params() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

cleanup:
	if (rd_buf) kfree(rd_buf);
	return ret;
}

static int mt7697_proc_get_radio_state(const struct mt7697q_rsp_hdr* rsp, 
                                       struct mt7697_cfg80211_info *cfg)
{
	u32 state;
	int ret = 0;

	dev_dbg(cfg->dev, "%s(): --> GET RADIO STATE\n", __func__);
	if (rsp->cmd.len - sizeof(struct mt7697q_rsp_hdr) != sizeof(u32)) {
		dev_err(cfg->dev, 
			"%s(): invalid get radio state rsp len(%u != %u)\n", 
			__func__, 
			rsp->cmd.len - sizeof(struct mt7697q_rsp_hdr),
			sizeof(u32));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&state, 
		LEN_TO_WORD(sizeof(u32)));
	if (ret != LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	cfg->radio_state = state;
	dev_dbg(cfg->dev, "%s(): radio state(%u)\n", 
		__func__, cfg->radio_state);

cleanup:
	return ret;
}

static int mt7697_proc_get_listen_interval(const struct mt7697q_rsp_hdr* rsp, 
                                           struct mt7697_cfg80211_info *cfg)
{
	u32 interval;
	int ret = 0;

	dev_dbg(cfg->dev, "%s(): --> GET LISTEN INTERVAL\n", __func__);
	if (rsp->cmd.len - sizeof(struct mt7697q_rsp_hdr) != sizeof(u32)) {
		dev_err(cfg->dev, 
			"%s(): invalid get listen interval rsp len(%u != %u)\n",
			__func__, 
			rsp->cmd.len - sizeof(struct mt7697q_rsp_hdr),
			sizeof(u32));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&interval, 
		LEN_TO_WORD(sizeof(u32)));
	if (ret != LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	cfg->listen_interval = interval;
	dev_dbg(cfg->dev, "%s(): listen interval(%u)\n", 
		__func__, cfg->listen_interval);

cleanup:
	return ret;
}

static int mt7697_proc_scan_ind(const struct mt7697q_rsp_hdr* rsp, 
	                        struct mt7697_cfg80211_info *cfg)
{
	struct ieee80211_mgmt *rx_mgmt_frame;
	s32 rssi;
	u32 ch;
        u32 probe_rsp_len;
	int ret = 0;
	__le16 fc;

	dev_dbg(cfg->dev, "%s(): --> SCAN IND\n", __func__);

	if (rsp->cmd.len <= sizeof(struct mt7697_scan_ind)) {
		dev_err(cfg->dev, "%s(): invalid scan ind len(%u <= %u)\n", 
			__func__, rsp->cmd.len, 
			sizeof(struct mt7697_scan_ind));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (s32*)&rssi, 
		LEN_TO_WORD(sizeof(s32)));
	if (ret != LEN_TO_WORD(sizeof(s32))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(s32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s(): rssi(%d)\n", __func__, rssi);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&ch, 
		LEN_TO_WORD(sizeof(u32)));
	if (ret != LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s(): channel(%u)\n", __func__, ch);

	probe_rsp_len = rsp->result;
	if (probe_rsp_len > IEEE80211_MAX_DATA_LEN) {
		dev_err(cfg->dev, "%s(): invalid probe rsp len(%d > %d)\n", 
			__func__, probe_rsp_len, IEEE80211_MAX_DATA_LEN);
		ret = -EINVAL;
       		goto cleanup;
	}
	else if (!probe_rsp_len) {
		dev_err(cfg->dev, "%s(): invalid probe rsp len(%d)\n", 
			__func__, probe_rsp_len);
		ret = -EINVAL;
       		goto cleanup;
	}
	dev_dbg(cfg->dev, "%s(): probe rsp len(%u)\n", 
		__func__, probe_rsp_len);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)cfg->probe_data, 
		LEN_TO_WORD(LEN32_ALIGNED(probe_rsp_len)));
	if (ret != LEN_TO_WORD(LEN32_ALIGNED(probe_rsp_len))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, 
			LEN_TO_WORD(LEN32_ALIGNED(probe_rsp_len)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	rx_mgmt_frame = (struct ieee80211_mgmt*)cfg->probe_data;
	fc = rx_mgmt_frame->frame_control;
	if (ieee80211_is_beacon(fc) || ieee80211_is_probe_resp(fc)) {
		struct cfg80211_bss *bss;
		struct ieee80211_channel *channel;
		struct ieee80211_supported_band *band;
		u32 freq;

		if ((ch > 0) && (ch <= MT7697_CH_MAX_2G_CHANNEL))
			band = cfg->wiphy->bands[IEEE80211_BAND_2GHZ];
		else if ((ch >= MT7697_CH_MIN_5G_CHANNEL) && 
		 	 (ch <= MT7697_CH_MAX_5G_CHANNEL))
			band = cfg->wiphy->bands[IEEE80211_BAND_5GHZ];
		else {
			dev_err(cfg->dev, "%s(): invalid channel(%u)\n",
				__func__, ch);
			ret = -EINVAL;
			goto cleanup;
		}

		freq = ieee80211_channel_to_frequency(ch, band->band);
		if (!freq) {
			dev_err(cfg->dev, 
				"%s(): ieee80211_channel_to_frequency() failed\n", 
				__func__);
			ret = -EINVAL;
			goto cleanup;	
		}

		channel = ieee80211_get_channel(cfg->wiphy, freq);		
		if (!channel) {
			dev_err(cfg->dev, 
				"%s(): ieee80211_get_channel() failed\n", 
				__func__);
			ret = -EINVAL;
			goto cleanup;
		}

		bss = cfg80211_inform_bss_frame(cfg->wiphy, channel, 
			rx_mgmt_frame, probe_rsp_len, rssi * 100, 
			GFP_ATOMIC);	
		if (!bss) {
			dev_err(cfg->dev, 
				"%s(): cfg80211_inform_bss_frame() failed\n",
				__func__);
			ret = -ENOMEM;
			goto cleanup;
		}

		print_hex_dump(KERN_DEBUG, DRVNAME" BSS BSSID ", 
			DUMP_PREFIX_OFFSET, 16, 1, bss->bssid, ETH_ALEN, 0);
		dev_dbg(cfg->dev, 
			"%s(): BSS signal(%d) scan width(%u) cap(0x%08x)\n", 
			__func__, bss->signal, bss->scan_width, 
			bss->capability);
		if (bss->channel) {
			dev_dbg(cfg->dev, 
				"%s(): BSS channel band(%u) center freq(%u)\n", 
				__func__, bss->channel->band, 
				bss->channel->center_freq);
		} 
	}
	else {
		dev_err(cfg->dev, "%s(): Rx unsupported mgmt frame\n", 
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}

cleanup:
	return ret;
}

static int mt7697_proc_scan_rsp(const struct mt7697q_rsp_hdr* rsp, 
	                        struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_vif *vif;
	u32 if_idx;
	int ret;

	dev_dbg(cfg->dev, "%s(): --> SCAN RSP result(%d)\n", 
		__func__, rsp->result);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&if_idx, 
		LEN_TO_WORD(sizeof(u32)));
	if (ret != LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s(): if idx(%d)\n", __func__, if_idx);

	if (rsp->result < 0) {
		vif = mt7697_get_vif_by_idx(cfg, if_idx);
		if (!vif) {
			dev_err(cfg->dev, 
				"%s(): mt7697_get_vif_by_idx(%u) failed\n",
				__func__, if_idx);
			ret = -EINVAL;
			goto cleanup;
		}

		dev_dbg(cfg->dev, "%s(): vif(%u)\n", 
			__func__, vif->fw_vif_idx);
		cfg80211_scan_done(vif->scan_req, true);
		vif->scan_req = NULL;
	}

cleanup:
	return ret;
}

static int mt7697_proc_scan_complete_ind(struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_vif *vif;
	u32 if_idx;
	int ret;

	dev_dbg(cfg->dev, "%s(): --> SCAN COMPLETE IND\n", __func__);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&if_idx, 
		LEN_TO_WORD(sizeof(u32)));
	if (ret != LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s(): if idx(%u)\n", __func__, if_idx);

	vif = mt7697_get_vif_by_idx(cfg, if_idx);
	if (!vif) {
		dev_err(cfg->dev, "%s(): mt7697_get_vif_by_idx(%u) failed\n",
			    __func__, if_idx);
		ret = -EINVAL;
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s(): vif(%u)\n", __func__, vif->fw_vif_idx);
	cfg80211_scan_done(vif->scan_req, false);
	vif->scan_req = NULL;

cleanup:
	return ret;
}

static int mt7697_proc_connect_ind(const struct mt7697q_rsp_hdr* rsp, 
                                   struct mt7697_cfg80211_info *cfg)
{
	u8 bssid[LEN32_ALIGNED(ETH_ALEN)];
	struct mt7697_vif *vif;
	u32 if_idx;
	u32 channel;
	int ret;

	dev_dbg(cfg->dev, "%s(): --> CONNECT IND\n", __func__);
	if (rsp->cmd.len != sizeof(struct mt7697_connect_ind)) {
		dev_err(cfg->dev, "%s(): invalid connect ind len(%d != %d)\n",
			__func__, rsp->cmd.len, 
			sizeof(struct mt7697_connect_ind));
		ret = -EINVAL;
		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&if_idx, 
		LEN_TO_WORD(sizeof(u32)));
	if (ret != LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s(): if idx(%d)\n", __func__, if_idx);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&channel, 
		LEN_TO_WORD(sizeof(u32)));
	if (ret != LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s(): channel(%d)\n", __func__, channel);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)bssid, 
		LEN_TO_WORD(sizeof(bssid)));
	if (ret != LEN_TO_WORD(sizeof(bssid))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(bssid)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	print_hex_dump(KERN_DEBUG, DRVNAME" BSSID ", 
		DUMP_PREFIX_OFFSET, 16, 1, bssid, ETH_ALEN, 0);

	if (list_empty(&cfg->vif_list)) {
		dev_dbg(cfg->dev, "%s(): no interfaces\n", __func__);
       		goto cleanup;
	}

	if (cfg->wifi_cfg.opmode == MT7697_WIFI_MODE_STA_ONLY) {
		vif = mt7697_get_vif_by_idx(cfg, if_idx);
		if (!vif) {
			dev_err(cfg->dev, "%s(): mt7697_get_vif_by_idx(%u) failed\n",
				__func__, if_idx);
			ret = -EINVAL;
			goto cleanup;
		}

		dev_dbg(cfg->dev, "%s(): vif(%u)\n", __func__, vif->fw_vif_idx);
		ret = mt7697_cfg80211_connect_event(vif, bssid, channel);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_cfg80211_connect_event() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
	}
	else {
		struct station_info sinfo = {0};

		vif = mt7697_get_vif_by_idx(cfg, 0);
		if (!vif) {
			dev_err(cfg->dev, "%s(): mt7697_get_vif_by_idx(%u) failed\n",
				__func__, if_idx);
			ret = -EINVAL;
			goto cleanup;
		}

		ret = mt7697_cfg80211_new_sta(vif, bssid);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_cfg80211_new_sta() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

		cfg80211_new_sta(vif->ndev, bssid, &sinfo, GFP_KERNEL);

		/* Update connect & link status atomically */
		spin_lock_bh(&vif->if_lock);
		set_bit(CONNECTED, &vif->flags);
		netif_wake_queue(vif->ndev);
		netif_carrier_on(vif->ndev);
		spin_unlock_bh(&vif->if_lock);
	}

cleanup:
	return ret;
}

static int mt7697_proc_disconnect_ind(struct mt7697_cfg80211_info *cfg)
{
	u8 bssid[LEN32_ALIGNED(ETH_ALEN)];
	struct mt7697_vif *vif;
	u32 if_idx;
	u16 proto_reason = 0;
	int ret;

	dev_dbg(cfg->dev, "%s(): --> DISCONNECT IND\n", __func__);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&if_idx, 
		LEN_TO_WORD(sizeof(u32)));
	if (ret != LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s(): if idx(%d)\n", __func__, if_idx);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)bssid, 
		LEN_TO_WORD(sizeof(bssid)));
	if (ret != LEN_TO_WORD(sizeof(bssid))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(sizeof(bssid)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	print_hex_dump(KERN_DEBUG, DRVNAME" BSSID ", 
		DUMP_PREFIX_OFFSET, 16, 1, bssid, ETH_ALEN, 0);

	if (list_empty(&cfg->vif_list)) {
		dev_dbg(cfg->dev, "%s(): no interfaces\n", __func__);
       		goto cleanup;
	}

	if (cfg->wifi_cfg.opmode == MT7697_WIFI_MODE_STA_ONLY) {
		vif = mt7697_get_vif_by_idx(cfg, if_idx);
		if (!vif) {
			dev_err(cfg->dev, "%s(): mt7697_get_vif_by_idx(%u) failed\n",
				    __func__, if_idx);
			ret = -EINVAL;
			goto cleanup;
		}

		dev_dbg(cfg->dev, "%s(): vif(%u)\n", __func__, vif->fw_vif_idx);
		if (vif->sme_state == SME_CONNECTING) {
			ret = mt7697_wr_disconnect_req(vif->cfg, bssid);
			if (ret < 0) {
				dev_err(vif->cfg->dev, 
					"%s(): mt7697_wr_disconnect_req() failed(%d)\n", 
					__func__, ret);
				goto cleanup;
			}

			cfg80211_connect_result(vif->ndev, bssid, 
						NULL, 0,
						NULL, 0,
						WLAN_STATUS_UNSPECIFIED_FAILURE,
						GFP_KERNEL);
		}
		else if (vif->sme_state == SME_CONNECTED) {
			ret = mt7697_wr_disconnect_req(vif->cfg, bssid);
			if (ret < 0) {
				dev_err(vif->cfg->dev, 
					"%s(): mt7697_wr_disconnect_req() failed(%d)\n", 
					__func__, ret);
				goto cleanup;
			}

			cfg80211_disconnected(vif->ndev, proto_reason,
				      	      NULL, 0, GFP_KERNEL);
		}
	}
	else {
		vif = mt7697_get_vif_by_idx(cfg, 0);
		if (!vif) {
			dev_err(cfg->dev, 
				"%s(): mt7697_get_vif_by_idx(%u) failed\n",
				__func__, if_idx);
			ret = -EINVAL;
			goto cleanup;
		}

//		cfg80211_del_sta(vif->ndev, bssid, GFP_KERNEL);
		ret = mt7697_cfg80211_del_sta(vif, bssid);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_cfg80211_del_sta() failed(%d)\n",
				__func__, ret);
			goto cleanup;
		}
	}

	netif_stop_queue(vif->ndev);
	vif->sme_state = SME_DISCONNECTED;
	spin_lock_bh(&vif->if_lock);
	clear_bit(CONNECT_PEND, &vif->flags);
	clear_bit(CONNECTED, &vif->flags);
	dev_dbg(cfg->dev, "%s(): vif sme_state(%u), flags(0x%08lx)\n",
		__func__, vif->sme_state, vif->flags);
	spin_unlock_bh(&vif->if_lock);

cleanup:
	return ret;
}

static int mt7697_rx_raw(const struct mt7697q_rsp_hdr* rsp, 
                         struct mt7697_cfg80211_info *cfg)
{
	int ret;

	dev_dbg(cfg->dev, "%s(): --> RX RAW(%u)\n", __func__, rsp->cmd.len);

	if (rsp->cmd.len <= sizeof(struct mt7697q_rsp_hdr)) {
		dev_err(cfg->dev, "%s(): invalid rx raw len(%u <= %u)\n", 
			__func__, rsp->cmd.len, 
			sizeof(struct mt7697q_rsp_hdr));
		ret = -EINVAL;
       		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s(): len(%d)\n", __func__, rsp->result);
	if (rsp->result > IEEE80211_MAX_FRAME_LEN) {
		dev_err(cfg->dev, "%s(): invalid rx data len(%u > %u)\n", 
			__func__, rsp->result, IEEE80211_MAX_FRAME_LEN);
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)cfg->rx_data, 
		LEN_TO_WORD(LEN32_ALIGNED(rsp->result)));
	if (ret != LEN_TO_WORD(LEN32_ALIGNED(rsp->result))) {
		dev_err(cfg->dev, "%s(): read() failed(%d != %d)\n", 
			__func__, ret, 
			LEN_TO_WORD(LEN32_ALIGNED(rsp->result)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

//	print_hex_dump(KERN_DEBUG, DRVNAME" RX ", DUMP_PREFIX_OFFSET, 
//		16, 1, cfg->rx_data, rsp->result, 0);

	if (list_empty(&cfg->vif_list)) {
		dev_dbg(cfg->dev, "%s(): no interfaces\n", __func__);
       		goto cleanup;
	}

	/* TODO: interface index come from MT7697 */
	ret = mt7697_rx_data(cfg, rsp->result, 0);
	if (ret) {
		dev_err(cfg->dev, "%s(): mt7697_rx_data() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

cleanup:
	return ret;
}

int mt7697_proc_80211cmd(const struct mt7697q_rsp_hdr* rsp, void* priv)
{
	struct mt7697_cfg80211_info *cfg = (struct mt7697_cfg80211_info*)priv;
	int ret = 0;

	switch (rsp->cmd.type) {
	case MT7697_CMD_MAC_ADDR_RSP:
		ret = mt7697_proc_mac_addr(rsp, cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_proc_mac_addr() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_GET_WIRELESS_MODE_RSP:
		ret = mt7697_proc_get_wireless_mode(rsp, cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_proc_get_wireless_mode() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_GET_CFG_RSP:
		ret = mt7697_proc_get_cfg(rsp, cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_proc_get_cfg() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_GET_RADIO_STATE_RSP:
		ret = mt7697_proc_get_radio_state(rsp, cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"mt7697_proc_get_radio_state() failed(%d)\n", 
				ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_GET_LISTEN_INTERVAL_RSP:
		ret = mt7697_proc_get_listen_interval(rsp, cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_proc_get_listen_interval() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_SCAN_RSP:
		ret = mt7697_proc_scan_rsp(rsp, cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_proc_scan_rsp() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_SCAN_IND:
		ret = mt7697_proc_scan_ind(rsp, cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_proc_scan_ind() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_SCAN_COMPLETE_IND:
		ret = mt7697_proc_scan_complete_ind(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_proc_scan_complete_ind() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_CONNECT_IND:
		ret = mt7697_proc_connect_ind(rsp, cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_proc_connect_ind() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_DISCONNECT_IND:
		ret = mt7697_proc_disconnect_ind(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_proc_disconnect_ind() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_RX_RAW:
		ret = mt7697_rx_raw(rsp, cfg);
		if (ret < 0) {
			dev_err(cfg->dev, "%s(): mt7697_rx_raw() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

        case MT7697_CMD_SET_OP_MODE_RSP:
		dev_dbg(cfg->dev, "%s(): --> SET OP MODE\n", __func__);
		break;

	case MT7697_CMD_SET_PMK_RSP:
		dev_dbg(cfg->dev, "%s(): --> SET PMK\n", __func__);
		break;

	case MT7697_CMD_SET_CHANNEL_RSP:
		dev_dbg(cfg->dev, "%s(): --> SET CHANNEL\n", __func__);
		break;

	case MT7697_CMD_SET_BSSID_RSP:
		dev_dbg(cfg->dev, "%s(): --> SET BSSID\n", __func__);
		break;

	case MT7697_CMD_SET_SSID_RSP:
		dev_dbg(cfg->dev, "%s(): --> SET SSID\n", __func__);
		break;

	case MT7697_CMD_RELOAD_SETTINGS_RSP:
		dev_dbg(cfg->dev, "%s(): --> RELOAD SETTINGS RSP\n", __func__);
		break;

	case MT7697_CMD_SET_LISTEN_INTERVAL_RSP:
		dev_dbg(cfg->dev, "%s(): --> SET LISTEN INTERVAL\n", __func__);
		break;

	case MT7697_CMD_SET_WIRELESS_MODE_RSP:
		dev_dbg(cfg->dev, "%s(): --> SET WIRELESS MODE\n", __func__);
		break;

	case MT7697_CMD_SET_RADIO_STATE_RSP:
		dev_dbg(cfg->dev, "%s(): --> SET RADIO STATE\n", __func__);
		break;

	case MT7697_CMD_SET_SECURITY_MODE_RSP:
		dev_dbg(cfg->dev, "%s(): --> SET SECURITY MODE\n", __func__);
		break;

	case MT7697_CMD_SCAN_STOP_RSP:
		dev_dbg(cfg->dev, "%s(): --> SCAN STOP\n", __func__);
		break;

	case MT7697_CMD_DISCONNECT_RSP:
		dev_dbg(cfg->dev, "%s(): --> DISCONNECT RSP\n", __func__);
		break;

	default:
		dev_err(cfg->dev, "%s(): unsupported cmd(%u)\n", 
			__func__, rsp->cmd.type);
		ret = -EINVAL;
		goto cleanup;
	}

cleanup:
	return ret;
}

int mt7697_wr_get_wireless_mode_req(const struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_get_wireless_mode_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_get_wireless_mode_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_GET_WIRELESS_MODE_REQ;
	req.port = cfg->port_type;

	dev_dbg(cfg->dev, "%s(): <-- GET WIRELESS MODE port(%u) len(%u)\n", 
		__func__, req.port, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_set_wireless_mode_req(const struct mt7697_cfg80211_info *cfg, 
                                    u8 mode)
{
	struct mt7697_set_wireless_mode_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_wireless_mode_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_WIRELESS_MODE_REQ;
	req.port = cfg->port_type;
	req.mode = mode;

	dev_dbg(cfg->dev, "%s(): <-- SET WIRELESS MODE port(%u) len(%u)\n", 
		__func__, req.port, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_set_pmk_req(const struct mt7697_cfg80211_info *cfg, 
			  const u8 pmk[MT7697_WIFI_LENGTH_PMK])
{
	struct mt7697_set_pmk_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_pmk_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_PMK_REQ;
	req.port = cfg->port_type;
	memcpy(req.pmk, pmk, MT7697_WIFI_LENGTH_PMK);

	dev_dbg(cfg->dev, "%s(): <-- SET PMK port(%u) len(%u)\n", 
		__func__, req.port, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(sizeof(struct mt7697_set_pmk_req)));
	if (ret != LEN_TO_WORD(sizeof(struct mt7697_set_pmk_req))) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, 
			LEN_TO_WORD(sizeof(struct mt7697_set_pmk_req)));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_set_channel_req(const struct mt7697_cfg80211_info *cfg, u8 ch)
{
	struct mt7697_set_channel_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_channel_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_CHANNEL_REQ;
	req.port = cfg->port_type;
	req.ch = ch;

	dev_dbg(cfg->dev, "%s(): <-- SET CHANNEL port(%u) len(%u)\n", 
		__func__, req.port, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(sizeof(struct mt7697_set_channel_req)));
	if (ret != LEN_TO_WORD(sizeof(struct mt7697_set_channel_req))) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, 
			LEN_TO_WORD(sizeof(struct mt7697_set_channel_req)));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_set_bssid_req(const struct mt7697_cfg80211_info *cfg, 
                            const u8 bssid[ETH_ALEN])
{
	struct mt7697_set_bssid_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_bssid_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_BSSID_REQ;
	memcpy(req.bssid, bssid, ETH_ALEN);

	dev_dbg(cfg->dev, "%s(): <-- SET BSSID len(%u)\n", 
		__func__, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(sizeof(struct mt7697_set_bssid_req)));
	if (ret != LEN_TO_WORD(sizeof(struct mt7697_set_bssid_req))) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, 
			LEN_TO_WORD(sizeof(struct mt7697_set_bssid_req)));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_set_ssid_req(const struct mt7697_cfg80211_info *cfg,  
                           u8 len, const u8 ssid[])
{
	struct mt7697_set_ssid_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_ssid_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_SSID_REQ;
	req.port = cfg->port_type;
	req.len = len;
	memcpy(req.ssid, ssid, len);

	dev_dbg(cfg->dev, "%s(): <-- SET SSID len(%u)\n", 
		__func__, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(sizeof(struct mt7697_set_ssid_req)));
	if (ret != LEN_TO_WORD(sizeof(struct mt7697_set_ssid_req))) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, 
			LEN_TO_WORD(sizeof(struct mt7697_set_ssid_req)));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_reload_settings_req(const struct mt7697_cfg80211_info* cfg, u8 if_idx)
{
	struct mt7697_reload_settings_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_reload_settings_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_RELOAD_SETTINGS_REQ;
	req.if_idx = if_idx;

	dev_dbg(cfg->dev, "%s(): <-- RELOAD SETTINGS len(%u)\n", 
		__func__, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(sizeof(struct mt7697_reload_settings_req)));
	if (ret != LEN_TO_WORD(sizeof(struct mt7697_reload_settings_req))) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, 
			LEN_TO_WORD(sizeof(struct mt7697_reload_settings_req)));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_mac_addr_req(const struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_mac_addr_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_mac_addr_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_MAC_ADDR_REQ;
	req.port = cfg->port_type;

	dev_dbg(cfg->dev, "%s(): <-- GET MAC ADDRESS port(%u) len(%u)\n", 
		__func__, req.port, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_set_op_mode_req(const struct mt7697_cfg80211_info* cfg)
{
	struct mt7697_set_op_mode_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_op_mode_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_OP_MODE_REQ;
	req.opmode = cfg->wifi_cfg.opmode;

	dev_dbg(cfg->dev, "%s(): <-- SET OPMODE(%u) len(%u)\n", 
		__func__, req.opmode, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_cfg_req(const struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_cfg_req req;
	int ret;

	req.len = sizeof(struct mt7697_cfg_req);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_GET_CFG_REQ;
	
	dev_dbg(cfg->dev, "%s(): <-- GET CONFIG len(%u)\n", __func__, req.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.len));
	if (ret != LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_get_radio_state_req(const struct mt7697_cfg80211_info* cfg)
{
	struct mt7697_get_radio_state_req req;
	int ret;

	req.len = sizeof(struct mt7697_get_radio_state_req);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_GET_RADIO_STATE_REQ;
	
	dev_dbg(cfg->dev, "%s(): <-- GET RADIO STATE len(%u)\n", 
		__func__, req.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.len));
	if (ret != LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_set_radio_state_req(const struct mt7697_cfg80211_info* cfg, 
                                  u8 state)
{
	struct mt7697_set_radio_state_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_radio_state_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_RADIO_STATE_REQ;
	req.state = state;

	dev_dbg(cfg->dev, "%s(): <-- SET RADIO STATE(%u) len(%u)\n", 
		__func__, state, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_get_listen_interval_req(const struct mt7697_cfg80211_info* cfg)
{
	struct mt7697_get_listen_interval_req req;
	int ret;

	req.len = sizeof(struct mt7697_get_listen_interval_req);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_GET_LISTEN_INTERVAL_REQ;
	
	dev_dbg(cfg->dev, "%s(): <-- GET LISTEN INTERVAL len(%u)\n", 
		__func__, req.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.len));
	if (ret != LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_set_listen_interval_req(const struct mt7697_cfg80211_info* cfg, 
				      u32 interval)
{
	struct mt7697_set_listen_interval_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_listen_interval_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_LISTEN_INTERVAL_REQ;
	req.interval = interval;

	dev_dbg(cfg->dev, "%s(): <-- SET LISTEN INTERVAL(%u) len(%u)\n", 
		__func__, interval, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_set_security_mode_req(const struct mt7697_cfg80211_info* cfg, 
                                    u8 auth_mode, u8 encrypt_type)
{
	struct mt7697_set_security_mode_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_security_mode_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_SECURITY_MODE_REQ;
	req.port = cfg->port_type;
	req.auth_mode = auth_mode;
	req.encrypt_type = encrypt_type;

	dev_dbg(cfg->dev, 
		"%s(): <-- SET SECURITY MODE auth/encrypt(%u/%u) len(%u)\n", 
		__func__, auth_mode, encrypt_type, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_get_security_mode_req(const struct mt7697_cfg80211_info* cfg, 
                                    u32 if_idx)
{
	struct mt7697_get_security_mode_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_get_security_mode_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_GET_SECURITY_MODE_REQ;
	req.if_idx = if_idx;
	req.port = cfg->port_type;

	dev_dbg(cfg->dev, "%s(): <-- GET SECURITY MODE len(%u)\n", 
		__func__, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_scan_req(const struct mt7697_cfg80211_info *cfg, u32 if_idx, 
	               const struct cfg80211_scan_request *req)
{
	struct mt7697_scan_req scan_req;
	int ret;

	memset(&scan_req, 0, sizeof(scan_req));
	scan_req.cmd.len = sizeof(struct mt7697_scan_req);
	scan_req.cmd.grp = MT7697_CMD_GRP_80211;
	scan_req.cmd.type = MT7697_CMD_SCAN_REQ;
	scan_req.if_idx = if_idx;
	scan_req.mode = MT7697_WIFI_SCAN_MODE_FULL;
	scan_req.option = MT7697_WIFI_SCAN_OPTION_FORCE_ACTIVE;

	dev_dbg(cfg->dev, "%s(): # ssids(%d)\n", __func__, req->n_ssids);
	WARN_ON(req->n_ssids > 1);
	if (req->n_ssids) {
		dev_dbg(cfg->dev, "%s(): ssid len(%d)\n", 
			__func__, req->ssids[0].ssid_len);
		scan_req.ssid_len = req->ssids[0].ssid_len;
		print_hex_dump(KERN_DEBUG, DRVNAME" SSID ", 
			DUMP_PREFIX_OFFSET, 16, 1, req->ssids[0].ssid, 
			req->ssids[0].ssid_len, 0);
		memcpy(scan_req.ssid, req->ssids[0].ssid, 
			req->ssids[0].ssid_len);
	}

	dev_dbg(cfg->dev, "%s(): <-- START SCAN len(%u)\n", 
		__func__, scan_req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&scan_req, 
		LEN_TO_WORD(scan_req.cmd.len));
	if (ret != LEN_TO_WORD(scan_req.cmd.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(scan_req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_scan_stop_req(const struct mt7697_cfg80211_info* cfg)
{
	struct mt7697_scan_stop req;
	int ret;

	req.len = sizeof(struct mt7697_scan_stop);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_SCAN_STOP;
	
	dev_dbg(cfg->dev, "%s(): <-- STOP SCAN len(%u)\n", __func__, req.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.len));
	if (ret != LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_disconnect_req(const struct mt7697_cfg80211_info *cfg, 
                             const u8 *addr)
{
	struct mt7697_disconnect_req req;
	int ret;

	memset(&req, 0, sizeof(req));
	req.cmd.len = sizeof(struct mt7697_disconnect_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_DISCONNECT_REQ;

	if (addr) {
		req.port = MT7697_PORT_AP;
		memcpy(req.addr, addr, ETH_ALEN);
	}
	else
		req.port = MT7697_PORT_STA;

	dev_dbg(cfg->dev, "%s(): <-- DISCONNECT len(%u)\n", 
		__func__, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_wr_tx_raw_packet(struct mt7697_cfg80211_info* cfg, 
			    const u8* data, u32 len)
{
	int ret;

	cfg->tx_req.cmd.len = sizeof(struct mt7697q_cmd_hdr) + sizeof(len) + len;
	cfg->tx_req.len = len;
	WARN_ON(len > sizeof(cfg->tx_req.data));
        memcpy(cfg->tx_req.data, data, len);

	dev_dbg(cfg->dev, "%s(): <-- TX RAW PKT len(%u)\n", 
		__func__, cfg->tx_req.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&cfg->tx_req, 
		LEN_TO_WORD(LEN32_ALIGNED(cfg->tx_req.cmd.len)));
	if (ret != LEN_TO_WORD(LEN32_ALIGNED(cfg->tx_req.cmd.len))) {
		dev_err(cfg->dev, "%s(): write() failed(%d != %d)\n", 
			__func__, ret,
			LEN_TO_WORD(LEN32_ALIGNED(cfg->tx_req.cmd.len)));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}
