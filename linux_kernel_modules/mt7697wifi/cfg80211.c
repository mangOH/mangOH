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

#define RATETAB_ENT(_rate, _rateid, _flags) {   \
	.bitrate    = (_rate),                  \
	.flags      = (_flags),                 \
	.hw_value   = (_rateid),                \
}

#define CHAN2G(_channel, _freq, _flags) {   	\
	.band           = IEEE80211_BAND_2GHZ,  \
	.hw_value       = (_channel),           \
	.center_freq    = (_freq),              \
	.flags          = (_flags),             \
	.max_antenna_gain   = 0,                \
	.max_power      = 30,                   \
}

#define CHAN5G(_channel, _flags) {		    \
	.band           = IEEE80211_BAND_5GHZ,      \
	.hw_value       = (_channel),               \
	.center_freq    = 5000 + (5 * (_channel)),  \
	.flags          = (_flags),                 \
	.max_antenna_gain   = 0,                    \
	.max_power      = 30,                       \
}

static struct ieee80211_rate mt7697_rates[] = {
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

static struct ieee80211_channel mt7697_2ghz_channels[] = {
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

static struct ieee80211_channel mt7697_5ghz_a_channels[] = {
	CHAN5G(34, 0), CHAN5G(36, 0),
	CHAN5G(38, 0), CHAN5G(40, 0),
	CHAN5G(42, 0), CHAN5G(44, 0),
	CHAN5G(46, 0), CHAN5G(48, 0),
	CHAN5G(52, 0), CHAN5G(56, 0),
	CHAN5G(60, 0), CHAN5G(64, 0),
	CHAN5G(100, 0), CHAN5G(104, 0),
	CHAN5G(108, 0), CHAN5G(112, 0),
	CHAN5G(116, 0), CHAN5G(120, 0),
	CHAN5G(124, 0), CHAN5G(128, 0),
	CHAN5G(132, 0), CHAN5G(136, 0),
	CHAN5G(140, 0), CHAN5G(149, 0),
	CHAN5G(153, 0), CHAN5G(157, 0),
	CHAN5G(161, 0), CHAN5G(165, 0),
	CHAN5G(184, 0), CHAN5G(188, 0),
	CHAN5G(192, 0), CHAN5G(196, 0),
	CHAN5G(200, 0), CHAN5G(204, 0),
	CHAN5G(208, 0), CHAN5G(212, 0),
	CHAN5G(216, 0),
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

static int mt7697_cfg80211_vif_stop(struct mt7697_vif *vif, bool wmi_ready)
{
	int ret = 0;

	netif_stop_queue(vif->ndev);

	clear_bit(WLAN_ENABLED, &vif->flags);

	if (wmi_ready) {
		ret = mt7697_disconnect(vif);
		if (ret < 0) {
			dev_err(vif->cfg->dev, 
				"mt7697_disconnect() failed(%d)\n", ret);
			goto cleanup;
		}

		del_timer(&vif->disconnect_timer);
	}

	if (vif->scan_req) {
		cfg80211_scan_done(vif->scan_req, true);
		vif->scan_req = NULL;
	}

cleanup:
	return ret;
}

static inline bool mt7697_is_wpa_ie(const u8 *pos)
{
	return pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
		pos[2] == 0x00 && pos[3] == 0x50 &&
		pos[4] == 0xf2 && pos[5] == 0x01;
}

static inline bool mt7697_is_rsn_ie(const u8 *pos)
{
	return pos[0] == WLAN_EID_RSN;
}

static inline bool mt7697_is_wps_ie(const u8 *pos)
{
	return (pos[0] == WLAN_EID_VENDOR_SPECIFIC &&
		pos[1] >= 4 &&
		pos[2] == 0x00 && pos[3] == 0x50 && pos[4] == 0xf2 &&
		pos[5] == 0x04);
}

static int mt7697_set_assoc_req_ies(struct mt7697_vif *vif, const u8 *ies,
				    size_t ies_len)
{
	struct mt7697_cfg80211_info *cfg = vif->cfg;
	const u8 *pos;
	u8 *buf = NULL;
	size_t len = 0;
	int ret = 0;

	cfg->connect_ctrl_flags &= ~MT7697_CONNECT_WPS_FLAG;

	if (ies && ies_len) {
		buf = kmalloc(ies_len, GFP_KERNEL);
		if (buf == NULL) {
			dev_err(cfg->dev, "%s: kmalloc() failed\n", __func__);
			ret = -ENOMEM;
			goto cleanup;
		}

		pos = ies;

		while (pos + 1 < ies + ies_len) {
			if (pos + 2 + pos[1] > ies + ies_len)
				break;
			if (!(mt7697_is_wpa_ie(pos) || mt7697_is_rsn_ie(pos))) {
				memcpy(buf + len, pos, 2 + pos[1]);
				len += 2 + pos[1];
			}

			if (mt7697_is_wps_ie(pos))
				cfg->connect_ctrl_flags |= MT7697_CONNECT_WPS_FLAG;

			pos += 2 + pos[1];
		}
	}

	print_hex_dump(KERN_DEBUG, DRVNAME" assoc req ", 
		DUMP_PREFIX_OFFSET, 16, 1, buf, len, 0);

//	ret = ath6kl_wmi_set_appie_cmd(ar->wmi, vif->fw_vif_idx,
//				       WMI_FRAME_ASSOC_REQ, buf, len);

cleanup:
	if (buf) kfree(buf);
	return ret;
}

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

static int mt7697_set_auth_type(struct mt7697_vif *vif,
				enum nl80211_auth_type auth_type)
{
	int ret = 0;

