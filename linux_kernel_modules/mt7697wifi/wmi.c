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

static int mt7697_proc_get_psk(struct mt7697_cfg80211_info *cfg)
{
	u32 len = 0;
	int ret = 0;

	dev_dbg(cfg->dev, "%s: --> PSK(%u)\n", __func__, cfg->rsp.cmd.len);
	if (cfg->rsp.cmd.len != sizeof(struct mt7697_get_psk_rsp)) {
		dev_err(cfg->dev, "%s: invalid PSK rsp len(%u != %u)\n", 
			__func__, cfg->rsp.cmd.len,
			sizeof(struct mt7697_get_psk_rsp));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&len, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	else if (!len || len > MT7697_PASSPHRASE_LEN) {
		dev_err(cfg->dev, "%s: invalid PSK length(%d)\n", 
			__func__, len);
		ret = -EINVAL;
       		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s: PSK length(%d)\n", __func__, len);
	memset(cfg->psk, 0, MT7697_PASSPHRASE_LEN);
	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&cfg->psk, 
		MT7697_QUEUE_LEN_TO_WORD(MT7697_PASSPHRASE_LEN));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(MT7697_PASSPHRASE_LEN)) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(MT7697_PASSPHRASE_LEN));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	dev_dbg(cfg->dev, "%s: PSK('%s')\n", __func__, cfg->psk);

cleanup:
	return ret;
}

static int mt7697_proc_mac_addr(struct mt7697_cfg80211_info *cfg)
{
	u8 addr[MT7697_LEN32_ALIGNED(ETH_ALEN)];
	struct wireless_dev *wdev;
	int ret = 0;

	dev_dbg(cfg->dev, "%s: --> MAC ADDRESS(%u)\n", 
		__func__, cfg->rsp.cmd.len);
	if (cfg->rsp.cmd.len != sizeof(struct mt7697_mac_addr_rsp)) {
		dev_err(cfg->dev, 
			"%s: invalid MAC address rsp len(%u != %u)\n",
			__func__, cfg->rsp.cmd.len, 
			sizeof(struct mt7697_mac_addr_rsp));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&addr, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(addr)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(addr))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(sizeof(addr)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	print_hex_dump(KERN_DEBUG, DRVNAME" MAC address ", 
		DUMP_PREFIX_OFFSET, 16, 1, addr, ETH_ALEN, 0);
	memcpy(cfg->mac_addr.addr, addr, ETH_ALEN);

	rtnl_lock();

	wdev = mt7697_interface_add(cfg, "wlan%d", NL80211_IFTYPE_STATION, 0);

	rtnl_unlock();

	if (!wdev) {
		dev_err(cfg->dev, "%s: mt7697_interface_add() failed\n", 
			__func__);
		ret = -ENOMEM;
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s: name/type('%s'/%u) netdev(0x%p), cfg(0x%p)\n",
		__func__, wdev->netdev->name, wdev->iftype, wdev->netdev, cfg);

cleanup:
	return ret;
}

static int mt7697_proc_get_wireless_mode(struct mt7697_cfg80211_info *cfg)
{
	u32 wireless_mode;
	int ret = 0;

	dev_dbg(cfg->dev, "%s: --> WIRELESS MODE(%u)\n", __func__, cfg->rsp.cmd.len);
	if (cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr) != sizeof(wireless_mode)) {
		dev_err(cfg->dev, "%s: invalid wireless mode rsp len(%u != %u)\n",
			__func__, 
			cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr),
			sizeof(wireless_mode));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&wireless_mode, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(wireless_mode)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(wireless_mode))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(sizeof(wireless_mode)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	dev_dbg(cfg->dev, "%s: wireless mode(%u)\n", __func__, wireless_mode);
	cfg->hw_wireless_mode = wireless_mode;

cleanup:
	return ret;
}

