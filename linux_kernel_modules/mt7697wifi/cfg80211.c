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
#include "wmi.h"
#include "core.h"
#include "cfg80211.h"

#define RATETAB_ENT(_rate, _rateid, _flags) {   		\
	.bitrate    = (_rate),                  		\
	.flags      = (_flags),                 		\
	.hw_value   = (_rateid),                		\
}

#define CHAN2G(_channel, _freq, _flags) {   			\
	.band           	= IEEE80211_BAND_2GHZ,  	\
	.hw_value       	= (_channel),           	\
	.center_freq    	= (_freq),              	\
	.flags          	= (_flags),             	\
	.max_antenna_gain   	= 0,                		\
	.max_power      	= 30,                   	\
}

#define CHAN5G(_channel, _freq, _flags) {			\
	.band           	= IEEE80211_BAND_5GHZ,      	\
	.hw_value       	= (_channel),               	\
	.center_freq    	= (_freq),              	\
	.flags          	= (_flags),                 	\
	.max_antenna_gain   	= 0,                    	\
	.max_power      	= 30,                       	\
}

struct ieee80211_rate mt7697_rates[] = {
	RATETAB_ENT(10, 0x1, 0),
	RATETAB_ENT(20, 0x2, 0),
	RATETAB_ENT(55, 0x4, 0),
	RATETAB_ENT(110, 0x8, 0),
	RATETAB_ENT(60, 0x10, 0),
	RATETAB_ENT(90, 0x20, 0),
	RATETAB_ENT(120, 0x40, 0),
	RATETAB_ENT(180, 0x80, 0),
	RATETAB_ENT(240, 0x100, 0),
	RATETAB_ENT(360, 0x200, 0),
	RATETAB_ENT(480, 0x400, 0),
	RATETAB_ENT(540, 0x800, 0),
};

#define mt7697_a_rates     	(mt7697_rates + 4)
#define mt7697_a_rates_size    	8
#define mt7697_g_rates     	(mt7697_rates + 0)
#define mt7697_g_rates_size    	12

#define mt7697_g_htcap IEEE80211_HT_CAP_SGI_20
#define mt7697_a_htcap (IEEE80211_HT_CAP_SUP_WIDTH_20_40 | \
			IEEE80211_HT_CAP_SGI_20		 | \
			IEEE80211_HT_CAP_SGI_40)

/* 
 * MT7697 supports 2400-2497MHz 
 * See Section 2.7.3 MT76x7 Technical Reference Manual
 * https://docs.labs.mediatek.com/resource/mt7687-mt7697/en/documentation
 */
struct ieee80211_channel mt7697_2ghz_channels[] = {
	CHAN2G(1, 2412, 0),
	CHAN2G(2, 2417, 0),
	CHAN2G(3, 2422, 0),
	CHAN2G(4, 2427, 0),
	CHAN2G(5, 2432, 0),
	CHAN2G(6, 2437, 0),
	CHAN2G(7, 2442, 0),
	CHAN2G(8, 2447, 0),
	CHAN2G(9, 2452, 0),
	CHAN2G(10, 2457, 0),
	CHAN2G(11, 2462, 0),
	CHAN2G(12, 2467, 0),
	CHAN2G(13, 2472, 0),
	CHAN2G(14, 2484, 0),
};

/* 
 * MT7697 supports:
 *  - 5150-5350MHz
 *  - 5470-5725MHz
 *  - 5725-5850MHz
 *  - 5850-5925MHz
 * See Section 2.7.3 MT76x7 Technical Reference Manual
 * https://docs.labs.mediatek.com/resource/mt7687-mt7697/en/documentation
 */
struct ieee80211_channel mt7697_5ghz_a_channels[] = {
        CHAN5G(36, 5180, 0),
        CHAN5G(40, 5200, 0),
        CHAN5G(44, 5220, 0),
        CHAN5G(48, 5240, 0),
        CHAN5G(52, 5260, 0),
        CHAN5G(56, 5280, 0),
        CHAN5G(60, 5300, 0),
        CHAN5G(64, 5320, 0),
        CHAN5G(100, 5500, 0),
        CHAN5G(104, 5520, 0),
        CHAN5G(108, 5540, 0),
        CHAN5G(112, 5560, 0),
        CHAN5G(116, 5580, 0),
        CHAN5G(120, 5600, 0),
        CHAN5G(124, 5620, 0),
        CHAN5G(128, 5640, 0),
        CHAN5G(132, 5660, 0),
        CHAN5G(136, 5680, 0),
        CHAN5G(140, 5700, 0),
        CHAN5G(149, 5745, 0),
        CHAN5G(153, 5765, 0),
        CHAN5G(157, 5785, 0),
        CHAN5G(161, 5805, 0),
        CHAN5G(165, 5825, 0),
};

static struct ieee80211_supported_band mt7697_band_2ghz = {
	.n_channels = ARRAY_SIZE(mt7697_2ghz_channels),
	.channels = mt7697_2ghz_channels,
	.n_bitrates = mt7697_g_rates_size,
	.bitrates = mt7697_g_rates,
	.ht_cap.cap = mt7697_g_htcap,
	.ht_cap.ht_supported = true,
};

static struct ieee80211_supported_band mt7697_band_5ghz = {
	.n_channels = ARRAY_SIZE(mt7697_5ghz_a_channels),
	.channels = mt7697_5ghz_a_channels,
	.n_bitrates = mt7697_a_rates_size,
	.bitrates = mt7697_a_rates,
	.ht_cap.cap = mt7697_a_htcap,
	.ht_cap.ht_supported = true,
};

static const u32 mt7697_cipher_suites[] = {
        WLAN_CIPHER_SUITE_WEP40,
        WLAN_CIPHER_SUITE_WEP104,
        WLAN_CIPHER_SUITE_TKIP,
        WLAN_CIPHER_SUITE_CCMP,
        WLAN_CIPHER_SUITE_AES_CMAC,
};

static int mt7697_set_key_mgmt(struct mt7697_vif *vif, u32 key_mgmt)
{
	int ret = 0;

	dev_dbg(vif->cfg->dev, "%s: key mgmt(0x%08x)\n", __func__, key_mgmt);

	if (key_mgmt == WLAN_AKM_SUITE_PSK) {
		if (vif->auth_mode == MT7697_WIFI_AUTH_MODE_WPA)
			vif->auth_mode = MT7697_WIFI_AUTH_MODE_WPA_PSK;
		else if (vif->auth_mode == MT7697_WIFI_AUTH_MODE_WPA2)
			vif->auth_mode = MT7697_WIFI_AUTH_MODE_WPA2_PSK;
	} else if (key_mgmt == 0x00409600) {
		dev_err(vif->cfg->dev, "%s: key mgmt(0x%08x) not supported\n", 
			__func__, key_mgmt);
		ret = -ENOTSUPP;
		goto cleanup;
	} else if (key_mgmt != WLAN_AKM_SUITE_8021X) {
		vif->auth_mode = MT7697_WIFI_AUTH_MODE_OPEN;
	}

	dev_dbg(vif->cfg->dev, "%s: auth mode(%u)\n", 
		__func__, vif->auth_mode);

cleanup:
	return ret;
}