	dev_dbg(vif->cfg->dev, "%s: type(%u)\n", __func__, auth_type);

	switch (auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
	case NL80211_AUTHTYPE_SHARED_KEY:
	case NL80211_AUTHTYPE_NETWORK_EAP:
		vif->auto_connect = false;
		break;

	case NL80211_AUTHTYPE_AUTOMATIC:
		vif->auto_connect = true;
		break;

	default:
		dev_err(vif->cfg->dev, "%s: type(%u) not supported\n", 
			__func__, auth_type);
		ret = -ENOTSUPP;
		goto cleanup;
	}

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

cleanup:
	return ret;
}

static bool mt7697_is_valid_iftype(struct mt7697_cfg80211_info *cfg, 
	enum nl80211_iftype type, u8 *if_idx)
{
	int i;

	if (cfg->ibss_if_active || ((type == NL80211_IFTYPE_ADHOC) &&
				   cfg->num_vif))
		return false;

	if (type == NL80211_IFTYPE_STATION ||
	    type == NL80211_IFTYPE_AP || type == NL80211_IFTYPE_ADHOC) {
		for (i = 0; i < cfg->vif_max; i++) {
			if ((cfg->avail_idx_map) & BIT(i)) {
				*if_idx = i;
				return true;
			}
		}
	}

	if (type == NL80211_IFTYPE_P2P_CLIENT ||
	    type == NL80211_IFTYPE_P2P_GO) {
		for (i = cfg->max_norm_iface; i < cfg->vif_max; i++) {
			if ((cfg->avail_idx_map) & BIT(i)) {
				*if_idx = i;
				return true;
			}
		}
	}

	return false;
}

static struct cfg80211_bss *mt7697_add_bss_if_needed(struct mt7697_vif *vif, 
						     const u8* bssid, 
						     u32 freq)
{
	struct ieee80211_channel *chan;
	struct mt7697_cfg80211_info *cfg = vif->cfg;
	struct cfg80211_bss *bss = NULL;
	u16 cap_mask = WLAN_CAPABILITY_ESS;
	u16 cap_val = WLAN_CAPABILITY_ESS;
	u8 *ie = NULL;
	
	chan = ieee80211_get_channel(cfg->wiphy, freq);
	if (!chan) {
		dev_err(cfg->dev, "%s: ieee80211_get_channel(%u) failed\n", 
			__func__, freq);
		goto cleanup;
	}

	bss = cfg80211_get_bss(cfg->wiphy, chan, vif->req_bssid,
			       vif->ssid, vif->ssid_len,
			       cap_mask, cap_val);
	if (!bss) {
		/*
		 * Since cfg80211 may not yet know about the BSS,
		 * generate a partial entry until the first BSS info
		 * event becomes available.
		 *
		 */
		ie = kmalloc(2 + vif->ssid_len, GFP_KERNEL);
		if (ie == NULL) {
			dev_err(cfg->dev, "%s: kmalloc() failed\n", 
				__func__);
			goto cleanup;
		}

		ie[0] = WLAN_EID_SSID;
		ie[1] = vif->ssid_len;
		memcpy(ie + 2, vif->ssid, vif->ssid_len);

		dev_dbg(cfg->dev, "%s: inform bss ssid(%u/'%s')\n",
			__func__, vif->ssid_len, vif->ssid);
		bss = cfg80211_inform_bss(cfg->wiphy, chan,
					  bssid, 0, cap_val, 100,
					  ie, 2 + vif->ssid_len,
					  0, GFP_KERNEL);
		if (!bss) {
			dev_err(cfg->dev, "%s: cfg80211_inform_bss() failed\n", 
				__func__);
			goto cleanup;
		}
			
		dev_dbg(cfg->dev, "%s: added bss %pM to cfg80211\n", 
			__func__, bssid);
	} else
		dev_dbg(cfg->dev, "%s: cfg80211 already has a bss\n", __func__);

cleanup:
	if (ie) kfree(ie);
	return bss;
}

static struct wireless_dev *mt7697_cfg80211_add_iface(struct wiphy *wiphy,
						      const char *name,
						      enum nl80211_iftype type,
						      u32 *flags,
						      struct vif_params *params)
{
	struct mt7697_cfg80211_info *cfg = wiphy_priv(wiphy);
	struct wireless_dev *wdev = NULL;
	u8 if_idx;

