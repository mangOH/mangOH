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

#include <linux/capability.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/etherdevice.h>

#include <net/iw_handler.h>
#include <net/cfg80211.h>
#include <net/cfg80211-wext.h>

#include "core.h"
#include "ioctl.h"

static int mt7697_wext_siwfreq(struct net_device *ndev,
                               struct iw_request_info *info,
                               struct iw_freq *frq,
                               char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	struct mt7697_vif *vif = mt7697_vif_from_wdev(wdev);
	int chan = -1;

	if ((frq->e == 0) && (frq->m <= 1000)) {
                /* Setting by channel number */
                chan = frq->m;
		dev_dbg(cfg->dev, "%s(): freq(%u)\n", __func__, frq->m);
        } else {
                /* Setting by frequency */
                int denom = 1;
                int i;

                /* Calculate denominator to rescale to MHz */
                for (i = 0; i < (6 - frq->e); i++)
                        denom *= 10;

                chan = ieee80211_freq_to_dsss_chan(frq->m / denom);
		dev_dbg(cfg->dev, "%s(): chan(%u)\n", __func__, chan);
        }

	vif->ch_hint = chan;
	return 0;
}

static int mt7697_wext_giwfreq(struct net_device *ndev,
                               struct iw_request_info *info,
                               struct iw_freq *frq,
                               char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_siwmode(struct net_device *ndev, 
			       struct iw_request_info *info,
                               u32 *mode, char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
        struct wireless_dev *wdev = ndev->ieee80211_ptr;
	struct mt7697_vif *vif = mt7697_vif_from_wdev(wdev);
        int ret = 0;

        dev_dbg(cfg->dev, "%s(): mode(%u)\n", __func__, *mode);
        switch (*mode) {
	case IW_MODE_MASTER:
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
        case IW_MODE_INFRA:
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
        default:
                dev_err(cfg->dev, "%s(): unsupported mode(%u)\n", 
			__func__, *mode);
                ret = -EINVAL;
		goto cleanup;
        }

cleanup:
	return ret;
}

static int mt7697_wext_giwrange(struct net_device *ndev, 
		                struct iw_request_info *info,
                                struct iw_point *dwrq, char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct wiphy *wiphy = cfg->wiphy;
        struct iw_range *range = (struct iw_range *) extra;
        int i;

	dev_dbg(cfg->dev, "%s():\n", __func__);

        memset(range, 0, sizeof (struct iw_range));
        dwrq->length = sizeof (struct iw_range);

        /* set the wireless extension version number */
        range->we_version_source = SUPPORTED_WIRELESS_EXT;
        range->we_version_compiled = WIRELESS_EXT;

        /* Now the encoding capabilities */
        range->num_encoding_sizes = 3;
        /* 64(40) bits WEP */
        range->encoding_size[0] = 5;
        /* 128(104) bits WEP */
        range->encoding_size[1] = 13;
        /* 256 bits for WPA-PSK */
        range->encoding_size[2] = 32;
        /* 4 keys are allowed */
        range->max_encoding_tokens = 4;

        /* we don't know the quality range... */
        range->max_qual.level = 0;
        range->max_qual.noise = 0;
        range->max_qual.qual = 0;
        /* these value describe an average quality. Needs more tweaking... */
        range->avg_qual.level = -80;    /* -80 dBm */
        range->avg_qual.noise = 0;      /* don't know what to put here */
        range->avg_qual.qual = 0;

        range->sensitivity = 200;

        /* retry limit capabilities */
        range->retry_capa = IW_RETRY_LIMIT | IW_RETRY_LIFETIME;
        range->retry_flags = IW_RETRY_LIMIT;
        range->r_time_flags = IW_RETRY_LIFETIME;

        /* I don't know the range. Put stupid things here */
        range->min_retry = 1;
        range->max_retry = 65535;
        range->min_r_time = 1024;
        range->max_r_time = 65535 * 1024;

        /* txpower is supported in dBm's */
        range->txpower_capa = IW_TXPOW_DBM;

        /* Event capability (kernel + driver) */
        range->event_capa[0] = IW_EVENT_CAPA_K_0;
        range->event_capa[1] = IW_EVENT_CAPA_K_1;

        range->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 |
                          IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_4WAY_HANDSHAKE;

        range->num_channels = wiphy->bands[IEEE80211_BAND_2GHZ]->n_channels + 
			      wiphy->bands[IEEE80211_BAND_5GHZ]->n_channels;
        range->num_frequency = wiphy->bands[IEEE80211_BAND_2GHZ]->n_channels + 
			       wiphy->bands[IEEE80211_BAND_5GHZ]->n_channels;

        for (i = 0; i < wiphy->bands[IEEE80211_BAND_2GHZ]->n_channels; i++) {
                range->freq[i].m = wiphy->bands[IEEE80211_BAND_2GHZ]->channels[i].center_freq;
                range->freq[i].e = 6;
                range->freq[i].i = wiphy->bands[IEEE80211_BAND_2GHZ]->channels[i].hw_value;
        }
        
	for (i = 0; i < wiphy->bands[IEEE80211_BAND_5GHZ]->n_channels; i++) {
                range->freq[i].m = wiphy->bands[IEEE80211_BAND_5GHZ]->channels[i].center_freq;
                range->freq[i].e = 6;
                range->freq[i].i = wiphy->bands[IEEE80211_BAND_5GHZ]->channels[i].hw_value;
        }

	range->num_bitrates = wiphy->bands[IEEE80211_BAND_2GHZ]->n_bitrates + 
                              wiphy->bands[IEEE80211_BAND_5GHZ]->n_bitrates;
        for (i = 0; i < wiphy->bands[IEEE80211_BAND_2GHZ]->n_bitrates; i++) {
                range->bitrate[i] = wiphy->bands[IEEE80211_BAND_2GHZ]->bitrates[i].bitrate * 100000;
        }

        for (i = 0; i < wiphy->bands[IEEE80211_BAND_5GHZ]->n_bitrates; i++) {
                range->bitrate[i] = wiphy->bands[IEEE80211_BAND_5GHZ]->bitrates[i].bitrate * 100000;
        }

        return 0;
}

static int mt7697_wext_siwap(struct net_device *ndev,
                             struct iw_request_info *info,
                             struct sockaddr *ap_addr,
                             char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	struct mt7697_vif *vif = mt7697_vif_from_wdev(wdev);
	u8 *bssid = ap_addr->sa_data;

	dev_dbg(cfg->dev, "%s():\n", __func__);
	if (bssid) {
		print_hex_dump(KERN_DEBUG, DRVNAME" BSSID ", 
			DUMP_PREFIX_OFFSET, 16, 1, bssid, ETH_ALEN, 0);

		memcpy(vif->req_bssid, bssid, ETH_ALEN);
	}

	return 0;
}

static int mt7697_wext_giwap(struct net_device *ndev,
                             struct iw_request_info *info,
                             struct sockaddr *ap_addr,
                             char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_siwmlme(struct net_device *ndev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu, char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_siwessid(struct net_device *ndev,
                                struct iw_request_info *info,
                                struct iw_point *data,
                                char *ssid)
{
	u8 pmk[MT7697_WIFI_LENGTH_PMK] = {0};
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	struct mt7697_vif *vif = mt7697_vif_from_wdev(wdev);
        size_t len = data->length;
        int ret = 0;

	dev_dbg(cfg->dev, "%s(): iftype(%u) ssid('%s')\n", 
		__func__, wdev->iftype, ssid);

        /* call only for station! */
        if (WARN_ON((wdev->iftype != NL80211_IFTYPE_STATION) && 
		    (wdev->iftype != NL80211_IFTYPE_AP))) {
		dev_warn(cfg->dev, "%s(): unsupported iftype(%u)\n", 
			__func__, wdev->iftype);
                ret = -EINVAL;
		goto cleanup;
	}

        if (!data->flags)
                len = 0;

        /* iwconfig uses nul termination in SSID.. */
        if (len > 0 && ssid[len - 1] == '\0')
                len--;

	memset(vif->ssid, 0, sizeof(vif->ssid));
	vif->ssid_len = len;
	memcpy(vif->ssid, ssid, len);

	memcpy(wdev->ssid, ssid, len);
        wdev->ssid_len = len;

	ret = mt7697_wr_set_ssid_req(cfg, len, ssid);
	if (ret < 0) {
		dev_err(cfg->dev, 
			"%s(): mt7697_wr_set_ssid_req() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

	if (cfg->wifi_cfg.opmode == MT7697_WIFI_MODE_STA_ONLY) {
		ret = mt7697_wr_set_bssid_req(cfg, vif->req_bssid);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_wr_set_channel_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}
	}

	if (vif->ch_hint > 0) {
		ret = mt7697_wr_set_channel_req(cfg, vif->ch_hint);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_wr_set_channel_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}
	}

	if (memcmp(pmk, vif->pmk, sizeof(pmk))) {
		ret = mt7697_wr_set_pmk_req(cfg, vif->pmk);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_wr_set_pmk_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}
	}
	else {
		ret = mt7697_wr_set_security_mode_req(cfg, 
                	MT7697_WIFI_AUTH_MODE_OPEN, 
			MT7697_WIFI_ENCRYPT_TYPE_ENCRYPT_DISABLED);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_wr_set_security_mode_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}
	}

	if (len > 0) {
		if (test_bit(CONNECTED, &vif->flags)) {
			dev_dbg(cfg->dev, "%s(): already connected\n", 
				__func__);
			goto cleanup;
		}

		vif->sme_state = SME_CONNECTING;

		if (test_bit(DESTROY_IN_PROGRESS, &cfg->flag)) {
			dev_err(cfg->dev, "%s(): busy, destroy in progress\n", 
				__func__);
			ret = -EBUSY;
			goto cleanup;
		}

		ret = mt7697_wr_reload_settings_req(cfg, vif->fw_vif_idx);
		if (ret < 0) {
			dev_err(cfg->dev, 
				"%s(): mt7697_wr_reload_settings_req() failed(%d)\n", 
				__func__, ret);
			goto cleanup;
		}
	}

cleanup:
	return ret;
}

static int mt7697_wext_giwessid(struct net_device *ndev,
                                struct iw_request_info *info,
                                struct iw_point *erq,
                                char *essidbuf)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_siwrate(struct net_device *ndev,
                               struct iw_request_info *info,
                               struct iw_param *rrq,
                               char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_giwrate(struct net_device *ndev,
                               struct iw_request_info *info,
                               struct iw_param *rrq,
                               char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_siwencode(struct net_device *ndev,
                                 struct iw_request_info *info,
                                 struct iw_point *erq,
                                 char *keybuf)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_giwencode(struct net_device *ndev,
                                 struct iw_request_info *info,
                                 struct iw_point *erq,
                                 char *keybuf)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_siwpower(struct net_device *ndev,
                                struct iw_request_info *info,
                                struct iw_param *prq,
                                char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_giwpower(struct net_device *ndev,
                                struct iw_request_info *info,
                                struct iw_param *prq,
                                char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_siwgenie(struct net_device *ndev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu, char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_giwgenie(struct net_device *ndev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu, char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_siwauth(struct net_device *ndev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_giwauth(struct net_device *ndev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static int mt7697_wext_siwencodeext(struct net_device *ndev,
                                    struct iw_request_info *info,
                                    struct iw_point *erq,
                                    char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	struct mt7697_vif *vif = mt7697_vif_from_wdev(wdev);
	struct iw_encode_ext *ext = (struct iw_encode_ext *) extra;
        int ret = 0;

	dev_dbg(cfg->dev, "%s(): iftype(%u)\n", __func__, wdev->iftype);
        if (wdev->iftype != NL80211_IFTYPE_STATION &&
            wdev->iftype != NL80211_IFTYPE_AP) {
		dev_err(cfg->dev, "%s(): iftype(%d) not supported\n", 
			__func__, wdev->iftype);
                ret = -EOPNOTSUPP;
		goto cleanup;
	}

	dev_dbg(cfg->dev, "%s(): alg(%u)\n", __func__, ext->alg);
	if (ext->alg == IW_ENCODE_ALG_PMK) {
		print_hex_dump(KERN_DEBUG, DRVNAME" PMK ", 
			DUMP_PREFIX_OFFSET, 16, 1, ext->key, ext->key_len, 0);

		memcpy(vif->pmk, ext->key, ext->key_len);

		if (wdev->iftype == NL80211_IFTYPE_AP) {
			ret = mt7697_wr_set_security_mode_req(cfg, 
                                                      MT7697_WIFI_AUTH_MODE_WPA2_PSK, 
						      MT7697_WIFI_ENCRYPT_TYPE_AES_ENABLED);
			if (ret < 0) {
				dev_err(cfg->dev, 
					"%s(): mt7697_wr_set_security_mode_req() failed(%d)\n", 
					__func__, ret);
                		goto cleanup;
			}
		}
        }

cleanup:
	return ret;
}

static int mt7697_wext_giwencodeext(struct net_device *ndev,
                                    struct iw_request_info *info,
                                    union iwreq_data *wrqu,
                                    char *extra)
{
	struct mt7697_cfg80211_info *cfg = mt7697_priv(ndev);
	dev_dbg(cfg->dev, "%s():\n", __func__);
	return 0;
}

static const iw_handler mt7697_wireless_handler[] = {
        [IW_IOCTL_IDX(SIOCGIWNAME)]     = (iw_handler) cfg80211_wext_giwname,
        [IW_IOCTL_IDX(SIOCSIWFREQ)]     = (iw_handler) mt7697_wext_siwfreq,
        [IW_IOCTL_IDX(SIOCGIWFREQ)]     = (iw_handler) mt7697_wext_giwfreq,
        [IW_IOCTL_IDX(SIOCSIWMODE)]     = (iw_handler) mt7697_wext_siwmode,
        [IW_IOCTL_IDX(SIOCGIWMODE)]     = (iw_handler) cfg80211_wext_giwmode,
        [IW_IOCTL_IDX(SIOCGIWRANGE)]    = (iw_handler) mt7697_wext_giwrange,
        [IW_IOCTL_IDX(SIOCSIWAP)]       = (iw_handler) mt7697_wext_siwap,
        [IW_IOCTL_IDX(SIOCGIWAP)]       = (iw_handler) mt7697_wext_giwap,
        [IW_IOCTL_IDX(SIOCSIWMLME)]     = (iw_handler) mt7697_wext_siwmlme,
        [IW_IOCTL_IDX(SIOCSIWSCAN)]     = (iw_handler) cfg80211_wext_siwscan,
        [IW_IOCTL_IDX(SIOCGIWSCAN)]     = (iw_handler) cfg80211_wext_giwscan,
        [IW_IOCTL_IDX(SIOCSIWESSID)]    = (iw_handler) mt7697_wext_siwessid,
        [IW_IOCTL_IDX(SIOCGIWESSID)]    = (iw_handler) mt7697_wext_giwessid,
        [IW_IOCTL_IDX(SIOCSIWRATE)]     = (iw_handler) mt7697_wext_siwrate,
        [IW_IOCTL_IDX(SIOCGIWRATE)]     = (iw_handler) mt7697_wext_giwrate,
        [IW_IOCTL_IDX(SIOCSIWRTS)]      = (iw_handler) cfg80211_wext_siwrts,
        [IW_IOCTL_IDX(SIOCGIWRTS)]      = (iw_handler) cfg80211_wext_giwrts,
        [IW_IOCTL_IDX(SIOCSIWFRAG)]     = (iw_handler) cfg80211_wext_siwfrag,
        [IW_IOCTL_IDX(SIOCGIWFRAG)]     = (iw_handler) cfg80211_wext_giwfrag,
        [IW_IOCTL_IDX(SIOCGIWRETRY)]    = (iw_handler) cfg80211_wext_giwretry,
        [IW_IOCTL_IDX(SIOCSIWENCODE)]   = (iw_handler) mt7697_wext_siwencode,
        [IW_IOCTL_IDX(SIOCGIWENCODE)]   = (iw_handler) mt7697_wext_giwencode,
        [IW_IOCTL_IDX(SIOCSIWPOWER)]    = (iw_handler) mt7697_wext_siwpower,
        [IW_IOCTL_IDX(SIOCGIWPOWER)]    = (iw_handler) mt7697_wext_giwpower,
        [IW_IOCTL_IDX(SIOCSIWGENIE)]    = (iw_handler) mt7697_wext_siwgenie,
	[IW_IOCTL_IDX(SIOCGIWGENIE)]    = (iw_handler) mt7697_wext_giwgenie,
        [IW_IOCTL_IDX(SIOCSIWAUTH)]     = (iw_handler) mt7697_wext_siwauth,
        [IW_IOCTL_IDX(SIOCGIWAUTH)]     = (iw_handler) mt7697_wext_giwauth,
        [IW_IOCTL_IDX(SIOCSIWENCODEEXT)]= (iw_handler) mt7697_wext_siwencodeext,
	[IW_IOCTL_IDX(SIOCGIWENCODEEXT)]= (iw_handler) mt7697_wext_giwencodeext,
};

const struct iw_handler_def mt7697_wireless_hndlrs = {
        .num_standard = ARRAY_SIZE(mt7697_wireless_handler),
        .standard = (iw_handler *) mt7697_wireless_handler,
	.get_wireless_stats = NULL,
};