static int mt7697_proc_get_cfg(struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_wifi_config_t *wifi_cfg;
	u8* rd_buf = NULL;
	int ret = 0;

	dev_dbg(cfg->dev, "%s: --> CONFIG(%u)\n", __func__, cfg->rsp.cmd.len);
	if (cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr) != 
		MT7697_LEN32_ALIGNED(sizeof(struct mt7697_wifi_config_t))) {
		dev_err(cfg->dev, "%s: invalid cfg rsp len(%u != %u)\n", 
			__func__, 
			cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr),
			MT7697_LEN32_ALIGNED(sizeof(struct mt7697_wifi_config_t)));
		ret = -EINVAL;
       		goto cleanup;
	}

	rd_buf = kzalloc(
		MT7697_LEN32_ALIGNED(sizeof(struct mt7697_wifi_config_t)), 
		GFP_KERNEL);
	if (!rd_buf) {
		dev_err(cfg->dev, "%s: kzalloc() failed\n", __func__);
		ret = -ENOMEM;
       		goto cleanup;
	}
	
	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)rd_buf, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_wifi_config_t)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_wifi_config_t))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_wifi_config_t)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	wifi_cfg = (struct mt7697_wifi_config_t*)rd_buf;
	dev_dbg(cfg->dev, "%s: operation mode(%u)\n", 
		__func__, wifi_cfg->opmode);
	if ((wifi_cfg->opmode == MT7697_WIFI_MODE_STA_ONLY) || 
		(wifi_cfg->opmode == MT7697_WIFI_MODE_REPEATER)) {
		dev_dbg(cfg->dev, "%s: STA ssid len(%u)\n", 
			__func__, wifi_cfg->sta_config.ssid_length);
		if (wifi_cfg->sta_config.ssid_length > 0) {
			if (wifi_cfg->sta_config.ssid_length > IEEE80211_MAX_SSID_LEN) {
				dev_err(cfg->dev, 
					"%s: invalid STA SSID len(%u > %u)\n",
					__func__,  
					wifi_cfg->sta_config.ssid_length, 
					IEEE80211_MAX_SSID_LEN);
				ret = -EINVAL;
				goto cleanup;
			}

			dev_dbg(cfg->dev, "%s: STA ssid('%s')\n", 
				__func__, wifi_cfg->sta_config.ssid);
		}
	
		if (wifi_cfg->sta_config.bssid_present) {
			print_hex_dump(KERN_DEBUG, DRVNAME"STA BSSID ", DUMP_PREFIX_OFFSET, 
				16, 1, wifi_cfg->sta_config.bssid, ETH_ALEN, 0);
		}

		dev_dbg(cfg->dev, "%s: STA passphrase len(%u)\n", 
			__func__, wifi_cfg->sta_config.password_length);
		if (wifi_cfg->sta_config.password_length > 0) {
			if (wifi_cfg->sta_config.password_length > MT7697_WIFI_LENGTH_PASSPHRASE) {
				dev_err(cfg->dev, 
					"%s: invalid STA passphrase len(%u > %u)\n",
					__func__,  
					wifi_cfg->sta_config.password_length, 
					MT7697_WIFI_LENGTH_PASSPHRASE);
				ret = -EINVAL;
				goto cleanup;
			}

			dev_dbg(cfg->dev, "%s: STA passphrase('%s')\n", 
				__func__, wifi_cfg->sta_config.password);
		}
	}

	if ((wifi_cfg->opmode == MT7697_WIFI_MODE_AP_ONLY) || 
		(wifi_cfg->opmode == MT7697_WIFI_MODE_REPEATER)) {
		dev_dbg(cfg->dev, "%s: AP ssid len(%u)\n", 
			__func__, wifi_cfg->ap_config.ssid_length);
		if (wifi_cfg->ap_config.ssid_length > 0) {
			if (wifi_cfg->ap_config.ssid_length > IEEE80211_MAX_SSID_LEN) {
				dev_err(cfg->dev, 
					"%s: invalid AP SSID len(%u > %u)\n",
					__func__,  
					wifi_cfg->ap_config.ssid_length, 
					IEEE80211_MAX_SSID_LEN);
				ret = -EINVAL;
				goto cleanup;
			}

			dev_dbg(cfg->dev, "%s: AP ssid('%s')\n", 
				__func__, wifi_cfg->ap_config.ssid);
		}

		dev_dbg(cfg->dev, "%s: AP passphrase len(%u)\n", 
			__func__, wifi_cfg->ap_config.password_length);
		if (wifi_cfg->ap_config.password_length > 0) {
			if (wifi_cfg->ap_config.password_length > MT7697_WIFI_LENGTH_PASSPHRASE) {
				dev_err(cfg->dev, 
					"%s: invalid AP passphrase len(%u > %u)\n",
					__func__, 
					wifi_cfg->ap_config.password_length, 
					MT7697_WIFI_LENGTH_PASSPHRASE);
				ret = -EINVAL;
				goto cleanup;
			}

			dev_dbg(cfg->dev, "%s: AP passphrase('%s')\n", 
				__func__, wifi_cfg->ap_config.password);
		}

		dev_dbg(cfg->dev, "%s: AP auth mode(%u) encrypt type(%u)\n", 
			__func__, wifi_cfg->ap_config.auth_mode, 
			wifi_cfg->ap_config.encrypt_type);
		dev_dbg(cfg->dev, "%s: AP channel(%u) bandwidth(%u)\n",
			__func__, wifi_cfg->ap_config.channel, 
			wifi_cfg->ap_config.bandwidth);
		if (wifi_cfg->ap_config.bandwidth == MT7697_WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_40MHZ) {
			dev_dbg(cfg->dev, "%s: AP bandwidth ext(%u)\n", 
				__func__, wifi_cfg->ap_config.bandwidth_ext);
		}
	}

	memcpy(&cfg->wifi_config, cfg, sizeof(struct mt7697_wifi_config_t));

cleanup:
	if (rd_buf) kfree(rd_buf);
	return ret;
}

static int mt7697_proc_get_rx_filter(struct mt7697_cfg80211_info *cfg)
{
	u32 rx_filter;
	int ret = 0;

	dev_dbg(cfg->dev, "%s: --> GET RX FILTER(%u)\n", 
		__func__, cfg->rsp.cmd.len);
	if (cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr) != sizeof(rx_filter)) {
		dev_err(cfg->dev, "%s: invalid rx filter rsp len(%u != %u)\n",
			__func__, 
			cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr),
			sizeof(rx_filter));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&rx_filter, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(rx_filter)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(rx_filter))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(sizeof(rx_filter)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	cfg->rx_filter = rx_filter;
	dev_dbg(cfg->dev, "%s: rx filter(0x%08x)\n", __func__, cfg->rx_filter);

cleanup:
	return ret;
}