static int mt7697_set_cipher(struct mt7697_vif *vif, u32 cipher, bool ucast)
{
	enum mt7697_wifi_encrypt_type_t *_cipher = ucast ? 
		&vif->prwise_crypto : &vif->grp_crypto;
	u8 *_cipher_len = ucast ? &vif->prwise_crypto_len :
		&vif->grp_crypto_len;
	int ret = 0;

	dev_dbg(vif->cfg->dev, "%s: cipher(0x%08x), ucast(%u)\n",
		   __func__, cipher, ucast);

	switch (cipher) {
	case 0:
		*_cipher = MT7697_WIFI_ENCRYPT_TYPE_ENCRYPT_DISABLED;
		*_cipher_len = 0;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		*_cipher = MT7697_WIFI_ENCRYPT_TYPE_TKIP_ENABLED;
		*_cipher_len = 0;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		*_cipher = MT7697_WIFI_ENCRYPT_TYPE_AES_ENABLED;
		*_cipher_len = 0;
		break;
	
	case WLAN_CIPHER_SUITE_WEP104:
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_SMS4:
	default:
		dev_err(vif->cfg->dev, "cipher(0x%08x) not supported\n", cipher);
		ret = -ENOTSUPP;
		goto cleanup;
	}

	dev_dbg(vif->cfg->dev, "%s: set cipher(%u)\n", __func__, *_cipher);

cleanup:
	return ret;
}

static int mt7697_set_wpa_version(struct mt7697_vif *vif,
				  enum nl80211_wpa_versions wpa_version)
{
	int ret = 0;

	dev_dbg(vif->cfg->dev, "%s: WPA version(%u)\n", __func__, wpa_version);

	if (!wpa_version) {
		vif->auth_mode = MT7697_WIFI_AUTH_MODE_OPEN;
	} else if (wpa_version & NL80211_WPA_VERSION_2) {
		vif->auth_mode = MT7697_WIFI_AUTH_MODE_WPA2;
	} else if (wpa_version & NL80211_WPA_VERSION_1) {
		vif->auth_mode = MT7697_WIFI_AUTH_MODE_WPA;
	} else {
		dev_err(vif->cfg->dev, "%s: WPA version(%u) not supported\n", 
			__func__, wpa_version);
		ret = -ENOTSUPP;
		goto cleanup;
	}

	dev_dbg(vif->cfg->dev, "%s: auth mode(%u)\n", __func__, vif->auth_mode);

cleanup:
	return ret;
}

static struct cfg80211_bss* mt7697_add_bss_if_needed(struct mt7697_vif *vif, 
						     const u8* bssid, 
						     u32 freq)
{
	struct ieee80211_channel *chan;
	struct mt7697_cfg80211_info *cfg = vif->cfg;
	struct cfg80211_bss *bss = NULL;
	u8 *ie = NULL;
	
	dev_dbg(cfg->dev, "%s(): SSID('%s')\n", __func__, vif->ssid);
	print_hex_dump(KERN_DEBUG, DRVNAME" BSSID ", DUMP_PREFIX_OFFSET, 
		16, 1, bssid, ETH_ALEN, 0);

	dev_dbg(cfg->dev, "%s(): freq(%u)\n", __func__, freq);
	chan = ieee80211_get_channel(cfg->wiphy, freq);
	if (!chan) {
		dev_err(cfg->dev, "%s(): ieee80211_get_channel(%u) failed\n", 
			__func__, freq);
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s(): band(%u) chan(%u)\n", 
		__func__, chan->band, chan->center_freq);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,44)
	bss = cfg80211_get_bss(cfg->wiphy, chan, bssid,
			       vif->ssid, vif->ssid_len,
			       CFG80211_BSS_FTYPE_UNKNOWN, 
			       IEEE80211_PRIVACY_ANY);
#else
	bss = cfg80211_get_bss(cfg->wiphy, chan, bssid,
			       vif->ssid, vif->ssid_len,
			       WLAN_CAPABILITY_ESS, WLAN_CAPABILITY_ESS);
#endif
	if (!bss) {
		/*
		 * Since cfg80211 may not yet know about the BSS,
		 * generate a partial entry until the first BSS info
		 * event becomes available.
		 *
		 */
		ie = kmalloc(2 + vif->ssid_len, GFP_ATOMIC);
		if (ie == NULL) {
			dev_err(cfg->dev, "%s(): kmalloc() failed\n", 
				__func__);
			goto cleanup;
		}

		ie[0] = WLAN_EID_SSID;
		ie[1] = vif->ssid_len;
		memcpy(ie + 2, vif->ssid, vif->ssid_len);

		dev_dbg(cfg->dev, "%s(): inform bss ssid(%u/'%s')\n",
			__func__, vif->ssid_len, vif->ssid);
		print_hex_dump(KERN_DEBUG, DRVNAME" inform bss BSSID ", 
			DUMP_PREFIX_OFFSET, 16, 1, bssid, ETH_ALEN, 0);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,44)
		bss = cfg80211_inform_bss(cfg->wiphy, chan,
					  CFG80211_BSS_FTYPE_UNKNOWN,
					  bssid, 0, WLAN_CAPABILITY_ESS, 100,
					  ie, 2 + vif->ssid_len,
					  0, GFP_ATOMIC);
#else
		bss = cfg80211_inform_bss(cfg->wiphy, chan,
					  bssid, 0, WLAN_CAPABILITY_ESS, 100,
					  ie, 2 + vif->ssid_len,
					  0, GFP_ATOMIC);
#endif
		if (!bss) {
			dev_err(cfg->dev, 
				"%s(): cfg80211_inform_bss() failed\n", 
				__func__);
			goto cleanup;
		}
			
		dev_dbg(cfg->dev, "%s(): added bss(%p) to cfg80211\n", 
			__func__, bssid);
	} else
		dev_dbg(cfg->dev, "%s(): cfg80211 has bss\n", __func__);

cleanup:
	if (ie) kfree(ie);
	return bss;
}

static struct wireless_dev* mt7697_cfg80211_add_iface(struct wiphy *wiphy,
						      const char *name,
						      enum nl80211_iftype type,
						      u32 *flags,
						      struct vif_params *params)
{
	struct mt7697_cfg80211_info *cfg = wiphy_priv(wiphy);
	struct mt7697_vif *vif;
	struct wireless_dev *wdev = NULL;
	u8 if_idx;

	dev_dbg(cfg->dev, "%s(): iface('%s') type(%u)\n", 
		__func__, name, type);
	
	spin_lock_bh(&cfg->vif_list_lock);
	if (!list_empty(&cfg->vif_list)) {
		list_for_each_entry(vif, &cfg->vif_list, next) {
			wdev = &vif->wdev;
			break;
		}
		
		spin_unlock_bh(&cfg->vif_list_lock);
		dev_dbg(cfg->dev, "%s(): iface('%s') exists\n", 
			__func__, name);
	}
	else {
		wdev = mt7697_interface_add(cfg, name, type, if_idx);
		if (!wdev) {
			dev_err(cfg->dev, "%s(): mt7697_interface_add() failed\n", 
				__func__);
			goto cleanup;
		}

		cfg->num_vif++;
	}

cleanup:
	return wdev;
}

static int mt7697_cfg80211_del_iface(struct wiphy *wiphy,
				     struct wireless_dev *wdev)
{
	struct mt7697_cfg80211_info *cfg = wiphy_priv(wiphy);
	struct mt7697_vif *vif = netdev_priv(wdev->netdev);
	int ret;