	dev_dbg(cfg->dev, "%s: iface('%s') type(%u)\n", __func__, name, type);
	if (cfg->num_vif == cfg->vif_max) {
		dev_err(cfg->dev, "maximum number of supported vif(%u)\n", 
			cfg->num_vif);
		goto cleanup;
	}

	if (!mt7697_is_valid_iftype(cfg, type, &if_idx)) {
		dev_err(cfg->dev, "%s: unsupported interface type(%d)\n", 
			__func__, type);
		goto cleanup;
	}

	wdev = mt7697_interface_add(cfg, name, type, if_idx);
	if (!wdev) {
		dev_err(cfg->dev, "%s: mt7697_interface_add() failed\n", __func__);
		goto cleanup;
	}

	cfg->num_vif++;

cleanup:
	return wdev;
}

static int mt7697_cfg80211_del_iface(struct wiphy *wiphy,
				     struct wireless_dev *wdev)
{
	struct mt7697_cfg80211_info *cfg = wiphy_priv(wiphy);
	struct mt7697_vif *vif = netdev_priv(wdev->netdev);
	int ret;

	dev_dbg(cfg->dev, "%s: DEL IFACE\n", __func__);

	spin_lock_bh(&cfg->vif_list_lock);
	list_del(&vif->list);
	spin_unlock_bh(&cfg->vif_list_lock);

	ret = mt7697_cfg80211_vif_stop(vif, test_bit(WMI_READY, &cfg->flag));
	if (ret < 0) {
		dev_err(cfg->dev, "%s: mt7697_cfg80211_vif_stop() failed(%d)\n", 
			__func__, ret);
                goto cleanup;
	}

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

	dev_dbg(cfg->dev, "%s: iface type(%u)\n", __func__, type);

	switch (type) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_STATION:
		break;
	
	case NL80211_IFTYPE_P2P_GO:
	case NL80211_IFTYPE_P2P_CLIENT:
	default:
		dev_err(cfg->dev, "%s: invalid interface type %u\n", 
			__func__, type);
		ret = -EOPNOTSUPP;
		goto cleanup;
	}

	vif->wdev.iftype = type;

cleanup:
	return ret;
}

static int mt7697_cfg80211_scan(struct wiphy *wiphy, 
	struct cfg80211_scan_request *request)
{
	struct mt7697_vif *vif = mt7697_vif_from_wdev(request->wdev);
	struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	int ret;

	dev_dbg(cfg->dev, "%s: START SCAN\n", __func__);

        vif->scan_req = request;
        ret = mt7697_send_scan_req(cfg, vif->fw_vif_idx, request);
	if (ret < 0) {
		dev_err(cfg->dev, "%s: mt7697_send_scan_req() failed(%d)\n", 
			__func__, ret);
                goto out;
	}

out:
	if (ret < 0) vif->scan_req = NULL;
	return ret;
}

static int mt7697_cfg80211_connect(struct wiphy *wiphy, struct net_device *ndev,
				   struct cfg80211_connect_params *sme)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	u32 interval;
	int ret;

	dev_dbg(cfg->dev, "%s: ssid(%u/'%s')\n", __func__, sme->ssid_len, sme->ssid);

	vif->sme_state = SME_CONNECTING;

	if (down_interruptible(&cfg->sem)) {
		dev_err(cfg->dev, "%s: busy, couldn't get access\n", __func__);
		ret = -ERESTARTSYS;
		goto cleanup;
	}

	if (test_bit(DESTROY_IN_PROGRESS, &cfg->flag)) {
		dev_err(cfg->dev, "%s: busy, destroy in progress\n", __func__);
		ret = -EBUSY;
		goto cleanup;
	}

	ret = mt7697_set_assoc_req_ies(vif, sme->ie, sme->ie_len);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s: mt7697_set_assoc_req_ies() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	if (sme->ie == NULL || sme->ie_len == 0)
		cfg->connect_ctrl_flags &= ~MT7697_CONNECT_WPS_FLAG;

	memset(vif->ssid, 0, sizeof(vif->ssid));
	vif->ssid_len = sme->ssid_len;
	memcpy(vif->ssid, sme->ssid, sme->ssid_len);

	if (sme->channel) {
		vif->ch_hint = sme->channel->center_freq;
		dev_dbg(cfg->dev, 
			"%s: channel(%u)\n", __func__, vif->ch_hint);
	}