static int mt7697_proc_get_smart_conn_filter(struct mt7697_cfg80211_info *cfg)
{
	u32 smart_conn_filter;
	int ret = 0;

	dev_dbg(cfg->dev, "%s: --> GET SMART CONN FILTER(%u)\n", 
		__func__, cfg->rsp.cmd.len);
	if (cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr) != sizeof(smart_conn_filter)) {
		dev_err(cfg->dev, "%s: invalid rx filter rsp len(%u != %u)\n",
			__func__, 
			cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr),
			sizeof(smart_conn_filter));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&smart_conn_filter, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(smart_conn_filter)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(smart_conn_filter))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(sizeof(smart_conn_filter)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	cfg->smart_conn_filter = smart_conn_filter;
	dev_dbg(cfg->dev, "%s: smart connection filter(%u)\n", 
		__func__, cfg->smart_conn_filter);

cleanup:
	return ret;
}

static int mt7697_proc_get_radio_state(struct mt7697_cfg80211_info *cfg)
{
	u32 state;
	int ret = 0;

	dev_dbg(cfg->dev, "%s: --> GET RADIO STATE(%u)\n", 
		__func__, cfg->rsp.cmd.len);
	if (cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr) != sizeof(state)) {
		dev_err(cfg->dev, "%s: invalid get radio state rsp len(%u != %u)\n", 
			__func__, 
			cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr),
			sizeof(state));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&state, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(state)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(state))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(sizeof(state)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	cfg->radio_state = state;
	dev_dbg(cfg->dev, "%s: radio state(%u)\n", __func__, cfg->radio_state);

cleanup:
	return ret;
}

static int mt7697_proc_get_listen_interval(struct mt7697_cfg80211_info *cfg)
{
	u32 interval;
	int ret = 0;

	dev_dbg(cfg->dev, "%s: --> GET LISTEN INTERVAL(%u)\n", 
		__func__, cfg->rsp.cmd.len);
	if (cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr) != sizeof(interval)) {
		dev_err(cfg->dev, 
			"%s: invalid get listen interval rsp len(%u != %u)\n",
			__func__, 
			cfg->rsp.cmd.len - sizeof(struct mt7697_rsp_hdr),
			sizeof(interval));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&interval, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(interval)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(interval))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(sizeof(interval)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	cfg->listen_interval = interval;
	dev_dbg(cfg->dev, "%s: listen interval(%u)\n", 
		__func__, cfg->listen_interval);

cleanup:
	return ret;
}