	dev_dbg(cfg->dev, "%s(): DEL IFACE\n", __func__);

	spin_lock_bh(&cfg->vif_list_lock);
	list_del(&vif->next);
	spin_unlock_bh(&cfg->vif_list_lock);

	ret = mt7697_cfg80211_stop(vif);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_cfg80211_stop() failed(%d)\n", 
			__func__, ret);
                goto cleanup;
	}

	unregister_netdev(vif->ndev);
	
cleanup:
	return ret;
}

static int mt7697_cfg80211_change_iface(struct wiphy *wiphy,
					struct net_device *ndev,
					enum nl80211_iftype type, u32 *flags,
					struct vif_params *params)
{
	struct mt7697_cfg80211_info *cfg = wiphy_priv(wiphy);
	struct mt7697_vif *vif = netdev_priv(ndev);
	int ret = 0;

	dev_dbg(cfg->dev, "%s(): iface type(%u)\n", __func__, type);

	print_hex_dump(KERN_DEBUG, DRVNAME" MAC ", 
		DUMP_PREFIX_OFFSET, 16, 1, params->macaddr, ETH_ALEN, 0);

	switch (type) {
	case NL80211_IFTYPE_STATION:
		if (cfg->wifi_cfg.opmode != MT7697_WIFI_MODE_STA_ONLY) {
			cfg->wifi_cfg.opmode = MT7697_WIFI_MODE_STA_ONLY;
			cfg->port_type = MT7697_PORT_STA;
			vif->wdev.iftype = NL80211_IFTYPE_STATION;

			ret = mt7697_wr_set_op_mode_req(cfg);
			if (ret < 0) {
				dev_err(cfg->dev, 
					"%s(): mt7697_wr_set_op_mode_req() failed(%d)\n", 
					__func__, ret);
       				goto cleanup;
    			}
		}

		break;

	case NL80211_IFTYPE_AP:
		if (cfg->wifi_cfg.opmode != MT7697_WIFI_MODE_AP_ONLY) {
			cfg->wifi_cfg.opmode = MT7697_WIFI_MODE_AP_ONLY;
			cfg->port_type = MT7697_PORT_AP;
			vif->wdev.iftype = NL80211_IFTYPE_AP;

			ret = mt7697_wr_set_op_mode_req(cfg);
			if (ret < 0) {
				dev_err(cfg->dev, 
					"%s(): mt7697_wr_set_op_mode_req() failed(%d)\n", 
					__func__, ret);
       				goto cleanup;
    			}
		}

		break;
	
	case NL80211_IFTYPE_P2P_GO:
	case NL80211_IFTYPE_P2P_CLIENT:
	default:
		dev_err(cfg->dev, "%s(): unsupported interface(%u)\n", 
			__func__, type);
		ret = -EOPNOTSUPP;
		goto cleanup;
	}

	vif->wdev.iftype = type;

cleanup:
	return ret;
}

static int mt7697_cfg80211_set_txq_params(struct wiphy *wiphy, 
					  struct net_device *ndev,
					  struct ieee80211_txq_params *params)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, 
		"%s(): SET TXQ PARAMS ac(%u) txop(%u) cwmin/cwmax(%u/%u) aifs(%u)\n", 
		__func__, params->ac, params->txop, params->cwmin, 
		params->cwmax, params->aifs);
	return 0;
}

static int mt7697_cfg80211_scan(struct wiphy *wiphy, 
	                        struct cfg80211_scan_request *request)
{
	struct mt7697_vif *vif = mt7697_vif_from_wdev(request->wdev);
	struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	int ret;

	dev_dbg(cfg->dev, "%s(): START SCAN\n", __func__);
	
	if (test_bit(CONNECTED, &vif->flags)) {
		dev_warn(cfg->dev, 
			"%s(): CONNECTED - scan not allowed\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CONNECT_PEND, &vif->flags)) {
		dev_dbg(cfg->dev, "%s(): pending connection t/o\n", __func__);
		print_hex_dump(KERN_DEBUG, DRVNAME" BSSID ", 
			DUMP_PREFIX_OFFSET, 16, 1, vif->req_bssid, 
			ETH_ALEN, 0);
		cfg80211_connect_result(vif->ndev, vif->req_bssid,
					NULL, 0,
					NULL, 0, 
					WLAN_STATUS_UNSPECIFIED_FAILURE, 
					GFP_KERNEL);
	}

        vif->scan_req = request;
        ret = mt7697_wr_scan_req(cfg, vif->fw_vif_idx, request);
	if (ret < 0) {
		dev_err(cfg->dev, "%s(): mt7697_wr_scan_req() failed(%d)\n", 
			__func__, ret);
                goto out;
	}

out:
	if (ret < 0) vif->scan_req = NULL;
	return ret;
}

static int mt7697_cfg80211_connect(struct wiphy *wiphy, 
                                   struct net_device *ndev,
                                   struct cfg80211_connect_params *sme)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s(): CONNECT\n", __func__);
	return 0;
}

static int mt7697_cfg80211_disconnect(struct wiphy *wiphy,
				      struct net_device *ndev, u16 reason_code)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	int ret;

	dev_dbg(cfg->dev, "%s(): DISCONNECT\n", __func__);

	if (down_interruptible(&cfg->sem)) {
		dev_err(cfg->dev, "%s(): down_interruptible() failed\n", 
			__func__);
		ret = -ERESTARTSYS;
		goto cleanup;
	}

	if (test_bit(DESTROY_IN_PROGRESS, &cfg->flag)) {
		dev_err(cfg->dev, "%s(): busy, destroy in progress\n", 
			__func__);
		ret = -EBUSY;
		goto cleanup;
	}

	vif->reconnect_flag = 0;

	ret = mt7697_disconnect(vif);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_disconnect() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	memset(vif->ssid, 0, sizeof(vif->ssid));
	vif->ssid_len = 0;

cleanup:
	up(&cfg->sem);
	return ret;
}

static int mt7697_cfg80211_add_key(struct wiphy *wiphy, 
				   struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr,
				   struct key_params *params)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s(): ADD KEY index(%u) cipher(0x%08x)\n", 
		__func__, key_index, params->cipher);

	if (mac_addr) {
		print_hex_dump(KERN_DEBUG, DRVNAME" MAC ", 
			DUMP_PREFIX_OFFSET, 16, 1, mac_addr, ETH_ALEN, 0);
	}

	dev_dbg(cfg->dev, "%s(): key(%u)\n", __func__, params->key_len);
	print_hex_dump(KERN_DEBUG, DRVNAME" KEY ", 
		DUMP_PREFIX_OFFSET, 16, 1, params->key, params->key_len, 0);

	dev_dbg(cfg->dev, "%s(): seq(%u)\n", __func__, params->key_len);
	print_hex_dump(KERN_DEBUG, DRVNAME" SEQ ", 
		DUMP_PREFIX_OFFSET, 16, 1, params->key, params->key_len, 0);

	return 0;
}