	memset(vif->req_bssid, 0, sizeof(vif->req_bssid));
	if (sme->bssid && !is_broadcast_ether_addr(sme->bssid)) {
		dev_dbg(cfg->dev, 
			"%s: BSSID(%02x:%02x:%02x:%02x:%02x:%02x)\n",
			__func__, sme->bssid[0], sme->bssid[1], sme->bssid[2], 
			sme->bssid[3], sme->bssid[4], sme->bssid[5]);

		memcpy(vif->req_bssid, sme->bssid, sizeof(vif->req_bssid));
	}

	ret = mt7697_set_wpa_version(vif, sme->crypto.wpa_versions);
	if (ret < 0) {
		dev_err(cfg->dev, "%s: mt7697_set_wpa_version() failed(%d)\n", 
			__func__, ret);
                goto cleanup;
	}

	ret = mt7697_set_auth_type(vif, sme->auth_type);
	if (ret < 0) {
		dev_err(cfg->dev, "%s: mt7697_set_auth_type() failed(%d)\n", 
			__func__, ret);
                goto cleanup;
	}

	dev_dbg(cfg->dev, "%s: WEP key len/idx(%u/%u)\n", 
		__func__, sme->key_len, sme->key_idx);

	print_hex_dump(KERN_DEBUG, DRVNAME" key ", 
		DUMP_PREFIX_OFFSET, 16, 1, sme->key, sme->key_len, 0);

	print_hex_dump(KERN_DEBUG, DRVNAME" association ie ", 
		DUMP_PREFIX_OFFSET, 16, 1, sme->ie, sme->ie_len, 0);

	if (sme->crypto.n_ciphers_pairwise)
		ret = mt7697_set_cipher(vif, sme->crypto.ciphers_pairwise[0], true);
	else
		ret = mt7697_set_cipher(vif, 0, true);
	if (ret < 0) {
		dev_err(cfg->dev, "mt7697_set_cipher() failed(%d)\n", ret);
                goto cleanup;
	}

	ret = mt7697_set_cipher(vif, sme->crypto.cipher_group, false);
	if (ret < 0) {
		dev_err(cfg->dev, "%s: mt7697_set_cipher() failed(%d)\n", 
			__func__, ret);
                goto cleanup;
	}

	if (sme->crypto.n_akm_suites) {
		ret = mt7697_set_key_mgmt(vif, sme->crypto.akm_suites[0]);
		if (ret < 0) {
			dev_err(cfg->dev, "%s: mt7697_set_key_mgmt() failed(%d)\n", 
				__func__, ret);
                	goto cleanup;
		}
	}

	ret = mt7697_send_set_security_mode_req(cfg, MT7697_PORT_STA, 
		vif->auth_mode, vif->prwise_crypto);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s: mt7697_send_set_security_mode_req() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	if ((sme->key_len) &&
	    (vif->auth_mode == MT7697_WIFI_AUTH_MODE_OPEN) &&
	    (vif->prwise_crypto == MT7697_WIFI_ENCRYPT_TYPE_WEP_ENABLED)) {
		struct mt7697_key *key = NULL;

		if (sme->key_idx > MT7697_MAX_KEY_INDEX) {
			dev_err(cfg->dev, "%s: key index(%d) out of bounds\n",
				   __func__, sme->key_idx);
			ret = -ENOENT;
			goto cleanup;
		}

		key = &vif->keys[sme->key_idx];
		key->key_len = sme->key_len;
		memcpy(key->key, sme->key, key->key_len);
		key->cipher = vif->prwise_crypto;
		vif->def_txkey_index = sme->key_idx;
/*
		ath6kl_wmi_addkey_cmd(ar->wmi, vif->fw_vif_idx, sme->key_idx,
				      vif->prwise_crypto,
				      GROUP_USAGE | TX_USAGE,
				      key->key_len,
				      NULL, 0,
				      key->key, KEY_OP_INIT_VAL, NULL,
				      NO_SYNC_WMIFLAG);*/
	}

	dev_dbg(cfg->dev, "%s: connect auth mode(%d) auto connect(%d) "
		"PW crypto/len(%d/%d) GRP crypto/len(%d/%d) "
		"channel hint(%u)\n",
		__func__, 
		vif->auth_mode, vif->auto_connect, vif->prwise_crypto,
		vif->prwise_crypto_len, vif->grp_crypto,
		vif->grp_crypto_len, vif->ch_hint);

	if (cfg->wifi_config.opmode == MT7697_WIFI_MODE_AP_ONLY) {
		interval = max_t(u8, vif->listen_intvl_t,
				 MT7697_MAX_WOW_LISTEN_INTL);
		ret = mt7697_send_set_listen_interval_req(cfg, interval);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s: mt7697_send_set_listen_interval_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}
	}