static int mt7697_proc_scan_rsp(struct mt7697_cfg80211_info *cfg)
{
	struct ieee80211_mgmt *rx_mgmt_frame;
	s32 rssi;
	u32 ch;
        u32 probe_rsp_len;
	int ret = 0;
	__le16 fc;

	dev_dbg(cfg->dev, "%s: --> SCAN ITEM(%u)\n", 
		__func__, cfg->rsp.cmd.len);

	if (cfg->rsp.cmd.len <= sizeof(struct mt7697_scan_rsp)) {
		dev_err(cfg->dev, "%s: invalid scan rsp len(%u <= %u)\n", 
			__func__, cfg->rsp.cmd.len, 
			sizeof(struct mt7697_scan_rsp));
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (s32*)&rssi, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(s32)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(s32))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(sizeof(s32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s: rssi(%d)\n", __func__, rssi);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&ch, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s: channel(%u)\n", __func__, ch);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&probe_rsp_len, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	if (probe_rsp_len > IEEE80211_MAX_DATA_LEN) {
		dev_err(cfg->dev, "%s: invalid probe rsp len(%d > %d)\n", 
			__func__, probe_rsp_len, IEEE80211_MAX_DATA_LEN);
		ret = -EINVAL;
       		goto cleanup;
	}
	dev_dbg(cfg->dev, "%s: probe rsp len(%u)\n", __func__, probe_rsp_len);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)cfg->probe_data, 
		MT7697_QUEUE_LEN_TO_WORD(MT7697_LEN32_ALIGNED(probe_rsp_len)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(MT7697_LEN32_ALIGNED(probe_rsp_len))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(MT7697_LEN32_ALIGNED(probe_rsp_len)));
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
			dev_err(cfg->dev, "%s: invalid channel(%u)\n",
				__func__, ch);
			ret = -EINVAL;
			goto cleanup;
		}

		freq = ieee80211_channel_to_frequency(ch, band->band);
		if (!freq) {
			dev_err(cfg->dev, 
				"%s: ieee80211_channel_to_frequency() failed\n", 
				__func__);
			ret = -EINVAL;
			goto cleanup;	
		}

		channel = ieee80211_get_channel(cfg->wiphy, freq);		
		if (!channel) {
			dev_err(cfg->dev, 
				"%s: ieee80211_get_channel() failed\n", 
				__func__);
			ret = -EINVAL;
			goto cleanup;
		}

		bss = cfg80211_inform_bss_frame(cfg->wiphy, channel, 
			rx_mgmt_frame, probe_rsp_len, rssi * 100, 
			GFP_ATOMIC);	
		if (!bss) {
			dev_err(cfg->dev, 
				"%s: cfg80211_inform_bss_frame() failed\n",
				__func__);
			ret = -ENOMEM;
			goto cleanup;
		}

		print_hex_dump(KERN_DEBUG, DRVNAME" BSS BSSID ", 
			DUMP_PREFIX_OFFSET, 16, 1, bss->bssid, ETH_ALEN, 0);
		dev_dbg(cfg->dev, "%s: BSS signal(%d) scan width(%u) cap(0x%08x)\n", 
			__func__, bss->signal, bss->scan_width, bss->capability);
		if (bss->channel) {
			dev_dbg(cfg->dev, 
				"%s: BSS channel band(%u) center freq(%u)\n", 
				__func__, bss->channel->band, 
				bss->channel->center_freq);
		} 
	}
	else {
		dev_err(cfg->dev, "%s: Rx unsupported mgmt frame\n", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

cleanup:
	return ret;
}

static int mt7697_proc_scan_complete(struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_vif *vif;
	u16 if_idx;
	int ret;

	dev_dbg(cfg->dev, "%s: --> SCAN COMPLETE\n", __func__);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&if_idx, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s: if idx(%d)\n", __func__, if_idx);

	vif = mt7697_get_vif_by_idx(cfg, if_idx);
	if (!vif) {
		dev_err(cfg->dev, "%s: mt7697_get_vif_by_idx(%u) failed\n",
			    __func__, if_idx);
		ret = -EINVAL;
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s: vif(%u)\n", __func__, vif->fw_vif_idx);
	cfg80211_scan_done(vif->scan_req, false);
	vif->scan_req = NULL;

cleanup:
	return ret;
}

static int mt7697_proc_connect(struct mt7697_cfg80211_info *cfg)
{
	u8 bssid[MT7697_LEN32_ALIGNED(ETH_ALEN)];
	struct mt7697_vif *vif;
	enum mt7697_wifi_rx_filter_t rx_filter;
	u32 if_idx;
	u32 channel;
	int ret;

	dev_dbg(cfg->dev, "%s: --> CONNECT RSP(%u)\n", 
		__func__, cfg->rsp.cmd.len);
	if (cfg->rsp.cmd.len != sizeof(struct mt7697_connect_rsp)) {
		dev_err(cfg->dev, "%s: invalid connect rsp len(%d != %d)\n",
			__func__, cfg->rsp.cmd.len, 
			sizeof(struct mt7697_connect_rsp));
		ret = -EINVAL;
		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&if_idx, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s: if idx(%d)\n", __func__, if_idx);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&channel, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s: channel(%d)\n", __func__, channel);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)bssid, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(bssid)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(bssid))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(sizeof(bssid)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	print_hex_dump(KERN_DEBUG, DRVNAME" BSSID ", 
		DUMP_PREFIX_OFFSET, 16, 1, bssid, ETH_ALEN, 0);

	vif = mt7697_get_vif_by_idx(cfg, if_idx);
	if (!vif) {
		dev_err(cfg->dev, "%s: mt7697_get_vif_by_idx(%u) failed\n",
			__func__, if_idx);
		ret = -EINVAL;
		goto cleanup;
	}

	rx_filter |= BIT(MT7697_WIFI_RX_FILTER_RM_FRAME_REPORT_EN);
    	rx_filter &= ~BIT(MT7697_WIFI_RX_FILTER_DROP_NOT_MY_BSSID);
    	rx_filter &= ~BIT(MT7697_WIFI_RX_FILTER_DROP_NOT_UC2ME);
    	rx_filter &= ~BIT(MT7697_WIFI_RX_FILTER_DROP_MC_FRAME);
	if (rx_filter != cfg->rx_filter) {
		ret = mt7697_send_set_rx_filter_req(cfg, cfg->rx_filter);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_send_set_rx_filter_req() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

		cfg->rx_filter = rx_filter;
	}

	if (!cfg->smart_conn_filter) {
		ret = mt7697_send_set_smart_conn_filter_req(cfg, true);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_send_set_smart_conn_filter_req() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

		cfg->smart_conn_filter = true;
	}

	ret = mt7697_send_register_rx_hndlr_req(cfg);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s: mt7697_send_register_rx_hndlr_req() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	dev_dbg(cfg->dev, "%s: vif(%u)\n", __func__, vif->fw_vif_idx);
	ret = mt7697_cfg80211_connect_event(vif, bssid, channel);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s: mt7697_cfg80211_connect_event() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

cleanup:
	return ret;
}

static int mt7697_proc_disconnect(struct mt7697_cfg80211_info *cfg)
{
	u8 bssid[MT7697_LEN32_ALIGNED(ETH_ALEN)];
	struct mt7697_vif *vif;
	u16 if_idx;
	u16 proto_reason = 0;
	int ret;

	dev_dbg(cfg->dev, "%s: --> DISCONNECT RSP(%u)\n", __func__, cfg->rsp.cmd.len);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&if_idx, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(u32))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(sizeof(u32)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}
	dev_dbg(cfg->dev, "%s: if idx(%d)\n", __func__, if_idx);

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)bssid, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(bssid)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(bssid))) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(sizeof(bssid)));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	print_hex_dump(KERN_DEBUG, DRVNAME" BSSID ", 
		DUMP_PREFIX_OFFSET, 16, 1, bssid, ETH_ALEN, 0);

	vif = mt7697_get_vif_by_idx(cfg, if_idx);
	if (!vif) {
		dev_err(cfg->dev, "%s: mt7697_get_vif_by_idx(%u) failed\n",
			    __func__, if_idx);
		ret = -EINVAL;
		goto cleanup;
	}

	ret = mt7697_send_unregister_rx_hndlr_req(cfg);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s: mt7697_send_unregister_rx_hndlr_req() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	dev_dbg(cfg->dev, "%s: vif(%u)\n", __func__, vif->fw_vif_idx);
	if (vif->sme_state == SME_CONNECTING) {
		cfg80211_connect_result(vif->ndev, bssid, 
					NULL, 0,
					NULL, 0,
					WLAN_STATUS_UNSPECIFIED_FAILURE,
					GFP_KERNEL);
	} else if (vif->sme_state == SME_CONNECTED) {
		cfg80211_disconnected(vif->ndev, proto_reason,
				      NULL, 0, GFP_KERNEL);
	}

	vif->sme_state = SME_DISCONNECTED;
	spin_lock_bh(&vif->if_lock);
	clear_bit(CONNECT_PEND, &vif->flags);
	clear_bit(CONNECTED, &vif->flags);
	dev_dbg(cfg->dev, "%s: vif sme_state(%u), flags(0x%08lx)\n",
		__func__, vif->sme_state, vif->flags);
	spin_unlock_bh(&vif->if_lock);