static int mt7697_cfg80211_get_key(struct wiphy *wiphy, 
				   struct net_device *ndev,
                                   u8 key_index, bool pairwise,
                                   const u8 *mac_addr, void *cookie,
                                   void (*callback) (void *cookie,
                                                     struct key_params *))
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s(): GET KEY(%u)\n", __func__, key_index);

	if (mac_addr) {
		print_hex_dump(KERN_DEBUG, DRVNAME" MAC ", 
			DUMP_PREFIX_OFFSET, 16, 1, mac_addr, ETH_ALEN, 0);
	}

	return 0;
}

static int mt7697_cfg80211_del_key(struct wiphy *wiphy, 
				   struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr)
{
	struct mt7697_vif *vif = netdev_priv(ndev);
	dev_dbg(vif->cfg->dev, "%s(): DEL KEY(%u)\n", __func__, key_index);
	return 0;
}

static int mt7697_cfg80211_set_default_key(struct wiphy *wiphy,
					   struct net_device *ndev,
					   u8 key_index, bool unicast,
					   bool multicast)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, 
		"%s(): SET DEFAULT KEY(%u) unicast(%u) multicast(%u)\n", 
		__func__, key_index, unicast, multicast);
	return 0;
}

static int mt7697_cfg80211_join_ibss(struct wiphy *wiphy,
				     struct net_device *ndev,
				     struct cfg80211_ibss_params *ibss_param)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	int ret = 0;

	dev_dbg(cfg->dev, "%s(): JOIN IBSS('%s')\n", 
		__func__, ibss_param->ssid);

	vif->ssid_len = ibss_param->ssid_len;
	memcpy(vif->ssid, ibss_param->ssid, vif->ssid_len);

	return ret;
}

static int mt7697_cfg80211_leave_ibss(struct wiphy *wiphy,
				      struct net_device *ndev)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	int ret = 0;

	dev_dbg(cfg->dev, "%s(): LEAVE IBSS\n", __func__);

	mt7697_disconnect(vif);
	memset(vif->ssid, 0, sizeof(vif->ssid));
	vif->ssid_len = 0;

	return ret;
}

static int mt7697_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	struct mt7697_cfg80211_info *cfg = (struct mt7697_cfg80211_info*)wiphy_priv(wiphy);
	dev_dbg(cfg->dev, "%s(): SET WIPHY PARAMS changed(0x%08x)\n", 
		__func__, changed);
	return 0;
}

static int mt7697_cfg80211_start_ap(struct wiphy *wiphy, 
 				    struct net_device *ndev,
				    struct cfg80211_ap_settings *settings)
{
	struct mt7697_cfg80211_info *cfg = (struct mt7697_cfg80211_info*)wiphy_priv(wiphy);
	struct mt7697_vif *vif = netdev_priv(ndev);
	int ret;

	dev_dbg(cfg->dev, "%s(): START AP ssid('%s') band(%u) freq(%u)\n", 
		__func__, settings->ssid, settings->chandef.chan->band, 
		settings->chandef.chan->center_freq);

	ret = mt7697_wr_set_ssid_req(cfg, settings->ssid_len, settings->ssid);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_set_ssid_req() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}
	
	cfg->wifi_cfg.ap.ssid_len = settings->ssid_len;
	memcpy(cfg->wifi_cfg.ap.ssid, settings->ssid, settings->ssid_len);

	vif->ch_hint = ieee80211_frequency_to_channel(settings->chandef.chan->center_freq);
	if (!vif->ch_hint) {
		dev_err(cfg->dev, 
			"%s(): ieee80211_frequency_to_channel() failed\n", 
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s(): channel(%u)\n", __func__, vif->ch_hint);
	ret = mt7697_wr_set_channel_req(cfg, vif->ch_hint);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_set_channel_req() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}
	cfg->wifi_cfg.ap.ch = vif->ch_hint;

	ret = mt7697_set_wpa_version(vif, settings->crypto.wpa_versions);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_set_wpa_version() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	if (settings->crypto.n_ciphers_pairwise)
		ret = mt7697_set_cipher(vif, settings->crypto.ciphers_pairwise[0], true);
	else
		ret = mt7697_set_cipher(vif, 0, true);
	if (ret < 0) {
		dev_err(cfg->dev, "mt7697_set_cipher() failed(%d)\n", ret);
                goto cleanup;
	}

	ret = mt7697_set_cipher(vif, settings->crypto.cipher_group, false);
	if (ret < 0) {
		dev_err(cfg->dev, "%s: mt7697_set_cipher() failed(%d)\n", 
			__func__, ret);
                goto cleanup;
	}

	if (settings->crypto.n_akm_suites) {
		ret = mt7697_set_key_mgmt(vif, settings->crypto.akm_suites[0]);
		if (ret < 0) {
			dev_err(cfg->dev, "%s: mt7697_set_key_mgmt() failed(%d)\n", 
				__func__, ret);
                	goto cleanup;
		}
	}

	ret = mt7697_wr_set_security_mode_req(cfg, vif->auth_mode, vif->prwise_crypto);
	if (ret < 0) {
		dev_err(cfg->dev, "%s: mt7697_wr_set_security_mode_req() failed(%d)\n", 
			__func__, ret);
                goto cleanup;
	}

	ret = mt7697_wr_reload_settings_req(cfg, vif->fw_vif_idx);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_reload_settings_req() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

cleanup:
	return ret;
}

static int mt7697_cfg80211_stop_ap(struct wiphy *wiphy, 
				   struct net_device *ndev)
{
	struct mt7697_cfg80211_info *cfg = (struct mt7697_cfg80211_info*)wiphy_priv(wiphy);
	dev_dbg(cfg->dev, "%s(): STOP AP\n", __func__);
	return 0;
}

static int mt7697_cfg80211_change_beacon(struct wiphy *wiphy, 
                                         struct net_device *ndev, 
                                         struct cfg80211_beacon_data *info)
{
	struct mt7697_cfg80211_info *cfg = (struct mt7697_cfg80211_info*)wiphy_priv(wiphy);
	dev_dbg(cfg->dev, "%s(): CHANGE BEACON\n", __func__);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,44)
static int mt7697_cfg80211_add_station(struct wiphy *wiphy, 
				       struct net_device *ndev,
				       const u8 *mac,
				       struct station_parameters *params)
#else
static int mt7697_cfg80211_add_station(struct wiphy *wiphy, 
				       struct net_device *ndev,
				       u8 *mac,
				       struct station_parameters *params)
#endif
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s(): ADD STATION\n", __func__);

	if (mac) {
		print_hex_dump(KERN_DEBUG, DRVNAME" MAC ", 
			DUMP_PREFIX_OFFSET, 16, 1, mac, ETH_ALEN, 0);
	}

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,44)
static int mt7697_cfg80211_get_station(struct wiphy *wiphy, 
				       struct net_device *ndev,
			      	       const u8 *mac, 
				       struct station_info *sinfo)
#else
static int mt7697_cfg80211_get_station(struct wiphy *wiphy, 
				       struct net_device *ndev,
			      	       u8 *mac, 
				       struct station_info *sinfo)
#endif
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s(): CHANGE STATION\n", __func__);

	if (mac) {
		print_hex_dump(KERN_DEBUG, DRVNAME" MAC ", 
			DUMP_PREFIX_OFFSET, 16, 1, mac, ETH_ALEN, 0);
	}

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,44)
static int mt7697_cfg80211_del_station(struct wiphy *wiphy, 
				       struct net_device *ndev,
                                       struct station_del_parameters *params)