	ret = mt7697_send_connect_req(cfg, MT7697_PORT_STA, vif->fw_vif_idx, 
		vif->req_bssid, vif->ssid, vif->ssid_len, vif->ch_hint);
	if (ret < 0) {
		memset(vif->ssid, 0, sizeof(vif->ssid));
		vif->ssid_len = 0;
		dev_err(cfg->dev,"%s: mt7697_send_connect_req() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	if (sme->bg_scan_period == 0) {
		/* disable background scan if period is 0 */
		sme->bg_scan_period = 0xffff;
	} else if (sme->bg_scan_period == -1) {
		/* configure default value if not specified */
		sme->bg_scan_period = MT7697_DEFAULT_BG_SCAN_PERIOD;
	}

	if ((!(cfg->connect_ctrl_flags & MT7697_CONNECT_DO_WPA_OFFLOAD)) &&
	    ((vif->auth_mode == MT7697_WIFI_AUTH_MODE_WPA_PSK) ||
	     (vif->auth_mode == MT7697_WIFI_AUTH_MODE_WPA2_PSK))) {
		mod_timer(&vif->disconnect_timer,
			  jiffies + msecs_to_jiffies(MT7697_DISCON_TIMER_INTVAL));
	}

	cfg->connect_ctrl_flags &= ~MT7697_CONNECT_DO_WPA_OFFLOAD;
	set_bit(CONNECT_PEND, &vif->flags);

cleanup:
	up(&cfg->sem);
	return ret;
}

static int mt7697_cfg80211_disconnect(struct wiphy *wiphy,
				      struct net_device *ndev, u16 reason_code)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	int ret;

	dev_dbg(cfg->dev, "%s: DISCONNECT\n", __func__);

	if (down_interruptible(&cfg->sem)) {
		dev_err(cfg->dev, "%s: down_interruptible() failed\n", __func__);
		ret = -ERESTARTSYS;
		goto cleanup;
	}

	if (test_bit(DESTROY_IN_PROGRESS, &cfg->flag)) {
		dev_err(cfg->dev, "%s: busy, destroy in progress\n", __func__);
		ret = -EBUSY;
		goto cleanup;
	}

	vif->reconnect_flag = 0;

	ret = mt7697_disconnect(vif);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s: mt7697_disconnect() failed(%d)\n", __func__, ret);
		goto cleanup;
	}

	memset(vif->ssid, 0, sizeof(vif->ssid));
	vif->ssid_len = 0;

	vif->sme_state = SME_DISCONNECTED;

cleanup:
	up(&cfg->sem);
	return ret;
}

static int mt7697_cfg80211_add_key(struct wiphy *wiphy, struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr,
				   struct key_params *params)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s: ADD KEY\n", __func__);
	return 0;
}

static int mt7697_cfg80211_get_key(struct wiphy *wiphy, struct net_device *ndev,
                                   u8 key_index, bool pairwise,
                                   const u8 *mac_addr, void *cookie,
                                   void (*callback) (void *cookie,
                                                     struct key_params *))
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s: GET KEY\n", __func__);
	return 0;
}

static int mt7697_cfg80211_del_key(struct wiphy *wiphy, struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr)
{
	struct mt7697_vif *vif = netdev_priv(ndev);
	dev_dbg(vif->cfg->dev, "%s: DEL KEY\n", __func__);
	return 0;
}

static int mt7697_cfg80211_set_default_key(struct wiphy *wiphy,
					   struct net_device *ndev,
					   u8 key_index, bool unicast,
					   bool multicast)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s: SET DEFAULT KEY\n", __func__);
	return 0;
}

static int mt7697_cfg80211_join_ibss(struct wiphy *wiphy,
				     struct net_device *ndev,
				     struct cfg80211_ibss_params *ibss_param)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	int ret = 0;

	dev_dbg(cfg->dev, "%s: JOIN IBSS('%s')\n", __func__, ibss_param->ssid);

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

	dev_dbg(cfg->dev, "%s: LEAVE IBSS\n", __func__);

	mt7697_disconnect(vif);
	memset(vif->ssid, 0, sizeof(vif->ssid));
	vif->ssid_len = 0;

	return ret;
}

static int mt7697_cfg80211_set_pmksa(struct wiphy *wiphy, 
				     struct net_device *ndev,
                            	     struct cfg80211_pmksa *pmksa)
{
        struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	int ret;

	dev_dbg(cfg->dev, "%s: SET PMKSA\n", __func__);