cleanup:
	return ret;
}

static int mt7697_rx_raw(struct mt7697_cfg80211_info *cfg)
{
	int ret;

	dev_dbg(cfg->dev, "%s: --> RX RAW(%u)\n", __func__, cfg->rsp.cmd.len);

	if (cfg->rsp.cmd.len <= sizeof(struct mt7697_rsp_hdr)) {
		dev_err(cfg->dev, "%s: invalid rx raw len(%u <= %u)\n", 
			__func__, cfg->rsp.cmd.len, 
			sizeof(struct mt7697_rsp_hdr));
		ret = -EINVAL;
       		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s: len(%d)\n", __func__, cfg->rsp.result);
	if (cfg->rsp.result > IEEE80211_MAX_FRAME_LEN) {
		dev_err(cfg->dev, "%s: invalid rx data len(%u <= %u)\n", 
			__func__, cfg->rsp.result, IEEE80211_MAX_FRAME_LEN);
		ret = -EINVAL;
       		goto cleanup;
	}

	ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)cfg->rx_data, 
		MT7697_QUEUE_LEN_TO_WORD(cfg->rsp.result));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(cfg->rsp.result)) {
		dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(cfg->rsp.result));
		ret = (ret < 0) ? ret:-EIO;
       		goto cleanup;
    	}

	print_hex_dump(KERN_DEBUG, DRVNAME" RX DATA ", 
		DUMP_PREFIX_OFFSET, 16, 1, cfg->rx_data, cfg->rsp.result, 0);

	/* TODO: interface index come from MT7697 */
	ret = mt7697_rx_data(cfg, 0);
	if (ret) {
		dev_err(cfg->dev, "%s: mt7697_rx_data() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

cleanup:
	return ret;
}

static int mt7697_proc_80211cmd(struct mt7697_cfg80211_info *cfg)
{
	int ret;

	switch (cfg->rsp.cmd.type) {
	case MT7697_CMD_GET_PSK_RSP:
		ret = mt7697_proc_get_psk(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_get_psk() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_MAC_ADDR_RSP:
		ret = mt7697_proc_mac_addr(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_mac_addr() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_GET_WIRELESS_MODE_RSP:
		ret = mt7697_proc_get_wireless_mode(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_get_wireless_mode() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_GET_CFG_RSP:
		ret = mt7697_proc_get_cfg(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_get_cfg() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_GET_RX_FILTER_RSP:
		ret = mt7697_proc_get_rx_filter(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_get_rx_filter() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_GET_SMART_CONN_FILTER_RSP:
		ret = mt7697_proc_get_smart_conn_filter(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_get_smart_conn_filter() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_GET_RADIO_STATE_RSP:
		ret = mt7697_proc_get_radio_state(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"mt7697_proc_get_radio_state() failed(%d)\n", 
				ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_GET_LISTEN_INTERVAL_RSP:
		ret = mt7697_proc_get_listen_interval(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_get_listen_interval() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_SCAN_RSP:
		ret = mt7697_proc_scan_rsp(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_scan_rsp() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_SCAN_COMPLETE:
		ret = mt7697_proc_scan_complete(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_scan_complete() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_CONNECT_RSP:
		ret = mt7697_proc_connect(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_connect() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_DISCONNECT_RSP:
		ret = mt7697_proc_disconnect(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_proc_disconnect() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_RX_RAW:
		ret = mt7697_rx_raw(cfg);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_rx_raw() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}
		break;

	case MT7697_CMD_SET_PSK_RSP:
		dev_dbg(cfg->dev, "%s: --> SET PSK(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

	case MT7697_CMD_SET_LISTEN_INTERVAL_RSP:
		dev_dbg(cfg->dev, "%s: --> SET LISTEN INTERVAL(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

        case MT7697_CMD_SET_OP_MODE_RSP:
		dev_dbg(cfg->dev, "%s: --> SET OP MODE(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

	case MT7697_CMD_SET_WIRELESS_MODE_RSP:
		dev_dbg(cfg->dev, "%s: --> SET WIRELESS MODE(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

	case MT7697_CMD_SET_RADIO_STATE_RSP:
		dev_dbg(cfg->dev, "%s: --> SET RADIO STATE(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

	case MT7697_CMD_SET_RX_FILTER_RSP:
		dev_dbg(cfg->dev, "%s: --> SET RX FILTER(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

        case MT7697_CMD_SET_SMART_CONN_FILTER_RSP:
		dev_dbg(cfg->dev, "%s: --> SET SMART CONN FILTER(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

	case MT7697_CMD_SET_SECURITY_MODE_RSP:
		dev_dbg(cfg->dev, "%s: --> SET SECURITY MODE(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

	case MT7697_CMD_SCAN_STOP_RSP:
		dev_dbg(cfg->dev, "%s: --> SCAN STOP(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

	case MT7697_CMD_REGISTER_RX_HNDLR_RSP:
		dev_dbg(cfg->dev, "%s: --> REGISTER RX HANDLER(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

	case MT7697_CMD_UNREGISTER_RX_HNDLR_RSP:
		dev_dbg(cfg->dev, "%s: --> UNREGISTER RX HANDLER(%u)\n", 
			__func__, cfg->rsp.cmd.len);
		break;

	default:
		dev_err(cfg->dev, "%s: unsupported cmd(%d)\n", 
			__func__, cfg->rsp.cmd.type);
		ret = -EINVAL;
		goto cleanup;
	}

cleanup:
	return ret;
}

int mt7697_send_init(struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_cmd_hdr req;
	int ret;

	req.len = sizeof(struct mt7697_cmd_hdr);
	req.grp = MT7697_CMD_GRP_QUEUE;
	req.type = MT7697_CMD_INIT;

 	dev_dbg(cfg->dev, "%s: <-- QUEUE INIT\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_reset(struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_cmd_hdr req;
	int ret;

	req.len = sizeof(struct mt7697_cmd_hdr);
	req.grp = MT7697_CMD_GRP_QUEUE;
	req.type = MT7697_CMD_RESET;

 	dev_dbg(cfg->dev, "%s: <-- QUEUE RESET\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_get_wireless_mode_req(struct mt7697_cfg80211_info *cfg, u8 port)
{
	struct mt7697_get_wireless_mode_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_get_wireless_mode_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_GET_WIRELESS_MODE_REQ;
	req.port = port;

	dev_dbg(cfg->dev, "%s: <-- GET WIRELESS MODE PORT(%u)\n", __func__, port);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_set_wireless_mode_req(struct mt7697_cfg80211_info *cfg, u8 port, u8 mode)
{
	struct mt7697_set_wireless_mode_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_wireless_mode_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_WIRELESS_MODE_REQ;
	req.port = port;
	req.mode = mode;

	dev_dbg(cfg->dev, "%s: <-- SET WIRELESS MODE port(%u)\n", __func__, port);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_get_psk_req(struct mt7697_cfg80211_info *cfg, u8 port)
{
	struct mt7697_get_psk_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_get_psk_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_GET_PSK_REQ;
	req.port = port;

	dev_dbg(cfg->dev, "%s: <-- GET PSK port(%u)\n", __func__, port);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_set_psk_req(struct mt7697_cfg80211_info *cfg, u8 port, 
			    const u8 psk[])
{
	struct mt7697_set_psk_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_psk_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_PSK_REQ;
	req.port = port;
	req.len = strlen(psk);
	memcpy(req.psk, psk, req.len);

	dev_dbg(cfg->dev, "%s: <-- SET PSK port(%u)\n", __func__, port);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_set_psk_req)));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_set_psk_req))) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, 
			MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_set_psk_req)));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_mac_addr_req(struct mt7697_cfg80211_info *cfg, u8 port)
{
	struct mt7697_mac_addr_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_mac_addr_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_MAC_ADDR_REQ;
	req.port = port;

	dev_dbg(cfg->dev, "%s: <-- GET MAC ADDRESS port(%u)\n", __func__, port);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_set_op_mode_req(struct mt7697_cfg80211_info* cfg, u8 opmode)
{
	struct mt7697_set_op_mode_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_op_mode_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_OP_MODE_REQ;
	req.opmode = opmode;

	dev_dbg(cfg->dev, "%s: <-- SET OPMODE(%u)\n", __func__, opmode);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_cfg_req(struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_cfg_req req;
	int ret;

	req.len = sizeof(struct mt7697_cfg_req);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_GET_CFG_REQ;

	dev_dbg(cfg->dev, "%s: <-- GET CONFIG\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_register_rx_hndlr_req(struct mt7697_cfg80211_info* cfg)
{
    	struct mt7697_register_rx_hndlr_req req;
	int ret;

	req.len = sizeof(struct mt7697_register_rx_hndlr_req);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_REGISTER_RX_HNDLR_REQ;

	dev_dbg(cfg->dev, "%s: <-- REGISTER RX HANDLER\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_unregister_rx_hndlr_req(struct mt7697_cfg80211_info* cfg)
{
    	struct mt7697_unregister_rx_hndlr_req req;
	int ret;

	req.len = sizeof(struct mt7697_unregister_rx_hndlr_req);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_UNREGISTER_RX_HNDLR_REQ;

	dev_dbg(cfg->dev, "%s: <-- UNREGISTER RX HANDLER\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_get_radio_state_req(struct mt7697_cfg80211_info* cfg)
{
	struct mt7697_get_radio_state_req req;
	int ret;

	req.len = sizeof(struct mt7697_get_radio_state_req);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_GET_RADIO_STATE_REQ;

	dev_dbg(cfg->dev, "%s: <-- GET RADIO STATE\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_set_radio_state_req(struct mt7697_cfg80211_info* cfg, u8 state)
{
	struct mt7697_set_radio_state_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_radio_state_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_RADIO_STATE_REQ;
	req.state = state;

	dev_dbg(cfg->dev, "%s: <-- SET RADIO STATE(%u)\n", __func__, state);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_get_rx_filter_req(struct mt7697_cfg80211_info* cfg)
{
	struct mt7697_get_rx_filter_req req;
	int ret;

	req.len = sizeof(struct mt7697_get_rx_filter_req);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_GET_RX_FILTER_REQ;

	dev_dbg(cfg->dev, "%s: <-- GET RX FILTER\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_set_rx_filter_req(struct mt7697_cfg80211_info* cfg, 
				  u32 rx_filter)
{
	struct mt7697_set_rx_filter_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_rx_filter_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_RX_FILTER_REQ;
	req.rx_filter = rx_filter;

	dev_dbg(cfg->dev, "%s: <-- SET RX FILTER(0x%08x)\n", 
		__func__, rx_filter);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_get_smart_conn_filter_req(struct mt7697_cfg80211_info* cfg)
{
	struct mt7697_get_smart_conn_filter_req req;
	int ret;

	req.len = sizeof(struct mt7697_get_smart_conn_filter_req);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_GET_SMART_CONN_FILTER_REQ;

	dev_dbg(cfg->dev, "%s: <-- GET SMART CONN FILTER\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_set_smart_conn_filter_req(struct mt7697_cfg80211_info* cfg, u8 flag)
{
	struct mt7697_set_smart_conn_filter_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_smart_conn_filter_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_SMART_CONN_FILTER_REQ;
	req.flag = flag;

	dev_dbg(cfg->dev, "%s: <-- SET SMART CONN FILTER(%u)\n", 
		__func__, flag);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_get_listen_interval_req(struct mt7697_cfg80211_info* cfg)
{
	struct mt7697_get_listen_interval_req req;
	int ret;

	req.len = sizeof(struct mt7697_get_listen_interval_req);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_GET_LISTEN_INTERVAL_REQ;

	dev_dbg(cfg->dev, "%s: <-- GET LISTEN INTERVAL\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_set_listen_interval_req(struct mt7697_cfg80211_info* cfg, 
				        u32 interval)
{
	struct mt7697_set_listen_interval_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_listen_interval_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_LISTEN_INTERVAL_REQ;
	req.interval = interval;

	dev_dbg(cfg->dev, "%s: <-- SET LISTEN INTERVAL(%u)\n", 
		__func__, interval);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_set_security_mode_req(struct mt7697_cfg80211_info* cfg, 
                                      u8 port, u8 auth_mode, u8 encrypt_type)
{
	struct mt7697_set_security_mode_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_set_security_mode_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_SET_SECURITY_MODE_REQ;
	req.port = port;
	req.auth_mode = auth_mode;
	req.encrypt_type = encrypt_type;

	dev_dbg(cfg->dev, "%s: <-- SET SECURITY MODE auth/encrypt(%u/%u)\n", 
		__func__, auth_mode, encrypt_type);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_get_security_mode_req(struct mt7697_cfg80211_info* cfg, 
                                      u32 if_idx, u8 port)
{
	struct mt7697_get_security_mode_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_get_security_mode_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_GET_SECURITY_MODE_REQ;
	req.if_idx = if_idx;
	req.port = port;

	dev_dbg(cfg->dev, "%s: <-- GET SECURITY MODE\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_scan_req(struct mt7697_cfg80211_info *cfg, u32 if_idx, 
	                 const struct cfg80211_scan_request *req)
{
	struct mt7697_scan_req scan_req;
	int ret;

	dev_dbg(cfg->dev, "%s: <-- START SCAN\n", __func__);
	memset(&scan_req, 0, sizeof(scan_req));
	scan_req.cmd.len = sizeof(struct mt7697_scan_req);
	scan_req.cmd.grp = MT7697_CMD_GRP_80211;
	scan_req.cmd.type = MT7697_CMD_SCAN_REQ;
	scan_req.if_idx = if_idx;
	scan_req.mode = MT7697_WIFI_SCAN_MODE_FULL;
	scan_req.option = MT7697_WIFI_SCAN_OPTION_FORCE_ACTIVE;

	dev_dbg(cfg->dev, "%s: # ssids(%d)\n", __func__, req->n_ssids);
	WARN_ON(req->n_ssids > 1);
	if (req->n_ssids) {
		dev_dbg(cfg->dev, "%s: ssid len(%d)\n", 
			__func__, req->ssids[0].ssid_len);
		scan_req.ssid_len = req->ssids[0].ssid_len;
		print_hex_dump(KERN_DEBUG, DRVNAME" SSID ", 
			DUMP_PREFIX_OFFSET, 16, 1, req->ssids[0].ssid, 
			req->ssids[0].ssid_len, 0);
		memcpy(scan_req.ssid, req->ssids[0].ssid, 
			req->ssids[0].ssid_len);
	}

	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&scan_req, 
		MT7697_QUEUE_LEN_TO_WORD(scan_req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(scan_req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(scan_req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_scan_stop_req(struct mt7697_cfg80211_info* cfg)
{
	struct mt7697_scan_stop req;
	int ret;

	req.len = sizeof(struct mt7697_scan_stop);
	req.grp = MT7697_CMD_GRP_80211;
	req.type = MT7697_CMD_SCAN_STOP;

	dev_dbg(cfg->dev, "%s: <-- STOP SCAN\n", __func__);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_connect_req(struct mt7697_cfg80211_info *cfg,
			    u8 port, u32 if_idx, const u8* bssid, 
			    const u8* ssid, u32 ssid_len, 
			    u32 channel)
{
	struct mt7697_connect_req req;
	int ret;

	WARN_ON(!ssid || !ssid_len);

	memset(&req, 0, sizeof(req));
	req.cmd.len = sizeof(struct mt7697_connect_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_CONNECT_REQ;
	req.if_idx = if_idx;
	req.port = port;
	req.channel = channel ? ieee80211_frequency_to_channel(channel):channel;
	memcpy(req.bssid, bssid, ETH_ALEN);
	req.ssid_len = ssid_len;
	memcpy(req.ssid, ssid, ssid_len);	

	dev_dbg(cfg->dev, "%s: <-- CONNECT(%u)\n", __func__, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_disconnect_req(struct mt7697_cfg80211_info *cfg, u32 if_idx, 
			       const u8 *addr)
{
	struct mt7697_disconnect_req req;
	int ret;

	memset(&req, 0, sizeof(req));
	req.cmd.len = sizeof(struct mt7697_disconnect_req);
	req.cmd.grp = MT7697_CMD_GRP_80211;
	req.cmd.type = MT7697_CMD_DISCONNECT_REQ;
	req.if_idx = if_idx;

	if (addr) {
		req.port = MT7697_PORT_AP;
		memcpy(req.addr, addr, ETH_ALEN);
	}
	else
		req.port = MT7697_PORT_STA;

	dev_dbg(cfg->dev, "%s: <-- DISCONNECT(%u)\n", __func__, req.cmd.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&req, 
		MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret, MT7697_QUEUE_LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

int mt7697_send_tx_raw_packet(struct mt7697_cfg80211_info* cfg, 
			      const u8* data, u32 len)
{
	int ret;

	cfg->tx_req.cmd.len = sizeof(struct mt7697_cmd_hdr) + sizeof(len) + len;
	cfg->tx_req.len = len;
	WARN_ON(len > sizeof(cfg->tx_req.data));
        memcpy(cfg->tx_req.data, data, len);

	dev_dbg(cfg->dev, "%s: <-- TX RAW PKT(%u)\n", __func__, cfg->tx_req.len);
	ret = cfg->hif_ops->write(cfg->txq_hdl, (const u32*)&cfg->tx_req, 
		MT7697_QUEUE_LEN_TO_WORD(cfg->tx_req.cmd.len));
	if (ret != MT7697_QUEUE_LEN_TO_WORD(cfg->tx_req.cmd.len)) {
		dev_err(cfg->dev, "%s: write() failed(%d != %d)\n", 
			__func__, ret,
			MT7697_QUEUE_LEN_TO_WORD(cfg->tx_req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

cleanup:
	return ret;
}

int mt7697_proc_data(void *priv)
{
	struct mt7697_cfg80211_info *cfg = (struct mt7697_cfg80211_info *)priv;
	size_t avail;
	int ret;

	ret = cfg->hif_ops->pull_wr_ptr(cfg->rxq_hdl);
	if (ret < 0) {
		dev_err(cfg->dev, "%s: pull_wr_ptr() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	/* TODO: Do not starve Tx here */
	avail = cfg->hif_ops->num_rd(cfg->rxq_hdl);	
	while (avail > MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_rsp_hdr))) {
		if (!cfg->rsp.cmd.len) {
			if (avail < MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_rsp_hdr))) {
				dev_dbg(cfg->dev, 
					"%s: queue need more data\n", __func__);
				goto cleanup;
			}

			ret = cfg->hif_ops->read(cfg->rxq_hdl, (u32*)&cfg->rsp, 
				MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_rsp_hdr)));
			if (ret != MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_rsp_hdr))) {
				dev_err(cfg->dev, "%s: read() failed(%d != %d)\n", 
					__func__, ret, 
					MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697_rsp_hdr)));
       				goto cleanup;
    			}
		
			if (cfg->rsp.result && (cfg->rsp.cmd.type != MT7697_CMD_RX_RAW)) {
				dev_warn(cfg->dev, "%s: result(%d)\n", 
					__func__, cfg->rsp.result);
			}

			ret = cfg->hif_ops->pull_wr_ptr(cfg->rxq_hdl);
			if (ret < 0) {
				dev_err(cfg->dev, "%s: pull_wr_ptr() failed(%d)\n",
					__func__, ret);
       				goto cleanup;
    			}

			avail = cfg->hif_ops->num_rd(cfg->rxq_hdl);
		}

		if (avail < MT7697_QUEUE_LEN_TO_WORD(cfg->rsp.cmd.len - 
					sizeof(struct mt7697_rsp_hdr))) {
			dev_dbg(cfg->dev, "%s: queue need more data\n", __func__);
			goto cleanup;
		}

		if (cfg->rsp.cmd.grp == MT7697_CMD_GRP_80211) {
			ret = mt7697_proc_80211cmd(cfg);
			if (ret < 0) {
				dev_err(cfg->dev, 
					"%s: mt7697_proc_80211cmd() failed(%d)\n",
					__func__, ret);
    			}
		}
		else {
			dev_err(cfg->dev, "%s: invalid grp(%d)\n", 
				__func__, cfg->rsp.cmd.grp);
			ret = -EINVAL;
			goto cleanup;
		}

		cfg->rsp.cmd.len = 0;

		ret = cfg->hif_ops->pull_wr_ptr(cfg->rxq_hdl);
		if (ret < 0) {
			dev_err(cfg->dev, "%s: pull_wr_ptr() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

		avail = cfg->hif_ops->num_rd(cfg->rxq_hdl);
	}

cleanup:
	if (ret < 0) cfg->rsp.cmd.len = 0;
	return ret;
}