#else
static int mt7697_cfg80211_del_station(struct wiphy *wiphy, 
				       struct net_device *ndev,
                                       u8 *mac)
#endif
{
        struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s(): DEL STATION\n", __func__);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,44)
	if (mac) {
		print_hex_dump(KERN_DEBUG, DRVNAME" MAC ", 
			DUMP_PREFIX_OFFSET, 16, 1, mac, ETH_ALEN, 0);
	}
#endif
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,44)
static int mt7697_cfg80211_change_station(struct wiphy *wiphy,
					  struct net_device *ndev,
				 	  const u8 *mac, 
					  struct station_parameters *params)
#else
static int mt7697_cfg80211_change_station(struct wiphy *wiphy,
					  struct net_device *ndev,
				 	  u8 *mac, 
					  struct station_parameters *params)
#endif
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	int err = 0;

	dev_dbg(cfg->dev, "%s(): CHANGE STATION flags(0x%08x)\n",
		__func__, params->sta_flags_set);

	if (mac) {
		print_hex_dump(KERN_DEBUG, DRVNAME" MAC ", 
			DUMP_PREFIX_OFFSET, 16, 1, mac, ETH_ALEN, 0);
	}

	if ((vif->wdev.iftype != NL80211_IFTYPE_STATION) &&
	    (vif->wdev.iftype != NL80211_IFTYPE_AP)) {
		dev_err(cfg->dev, "%s(): iftype(%u) not supported\n", 
			__func__, vif->wdev.iftype);
		err = -EOPNOTSUPP;
		goto cleanup;
	}

	err = cfg80211_check_station_change(wiphy, params,
					    CFG80211_STA_AP_MLME_CLIENT);
	if (err) {
		dev_err(cfg->dev, 
			"%s(): cfg80211_check_station_change() failed(%d)\n", 
			__func__, err);
		goto cleanup;
	}

cleanup:
	return err;
}

static int mt7697_cfg80211_set_pmksa(struct wiphy *wiphy, 
				     struct net_device *ndev,
				     struct cfg80211_pmksa *pmksa)
{
	struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	dev_dbg(cfg->dev, "%s(): SET PMKSA\n", __func__);
	return 0;
}

static int mt7697_cfg80211_del_pmksa(struct wiphy *wiphy, 
				     struct net_device *ndev,
				     struct cfg80211_pmksa *pmksa)
{
	struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	dev_dbg(cfg->dev, "%s(): DEL PMKSA\n", __func__);
	return 0;
}

static int mt7697_cfg80211_flush_pmksa(struct wiphy *wiphy, 
				       struct net_device *ndev)
{
	struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	dev_dbg(cfg->dev, "%s(): FLUSH PMKSA\n", __func__);
	return 0;
}

static int mt7697_cfg80211_remain_on_channel(struct wiphy *wiphy,
                                    	     struct wireless_dev *wdev,
                                    	     struct ieee80211_channel *chan,
                                    	     unsigned int duration,
                                    	     u64 *cookie)
{
        struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	dev_dbg(cfg->dev, "%s(): REMAIN ON CH\n", __func__);
	return 0;
}

static int mt7697_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
                                           	    struct wireless_dev *wdev,
                                           	    u64 cookie)
{
        struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	dev_dbg(cfg->dev, "%s(): CANCEL REMAIN ON CH\n", __func__);
	return 0;
}

static int mt7697_cfg80211_mgmt_tx(struct wiphy *wiphy, 
				   struct wireless_dev *wdev,
                          	   struct cfg80211_mgmt_tx_params *params, 
				   u64 *cookie)
{
        struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	dev_dbg(cfg->dev, "%s(): MGMT TX len(%u)\n", __func__, params->len);

	print_hex_dump(KERN_DEBUG, DRVNAME" MGMT Tx ", DUMP_PREFIX_OFFSET, 
		16, 1, params->buf, params->len, 0);

	return 0;
}

static void mt7697_cfg80211_mgmt_frame_register(struct wiphy *wiphy,
                                       		struct wireless_dev *wdev,
                                       		u16 frame_type, bool reg)
{
	struct mt7697_vif *vif = mt7697_vif_from_wdev(wdev);
        struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	dev_dbg(cfg->dev, "%s(): MGMT FRAME REG type(0x%04x) reg(%u)\n", 
		__func__, frame_type, reg);

	if (frame_type == IEEE80211_STYPE_PROBE_REQ) {
		/*
		 * Note: This notification callback is not allowed to sleep, so
		 * we cannot send WMI_PROBE_REQ_REPORT_CMD here. Instead, we
		 * hardcode target to report Probe Request frames all the time.
		 */
		vif->probe_req_report = reg;
	}
}

static const struct cfg80211_ops mt7697_cfg80211_ops = 
{        
	.add_virtual_intf 	  = mt7697_cfg80211_add_iface,
	.del_virtual_intf 	  = mt7697_cfg80211_del_iface,
	.change_virtual_intf 	  = mt7697_cfg80211_change_iface,
	.set_txq_params		  = mt7697_cfg80211_set_txq_params,
	.scan 			  = mt7697_cfg80211_scan,
	.connect 		  = mt7697_cfg80211_connect,
	.disconnect 		  = mt7697_cfg80211_disconnect,
	.add_key 		  = mt7697_cfg80211_add_key,
	.get_key 		  = mt7697_cfg80211_get_key,
	.del_key 		  = mt7697_cfg80211_del_key,
	.set_default_key 	  = mt7697_cfg80211_set_default_key,
	.join_ibss 		  = mt7697_cfg80211_join_ibss,
	.leave_ibss 		  = mt7697_cfg80211_leave_ibss,
	.set_wiphy_params	  = mt7697_cfg80211_set_wiphy_params,
	.start_ap		  = mt7697_cfg80211_start_ap,
	.stop_ap		  = mt7697_cfg80211_stop_ap,
	.change_beacon		  = mt7697_cfg80211_change_beacon,
	.add_station 		  = mt7697_cfg80211_add_station,
	.get_station 		  = mt7697_cfg80211_get_station,
	.del_station 		  = mt7697_cfg80211_del_station,
	.change_station 	  = mt7697_cfg80211_change_station,
	.set_pmksa		  = mt7697_cfg80211_set_pmksa,
  	.del_pmksa		  = mt7697_cfg80211_del_pmksa,
  	.flush_pmksa		  = mt7697_cfg80211_flush_pmksa,
	.remain_on_channel 	  = mt7697_cfg80211_remain_on_channel,
        .cancel_remain_on_channel = mt7697_cfg80211_cancel_remain_on_channel,
        .mgmt_tx 		  = mt7697_cfg80211_mgmt_tx,
        .mgmt_frame_register 	  = mt7697_cfg80211_mgmt_frame_register,
};