	if (pmksa->bssid == NULL) {
		dev_err(cfg->dev, "%s: NULL BSSID\n", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

	if (pmksa->pmkid == NULL) {
		dev_err(cfg->dev, "%s: NULL PMKID\n", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s: BSSID(%02x:%02x:%02x:%02x:%02x:%02x)\n",
		__func__,
		pmksa->bssid[0], pmksa->bssid[1], pmksa->bssid[2], 
		pmksa->bssid[3], pmksa->bssid[4], pmksa->bssid[5]);

	if (memcmp(pmksa->pmkid, cfg->pmkid, MT7697_WIFI_LENGTH_PMK)) {
		ret = mt7697_send_set_pmk_req(cfg, MT7697_PORT_STA, pmksa->pmkid);
		if (ret) {
			dev_err(cfg->dev, 
				"%s: mt7697_send_set_pmk_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		memcpy(cfg->pmkid, pmksa->pmkid, MT7697_WIFI_LENGTH_PMK);
	}

cleanup:
	return ret;
}

static int mt7697_cfg80211_del_pmksa(struct wiphy *wiphy, 
				     struct net_device *ndev,
                            	     struct cfg80211_pmksa *pmksa)
{
	u8 pmkid[MT7697_WIFI_LENGTH_PMK] = {0};
        struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	int ret;

	dev_dbg(cfg->dev, "%s: DEL PMKSA\n", __func__);

	dev_dbg(cfg->dev, "%s: BSSID(%02x:%02x:%02x:%02x:%02x:%02x)\n",
		__func__,
		pmksa->bssid[0], pmksa->bssid[1], pmksa->bssid[2], 
		pmksa->bssid[3], pmksa->bssid[4], pmksa->bssid[5]);

	if (memcmp(pmkid, cfg->pmkid, MT7697_WIFI_LENGTH_PMK)) {
		ret = mt7697_send_set_pmk_req(cfg, MT7697_PORT_STA, pmkid);
		if (ret) {
			dev_err(cfg->dev, 
				"%s: mt7697_send_set_pmk_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		memset(cfg->pmkid, 0, MT7697_WIFI_LENGTH_PMK);
	}

cleanup:
	return ret;
}

static int mt7697_cfg80211_flush_pmksa(struct wiphy *wiphy, 
				       struct net_device *ndev)
{
	u8 pmk[MT7697_WIFI_LENGTH_PMK] = {0};
        struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	int ret = 0;

	dev_dbg(cfg->dev, "%s: FLUSH PMKSA\n", __func__);

	if (test_bit(CONNECTED, &vif->flags)) {
		ret = mt7697_send_set_pmk_req(cfg, MT7697_PORT_STA, pmk);
		if (ret) {
			dev_err(cfg->dev, 
				"%s: mt7697_send_set_pmk_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}

		memset(cfg->pmkid, 0, MT7697_WIFI_LENGTH_PMK);
	}

cleanup:
	return ret;
}

static int mt7697_cfg80211_del_station(struct wiphy *wiphy, 
				       struct net_device *ndev,
                                       u8 *mac)
{
        struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s: DEL STATION\n", __func__);
	return 0;
}

static int mt7697_cfg80211_change_station(struct wiphy *wiphy,
					  struct net_device *ndev,
				 	  u8 *mac, 
					  struct station_parameters *params)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct mt7697_vif *vif = netdev_priv(ndev);
	int err = 0;

	dev_dbg(cfg->dev, "%s: CHANGE STATION\n\t"
		"mac(%02x:%02x:%02x:%02x:%02x:%02x)\n\tsta flags(0x%08x)\n",
		__func__, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
		params->sta_flags_set);

	if ((vif->wdev.iftype != NL80211_IFTYPE_STATION)) {
		dev_err(cfg->dev, "%s: iftype(%u) not supported\n", 
			__func__, vif->wdev.iftype);
		err = -EOPNOTSUPP;
		goto cleanup;
	}

	err = cfg80211_check_station_change(wiphy, params,
					    CFG80211_STA_AP_MLME_CLIENT);
	if (err) {
		dev_err(cfg->dev, 
			"%s: cfg80211_check_station_change() failed(%d)\n", 
			__func__, err);
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s: set mlme('%s')\n",
		__func__,  
		params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED) ?
		"AUTHORIZED":"UNAUTHORIZE");
/*
	err = mt7697_wmi_ap_set_mlme(ar->wmi, vif->fw_vif_idx,
		(params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED)) ? 
			WMI_AP_MLME_AUTHORIZE:WMI_AP_MLME_UNAUTHORIZE, mac, 0);
	if (err) {
		dev_err(cfg->dev, 
			"mt7697_wmi_ap_set_mlme() failed(%d)\n", err);
		goto cleanup;
	}
*/
cleanup:
	return err;
}

static int mt7697_cfg80211_remain_on_channel(struct wiphy *wiphy,
                                    	     struct wireless_dev *wdev,
                                    	     struct ieee80211_channel *chan,
                                    	     unsigned int duration,
                                    	     u64 *cookie)
{
        struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	dev_dbg(cfg->dev, "%s: REMAIN ON CH\n", __func__);
	return 0;
}

static int mt7697_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
                                           	    struct wireless_dev *wdev,
                                           	    u64 cookie)
{
        struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	dev_dbg(cfg->dev, "%s: CANCEL REMAIN ON CH\n", __func__);
	return 0;
}

static int mt7697_cfg80211_mgmt_tx(struct wiphy *wiphy, 
				   struct wireless_dev *wdev,
                          	   struct cfg80211_mgmt_tx_params *params, 
				   u64 *cookie)
{
        struct mt7697_cfg80211_info *cfg = wiphy_to_cfg(wiphy);
	dev_dbg(cfg->dev, "%s: MGMT TX len(%u)\n", __func__, params->len);

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
	dev_dbg(cfg->dev, "%s: MGMT FRAME REG type(0x%04x) reg(%u)\n", 
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
	.add_virtual_intf 	= mt7697_cfg80211_add_iface,
	.del_virtual_intf 	= mt7697_cfg80211_del_iface,
	.change_virtual_intf 	= mt7697_cfg80211_change_iface,
	.scan 			= mt7697_cfg80211_scan,
	.connect 		= mt7697_cfg80211_connect,
	.disconnect 		= mt7697_cfg80211_disconnect,
	.add_key 		= mt7697_cfg80211_add_key,
	.get_key 		= mt7697_cfg80211_get_key,
	.del_key 		= mt7697_cfg80211_del_key,
	.set_default_key 	= mt7697_cfg80211_set_default_key,
	.join_ibss 		= mt7697_cfg80211_join_ibss,
	.leave_ibss 		= mt7697_cfg80211_leave_ibss,
	.set_pmksa 		= mt7697_cfg80211_set_pmksa,
        .del_pmksa 		= mt7697_cfg80211_del_pmksa,
        .flush_pmksa 		= mt7697_cfg80211_flush_pmksa,
	.del_station 		= mt7697_cfg80211_del_station,
	.change_station 	= mt7697_cfg80211_change_station,
	.remain_on_channel 	= mt7697_cfg80211_remain_on_channel,
        .cancel_remain_on_channel = mt7697_cfg80211_cancel_remain_on_channel,
        .mgmt_tx 		= mt7697_cfg80211_mgmt_tx,
        .mgmt_frame_register 	= mt7697_cfg80211_mgmt_frame_register,
};

void mt7697_cfg80211_stop(struct mt7697_vif *vif)
{
//	ath6kl_cfg80211_sscan_disable(vif);

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
		cfg80211_disconnected(vif->ndev, 0, NULL, 0, GFP_KERNEL);
		break;
	}
/*
	if (vif->cfg->state != ATH6KL_STATE_RECOVERY &&
	    (test_bit(CONNECTED, &vif->flags) ||
	    test_bit(CONNECT_PEND, &vif->flags)))
		ath6kl_wmi_disconnect_cmd(vif->ar->wmi, vif->fw_vif_idx);
*/
	vif->sme_state = SME_DISCONNECTED;
	clear_bit(CONNECTED, &vif->flags);
	clear_bit(CONNECT_PEND, &vif->flags);

	/* Stop netdev queues, needed during recovery */
	netif_stop_queue(vif->ndev);
	netif_carrier_off(vif->ndev);

	/* disable scanning */
/*	if (vif->ar->state != ATH6KL_STATE_RECOVERY &&
	    ath6kl_wmi_scanparams_cmd(vif->ar->wmi, vif->fw_vif_idx, 0xFFFF,
				      0, 0, 0, 0, 0, 0, 0, 0, 0) != 0)
		ath6kl_warn("failed to disable scan during stop\n");
*/
//	ath6kl_cfg80211_scan_complete_event(vif, true);
	cfg80211_scan_done(vif->scan_req, true);
	vif->scan_req = NULL;
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
        struct mt7697_vif *vif = container_of(work, 
		struct mt7697_vif, disconnect_work);
	struct mt7697_cfg80211_info *cfg = vif->cfg;
	int ret;

	ret = mt7697_disconnect(vif);
	if (ret < 0) {
		dev_err(cfg->dev, "%s: mt7697_disconnect() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

cleanup:
	return;
}

struct wireless_dev *mt7697_interface_add(
	struct mt7697_cfg80211_info *cfg, const char *name,
	enum nl80211_iftype type, u8 fw_vif_idx)
{
	struct net_device *ndev;
	struct mt7697_vif *vif;

	dev_err(cfg->dev, "%s: add interface('%s') type(%u)\n", 
		__func__, name, type);

	ndev = alloc_netdev(sizeof(struct mt7697_vif), name, ether_setup);
	if (!ndev) {
		dev_err(cfg->dev, "%s: alloc_netdev() failed\n", __func__);
		return NULL;
	}

	vif = netdev_priv(ndev);
	ndev->ieee80211_ptr = &vif->wdev;
	vif->wdev.wiphy = cfg->wiphy;
	vif->cfg = cfg;
	vif->ndev = ndev;
	INIT_WORK(&vif->disconnect_work, mt7697_cfg80211_disconnect_work);
	INIT_LIST_HEAD(&vif->list);
	SET_NETDEV_DEV(ndev, wiphy_dev(vif->wdev.wiphy));
	vif->wdev.netdev = ndev;
	vif->wdev.iftype = type;
	vif->fw_vif_idx = fw_vif_idx;

	memcpy(ndev->dev_addr, &cfg->mac_addr, ETH_ALEN);

	mt7697_init_netdev(ndev);

	if (mt7697_cfg80211_vif_init(vif)) {
		dev_err(cfg->dev, "%s: mt7697_cfg80211_vif_init() failed\n", 
			__func__);
		goto err;
	}

	if (register_netdevice(ndev)) {
		dev_err(cfg->dev, "%s: register_netdevice() failed\n", 
			__func__);
		goto err;
	}

	vif->sme_state = SME_DISCONNECTED;
	set_bit(WLAN_ENABLED, &vif->flags);

	spin_lock_bh(&cfg->vif_list_lock);
	list_add_tail(&vif->list, &cfg->vif_list);
	spin_unlock_bh(&cfg->vif_list_lock);

	return &vif->wdev;

err:
	free_netdev(ndev);
	return NULL;
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

	if (channel <= MT7697_CH_MAX_2G_CHANNEL)
		band = wiphy->bands[IEEE80211_BAND_2GHZ];
	else
		band = wiphy->bands[IEEE80211_BAND_5GHZ];

	freq = ieee80211_channel_to_frequency(channel, band->band);

	bss = mt7697_add_bss_if_needed(vif, bssid, freq);
	if (!bss) {
		dev_err(cfg->dev, "%s: could not add cfg80211 bss entry\n",
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}

	if (vif->sme_state == SME_CONNECTING) {
		vif->sme_state = SME_CONNECTED;
		cfg80211_connect_result(vif->ndev, bssid,
					NULL, 0,
					NULL, 0, 
//					assoc_req_ie, assoc_req_len,
//					assoc_resp_ie, assoc_resp_len,
					WLAN_STATUS_SUCCESS, GFP_KERNEL);
		cfg80211_put_bss(cfg->wiphy, bss);
	} else if (vif->sme_state == SME_CONNECTED) {
		/* inform roam event to cfg80211 */
		cfg80211_roamed_bss(vif->ndev, bss,
				    NULL, 0,
				    NULL, 0,  
//				    assoc_req_ie, assoc_req_len,
//				    assoc_resp_ie, assoc_resp_len, 
				    GFP_KERNEL);
	}

	netif_wake_queue(vif->ndev);

	/* Update connect & link status atomically */
	spin_lock_bh(&vif->if_lock);
	set_bit(CONNECTED, &vif->flags);
	clear_bit(CONNECT_PEND, &vif->flags);
	netif_carrier_on(vif->ndev);
	spin_unlock_bh(&vif->if_lock);

cleanup:
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

int mt7697_cfg80211_init(struct mt7697_cfg80211_info *cfg)
{
	struct wiphy *wiphy = cfg->wiphy;
	bool band_2gig = false, band_5gig = false, ht = false;
	s32 err = 0;

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
		dev_err(cfg->dev, "%s: invalid phy capability!\n", __func__);
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

	if (band_2gig)
        	wiphy->bands[IEEE80211_BAND_2GHZ] = &mt7697_band_2ghz;
	if (band_5gig)
		wiphy->bands[IEEE80211_BAND_5GHZ] = &mt7697_band_5ghz;

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
                dev_err(cfg->dev, "%s: wiphy_register() failed(%d)\n", 
			__func__, err);               
                goto cleanup;
        }

	cfg->wiphy_registered = true;

cleanup:
        return err;
}

void mt7697_cfg80211_cleanup(struct mt7697_cfg80211_info *cfg)
{
	wiphy_unregister(cfg->wiphy);
	cfg->wiphy_registered = false;
}

struct mt7697_cfg80211_info *mt7697_cfg80211_create(void)
{
	struct mt7697_cfg80211_info *cfg = NULL;
	struct wiphy *wiphy;

	/* create a new wiphy for use with cfg80211 */
	wiphy = wiphy_new(&mt7697_cfg80211_ops, sizeof(struct mt7697_cfg80211_info));
	if (wiphy) {
		cfg = wiphy_priv(wiphy);
		cfg->wiphy = wiphy;
	}

	return cfg;
}

void mt7697_cfg80211_destroy(struct mt7697_cfg80211_info *cfg)
{
	wiphy_free(cfg->wiphy);
}