int mt7697_cfg80211_stop(struct mt7697_vif *vif)
{
	struct mt7697_sta *sta, *sta_next;
	int ret = 0;

        mt7697_tx_stop(vif->cfg);

	if (vif->wdev.iftype == NL80211_IFTYPE_STATION) {
                ret = mt7697_wr_disconnect_req(vif->cfg, NULL);
		if (ret < 0) {
			dev_err(vif->cfg->dev, 
				"%s(): mt7697_wr_disconnect_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		switch (vif->sme_state) {
		case SME_DISCONNECTED:
			break;

		case SME_CONNECTING:               
			cfg80211_connect_result(vif->ndev, vif->bssid, NULL, 0,
						NULL, 0,
						WLAN_STATUS_UNSPECIFIED_FAILURE,
						GFP_KERNEL);
			break;

		case SME_CONNECTED:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,44)
			cfg80211_disconnected(vif->ndev, 0, NULL, 0, 
				              vif->locally_generated, GFP_KERNEL);
#else
			cfg80211_disconnected(vif->ndev, 0, NULL, 0, GFP_KERNEL);
#endif
			break;
		}
	}
	else if (vif->wdev.iftype == NL80211_IFTYPE_AP) {
		spin_lock_bh(&vif->sta_list_lock);
		list_for_each_entry_safe(sta, sta_next, &vif->sta_list, next) {
			print_hex_dump(KERN_DEBUG, DRVNAME" DISCONNECT STA BSSID ", 
				DUMP_PREFIX_OFFSET, 16, 1, sta->bssid, ETH_ALEN, 0);

			spin_unlock_bh(&vif->sta_list_lock);
			ret = mt7697_wr_disconnect_req(vif->cfg, sta->bssid);
			if (ret < 0) {
				dev_err(vif->cfg->dev, 
					"%s(): mt7697_wr_disconnect_req() failed(%d)\n", 
					__func__, ret);
				goto cleanup;
			}

			spin_lock_bh(&vif->sta_list_lock);
			list_del(&sta->next);
			kfree(sta);
		}

		vif->sta_count = 0;
		spin_unlock_bh(&vif->sta_list_lock);
	}

	vif->sme_state = SME_DISCONNECTED;
	clear_bit(CONNECTED, &vif->flags);
	clear_bit(CONNECT_PEND, &vif->flags);

	vif->cfg->wifi_cfg.opmode = MT7697_WIFI_MODE_STA_ONLY;
	vif->cfg->port_type = MT7697_PORT_STA;
	vif->wdev.iftype = NL80211_IFTYPE_STATION;
	ret = mt7697_wr_set_op_mode_req(vif->cfg);
	if (ret < 0) {
		dev_err(vif->cfg->dev, 
			"%s(): mt7697_wr_set_op_mode_req() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	memset(vif->ssid, 0, IEEE80211_MAX_SSID_LEN);
	memset(vif->req_bssid, 0, ETH_ALEN);
	memset(vif->pmk, 0, MT7697_WIFI_LENGTH_PMK);

	/* Stop netdev queues, needed during recovery */
	netif_stop_queue(vif->ndev);
	netif_carrier_off(vif->ndev);

	if (vif->scan_req) {
		cfg80211_scan_done(vif->scan_req, true);
		vif->scan_req = NULL;
	}

	ret = vif->cfg->hif_ops->close(vif->cfg->txq_hdl);
	if (ret < 0) {
		dev_err(vif->cfg->dev, "%s(): close() failed(%d)\n", 
			__func__, ret);
	}

        vif->cfg->txq_hdl = NULL;
	vif->cfg->rxq_hdl = NULL;

cleanup:
	return ret;
}

static const struct ieee80211_txrx_stypes
mt7697_txrx_stypes[NUM_NL80211_IFTYPES] = {
        [NL80211_IFTYPE_STATION] = {
		.tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		      BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		      BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
	[NL80211_IFTYPE_AP] = {
		.tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		      BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		      BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
};

static int mt7697_cfg80211_vif_init(struct mt7697_vif *vif)
{
	setup_timer(&vif->disconnect_timer, mt7697_disconnect_timer_hndlr,
		    (unsigned long) vif->ndev);

	set_bit(WMM_ENABLED, &vif->flags);
	spin_lock_init(&vif->if_lock);

	return 0;
}

static void mt7697_cfg80211_disconnect_work(struct work_struct *work)
{
        struct mt7697_vif *vif = container_of(work, struct mt7697_vif, 
                                              disconnect_work);
	struct mt7697_cfg80211_info *cfg = vif->cfg;
	int ret;

	ret = mt7697_disconnect(vif);
	if (ret < 0) {
		dev_err(cfg->dev, "%s(): mt7697_disconnect() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	memset(vif->ssid, 0, sizeof(vif->ssid));
	vif->ssid_len = 0;

cleanup:
	return;
}

static void mt7697_cleanup_vif(struct mt7697_cfg80211_info *cfg)
{
	struct mt7697_vif *vif, *vif_next = NULL;
	int ret;

	spin_lock_bh(&cfg->vif_list_lock);
	list_for_each_entry_safe(vif, vif_next, &cfg->vif_list, next) {
		dev_dbg(cfg->dev, "%s(): remove vif(%u)\n", 
			__func__, vif->fw_vif_idx);

		if (vif->cfg->hif_ops->shutdown) {
			ret = vif->cfg->hif_ops->shutdown(&vif->cfg->txq_hdl, &vif->cfg->rxq_hdl);
			if (ret < 0) {
				dev_err(vif->cfg->dev, 
					"%s(): shutdown() failed(%d)\n", 
					__func__, ret);
			}
		}

		list_del(&vif->next);
		WARN_ON(vif->sta_count > 0);
		spin_unlock_bh(&cfg->vif_list_lock);

		unregister_netdev(vif->ndev);	

		spin_lock_bh(&cfg->vif_list_lock);
	}

	spin_unlock_bh(&cfg->vif_list_lock);
}

struct mt7697_vif* mt7697_get_vif_by_idx(struct mt7697_cfg80211_info *cfg, 
					 u32 if_idx)
{
	struct mt7697_vif *vif, *found = NULL;

	if (if_idx > cfg->vif_max - 1) {
		dev_err(cfg->dev, "%s(): invalid if idx(%u > %u)\n", 
			__func__, if_idx, cfg->vif_max - 1);
		return NULL;
	}

	spin_lock_bh(&cfg->vif_list_lock);
	list_for_each_entry(vif, &cfg->vif_list, next) {
		if (vif->fw_vif_idx == if_idx) {
			found = vif;
			break;
		}
	}

	spin_unlock_bh(&cfg->vif_list_lock);
	return found;
}

struct wireless_dev* mt7697_interface_add(struct mt7697_cfg80211_info *cfg, 
                                          const char *name,
	                                  enum nl80211_iftype type, 
                                          u8 fw_vif_idx)
{
	struct net_device *ndev;
	struct mt7697_vif *vif;
	int err;

	dev_dbg(cfg->dev, "%s(): interface('%s') type(%u)\n", 
		__func__, name, type);

	if (list_empty(&cfg->vif_list)) {
		ndev = alloc_etherdev(sizeof(struct mt7697_vif));
        	if (!ndev) {
                	dev_err(cfg->dev, "%s(): alloc_etherdev() failed\n", 
				__func__);
			goto err;
 		}

		err = dev_alloc_name(ndev, name);
        	if (err < 0) {
                	dev_err(cfg->dev, "%s(): dev_alloc_name() failed(%d)\n", 
				__func__, err);
			goto err;
 		}

		vif = netdev_priv(ndev);
		ndev->ieee80211_ptr = &vif->wdev;
		vif->wdev.wiphy = cfg->wiphy;
		vif->cfg = cfg;
		vif->ndev = ndev;
		INIT_WORK(&vif->disconnect_work, mt7697_cfg80211_disconnect_work);
		INIT_LIST_HEAD(&vif->next);
		SET_NETDEV_DEV(ndev, wiphy_dev(vif->wdev.wiphy));
		vif->wdev.netdev = ndev;
		vif->wdev.iftype = type;
		vif->fw_vif_idx = fw_vif_idx;

		spin_lock_init(&vif->sta_list_lock);
		INIT_LIST_HEAD(&vif->sta_list);
		vif->sta_max = MT7697_MAX_STA;

		ndev->addr_assign_type = NET_ADDR_PERM;
		ndev->addr_len = ETH_ALEN;
		ndev->dev_addr = cfg->mac_addr.addr;
		ndev->wireless_data = &vif->wireless_data;

		mt7697_init_netdev(ndev);

		if (mt7697_cfg80211_vif_init(vif)) {
			dev_err(cfg->dev, "%s(): mt7697_cfg80211_vif_init() failed\n", 
				__func__);
			goto err;
		}

		dev_dbg(cfg->dev, "%s(): register('%s') type(%u)\n", 
			__func__, ndev->name, type);

		if (register_netdevice(ndev)) {
			dev_err(cfg->dev, "%s(): register_netdevice() failed\n", 
				__func__);
			goto err;
		}

		netif_carrier_off(ndev);

		vif->sme_state = SME_DISCONNECTED;
		set_bit(WLAN_ENABLED, &vif->flags);

		spin_lock_bh(&cfg->vif_list_lock);
		list_add_tail(&vif->next, &cfg->vif_list);
		spin_unlock_bh(&cfg->vif_list_lock);

		dev_err(cfg->dev, "%s(): added('%s') type(%u)\n", 
			__func__, ndev->name, type);
	}
	else {
		dev_dbg(cfg->dev, "%s(): interface exists\n", __func__);
	    	vif = mt7697_get_vif_by_idx(cfg, 0);
	}

	return &vif->wdev;

err:
	if (ndev) free_netdev(ndev);
	return NULL;
}

int mt7697_cfg80211_del_sta(struct mt7697_vif *vif, const u8* bssid)
{
	struct mt7697_sta *sta, *sta_next;
	int ret = -EINVAL;

	spin_lock_bh(&vif->sta_list_lock);
	list_for_each_entry_safe(sta, sta_next, &vif->sta_list, next) {
		if (!memcmp(bssid, sta->bssid, ETH_ALEN)) {
			print_hex_dump(KERN_DEBUG, DRVNAME" DISCONNECT STA BSSID ", 
				DUMP_PREFIX_OFFSET, 16, 1, sta->bssid, ETH_ALEN, 0);

			spin_unlock_bh(&vif->sta_list_lock);
			ret = mt7697_wr_disconnect_req(vif->cfg, sta->bssid);
			if (ret < 0) {
				dev_err(vif->cfg->dev, 
					"%s(): mt7697_wr_disconnect_req() failed(%d)\n", 
					__func__, ret);
				goto cleanup;
			}

			spin_lock_bh(&vif->sta_list_lock);
			list_del(&sta->next);
			kfree(sta);
			vif->sta_count--;
			ret = 0;
			break;
		}
	}

cleanup:
	spin_unlock_bh(&vif->sta_list_lock);
	return ret;
}

int mt7697_cfg80211_new_sta(struct mt7697_vif *vif, const u8* bssid)
{
	struct mt7697_sta *sta;
	int ret = 0;

	dev_dbg(vif->cfg->dev, "%s(): # stations(%u)\n",
		__func__, vif->sta_count);
	if (vif->sta_count > vif->sta_max) {
		dev_err(vif->cfg->dev, "%s(): max stations reached(%u)\n",
			__func__, vif->sta_max);
		ret = -EINVAL;
		goto cleanup;
	}

	sta = kmalloc(sizeof(struct mt7697_sta), GFP_KERNEL);
	if (!sta) {
		dev_err(vif->cfg->dev, "%s(): kmalloc() failed\n", __func__);
		ret = -ENOMEM;
		goto cleanup;
	}

	print_hex_dump(KERN_DEBUG, DRVNAME" ADD STA BSSID ", 
		DUMP_PREFIX_OFFSET, 16, 1, bssid, ETH_ALEN, 0);

	memcpy(sta->bssid, bssid, ETH_ALEN);
	INIT_LIST_HEAD(&sta->next);
	spin_lock_bh(&vif->sta_list_lock);
	list_add_tail(&sta->next, &vif->sta_list);
	vif->sta_count++;
	spin_unlock_bh(&vif->sta_list_lock);

cleanup:
	return ret;
}

int mt7697_cfg80211_connect_event(struct mt7697_vif *vif, const u8* bssid, 
				  u32 channel)
{
	struct mt7697_cfg80211_info *cfg = vif->cfg;
	struct wiphy *wiphy = cfg_to_wiphy(cfg);
	struct ieee80211_supported_band *band;
	struct cfg80211_bss *bss;
	u32 freq;
	int ret = 0;

	spin_lock_bh(&vif->if_lock);

	set_bit(CONNECTED, &vif->flags);
	clear_bit(CONNECT_PEND, &vif->flags);

	dev_dbg(cfg->dev, "%s(): vif flags(0x%08lx)\n", __func__, vif->flags);
        if (bssid) {
                print_hex_dump(KERN_DEBUG, DRVNAME" BSSID ", 
			        DUMP_PREFIX_OFFSET, 16, 1, bssid, ETH_ALEN, 0);
        }

	if ((channel > 0) && (channel <= MT7697_CH_MAX_2G_CHANNEL))
		band = wiphy->bands[IEEE80211_BAND_2GHZ];
	else if ((channel >= MT7697_CH_MIN_5G_CHANNEL) && 
		 (channel <= MT7697_CH_MAX_5G_CHANNEL))
		band = wiphy->bands[IEEE80211_BAND_5GHZ];
	else {
		dev_err(cfg->dev, "%s(): invalid channel(%u)\n",
			__func__, channel);
		ret = -EINVAL;
		goto cleanup;
	}

	if (!band) {
		dev_err(cfg->dev, "%s(): channel(%u) NULL band\n",
			__func__, channel);
		ret = -EINVAL;
		goto cleanup;
	}

	freq = ieee80211_channel_to_frequency(channel, band->band);
	if (!freq) {
		dev_err(cfg->dev, 
			"%s(): ieee80211_channel_to_frequency() failed\n", 
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}

	bss = mt7697_add_bss_if_needed(vif, bssid, freq);
	if (!bss) {
		dev_err(cfg->dev, "%s(): mt7697_add_bss_if_needed() failed\n",
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}

	WARN_ON(!vif->ndev);
	dev_dbg(cfg->dev, "%s(): vif sme_state(%u)\n", 
		__func__, vif->sme_state);
	if ((vif->sme_state == SME_CONNECTING) ||
	    (vif->sme_state == SME_DISCONNECTED)) {
		vif->sme_state = SME_CONNECTED;
		memcpy(vif->bssid, bssid, ETH_ALEN);

		cfg80211_connect_result(vif->ndev, bssid,
					NULL, 0,
					NULL, 0, 
					WLAN_STATUS_SUCCESS, GFP_ATOMIC);
		cfg80211_put_bss(cfg->wiphy, bss);
	} else if (vif->sme_state == SME_CONNECTED) {
		/* inform roam event to cfg80211 */		
		cfg80211_roamed_bss(vif->ndev, bss,
				    NULL, 0,
				    NULL, 0,  
				    GFP_ATOMIC);
	}

	netif_wake_queue(vif->ndev);
	netif_carrier_on(vif->ndev);

cleanup:
	spin_unlock_bh(&vif->if_lock);
	return ret;
}

#ifdef CONFIG_PM
static const struct wiphy_wowlan_support mt7697_wowlan_support = {
	.flags = WIPHY_WOWLAN_MAGIC_PKT |
		 WIPHY_WOWLAN_DISCONNECT |
		 WIPHY_WOWLAN_GTK_REKEY_FAILURE  |
		 WIPHY_WOWLAN_SUPPORTS_GTK_REKEY |
		 WIPHY_WOWLAN_EAP_IDENTITY_REQ   |
		 WIPHY_WOWLAN_4WAY_HANDSHAKE,
	.n_patterns = MT7697_WOW_MAX_FILTERS_PER_LIST,
	.pattern_min_len = 1,
	.pattern_max_len = MT7697_WOW_PATTERN_SIZE,
};
#endif

int mt7697_cfg80211_get_params(struct mt7697_cfg80211_info *cfg)
{
	int err = mt7697_wr_get_listen_interval_req(cfg);
	if (err < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_get_listen_interval_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

	err = mt7697_wr_get_wireless_mode_req(cfg);
	if (err < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_get_wireless_mode_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

	err = mt7697_wr_mac_addr_req(cfg);
	if (err < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_mac_addr_req() failed(%d)\n", 
			__func__, err);
		goto failed;
	}

failed:
	return err;
}

int mt7697_cfg80211_init(struct mt7697_cfg80211_info *cfg)
{
	struct wiphy *wiphy = cfg->wiphy;
	bool band_2gig = false, band_5gig = false, ht = false;
	s32 err = 0;

	dev_dbg(cfg->dev, "%s(): init mt7697 cfg80211\n", __func__);
	cfg->tx_req.cmd.grp = MT7697_CMD_GRP_80211;
	cfg->tx_req.cmd.type = MT7697_CMD_TX_RAW;

	cfg->wireless_mode = MT7697_WIFI_PHY_11ABGN_MIXED;

	wiphy->mgmt_stypes = mt7697_txrx_stypes;
        wiphy->max_remain_on_channel_duration = 5000;
	set_wiphy_dev(wiphy, cfg->dev);

	wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) |
				 BIT(NL80211_IFTYPE_ADHOC) |
                                 BIT(NL80211_IFTYPE_AP);

        wiphy->max_scan_ssids = MT7697_SCAN_MAX_ITEMS;
        wiphy->max_scan_ie_len = IEEE80211_MAX_SSID_LEN;
	
        wiphy->max_scan_ie_len = 1000; /* FIX: what is correct limit? */
	switch (cfg->wireless_mode) {
	case MT7697_WIFI_PHY_11AN_MIXED:
	case MT7697_WIFI_PHY_11N_5G:
		ht = true;
	case MT7697_WIFI_PHY_11A:
		band_5gig = true;
		break;
	case MT7697_WIFI_PHY_11GN_MIXED:
	case MT7697_WIFI_PHY_11N_2_4G:
	case MT7697_WIFI_PHY_11BGN_MIXED:
		ht = true;
	case MT7697_WIFI_PHY_11BG_MIXED:
	case MT7697_WIFI_PHY_11B:
	case MT7697_WIFI_PHY_11G:
		band_2gig = true;
		break;
	case MT7697_WIFI_PHY_11ABGN_MIXED:
	case MT7697_WIFI_PHY_11AGN_MIXED:
		ht = true;
	case MT7697_WIFI_PHY_11ABG_MIXED:
		band_2gig = true;
		band_5gig = true;
		break;
	default:
		dev_err(cfg->dev, "%s(): invalid phy capability(%d)\n", 
			__func__, cfg->wireless_mode);
		err = -EINVAL;
		goto cleanup;
	}

	/*
	 * Even if the fw has HT support, advertise HT cap only when
	 * the firmware has support to override RSN capability, otherwise
	 * 4-way handshake would fail.
	 */
	if (!(ht/* &&
	      test_bit(ATH6KL_FW_CAPABILITY_RSN_CAP_OVERRIDE,
		       ar->fw_capabilities)*/)) {
		mt7697_band_2ghz.ht_cap.cap = 0;
		mt7697_band_2ghz.ht_cap.ht_supported = false;
		mt7697_band_5ghz.ht_cap.cap = 0;
		mt7697_band_5ghz.ht_cap.ht_supported = false;
	}

	wiphy->bands[IEEE80211_BAND_2GHZ] = band_2gig ? &mt7697_band_2ghz : NULL;
	wiphy->bands[IEEE80211_BAND_5GHZ] = band_5gig ? &mt7697_band_5ghz : NULL;

	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
        wiphy->cipher_suites = mt7697_cipher_suites;
        wiphy->n_cipher_suites = ARRAY_SIZE(mt7697_cipher_suites);

#ifdef CONFIG_PM
	wiphy->wowlan = &mt7697_wowlan_support;
#endif
	wiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM |
			WIPHY_FLAG_HAVE_AP_SME |
			WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
			WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD;

	wiphy->probe_resp_offload =
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS |
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS2 |
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_P2P;

        err = wiphy_register(wiphy);
        if (err < 0) {
                dev_err(cfg->dev, "%s(): wiphy_register() failed(%d)\n", 
			__func__, err);               
                goto cleanup;
        }

	cfg->wiphy_registered = true;
	dev_dbg(cfg->dev, "%s(): wiphy registered\n", __func__);

cleanup:
        return err;
}

void mt7697_cfg80211_cleanup(struct mt7697_cfg80211_info *cfg)
{
	dev_dbg(cfg->dev, "%s(): cleanup\n", __func__);

	dev_dbg(cfg->dev, "%s(): cleanup vif\n", __func__);
	mt7697_cleanup_vif(cfg);

	dev_dbg(cfg->dev, "%s(): destroy Tx workqueues\n", __func__);
	flush_workqueue(cfg->tx_workq);
	destroy_workqueue(cfg->tx_workq);

	if (cfg->wiphy_registered) {
		dev_dbg(cfg->dev, "%s(): unregister wiphy\n", __func__);
		wiphy_unregister(cfg->wiphy);
		cfg->wiphy_registered = false;
		dev_dbg(cfg->dev, "%s(): wiphy unregistered\n", __func__);
	}
}

struct mt7697_cfg80211_info *mt7697_cfg80211_create(void)
{
	struct mt7697_cfg80211_info *cfg = NULL;
	struct wiphy *wiphy;

	/* create a new wiphy for use with cfg80211 */
	wiphy = wiphy_new(&mt7697_cfg80211_ops, 
		          sizeof(struct mt7697_cfg80211_info));
	if (wiphy) {
		cfg = wiphy_priv(wiphy);
		cfg->wiphy = wiphy;
	}

	return cfg;
}

void mt7697_cfg80211_destroy(struct mt7697_cfg80211_info *cfg)
{
	dev_dbg(cfg->dev, "%s(): destroy\n", __func__);
	wiphy_free(cfg->wiphy);
}
