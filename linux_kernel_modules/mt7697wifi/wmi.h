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

#ifndef _MT7697_CMD_H_
#define _MT7697_CMD_H_

#include <linux/ieee80211.h>
#include <linux/if_ether.h>

#include "mt7697_i.h"
#include "wifi_api.h"

#define MT7697_WOW_MAX_FILTERS_PER_LIST 4
#define MT7697_WOW_PATTERN_SIZE	 	64
#define MT7697_PASSPHRASE_LEN		64

#define mt7697_cfg_req			 mt7697_cmd_hdr
#define mt7697_get_rx_filter_req	 mt7697_cmd_hdr
#define mt7697_get_listen_interval_req	 mt7697_cmd_hdr
#define mt7697_get_smart_conn_filter_req mt7697_cmd_hdr
#define mt7697_scan_stop		 mt7697_cmd_hdr

#define mt7697_set_wireless_mode_rsp	 mt7697_rsp_hdr
#define mt7697_set_op_mode_rsp		 mt7697_rsp_hdr
#define mt7697_set_rx_filter_rsp	 mt7697_rsp_hdr
#define mt7697_set_smart_conn_filter_rsp mt7697_rsp_hdr
#define mt7697_set_listen_interval_rsp	 mt7697_rsp_hdr
#define mt7697_set_pmk_rsp		 mt7697_rsp_hdr
#define mt7697_set_channel_rsp		 mt7697_rsp_hdr
#define mt7697_set_bssid_rsp		 mt7697_rsp_hdr
#define mt7697_set_ssid_rsp		 mt7697_rsp_hdr
#define mt7697_set_security_mode_rsp	 mt7697_rsp_hdr
#define mt7697_scan_stop_rsp		 mt7697_rsp_hdr
#define mt7697_reload_settings_rsp	 mt7697_rsp_hdr
#define mt7697_disconnect_rsp		 mt7697_rsp_hdr

enum mt7697_connect_ctrl_flags_bits {
	MT7697_CONNECT_ASSOC_POLICY_USER        = 0x0001,
	MT7697_CONNECT_SEND_REASSOC             = 0x0002,
	MT7697_CONNECT_IGNORE_WPAx_GROUP_CIPHER = 0x0004,
	MT7697_CONNECT_PROFILE_MATCH_DONE       = 0x0008,
	MT7697_CONNECT_IGNORE_AAC_BEACON        = 0x0010,
	MT7697_CONNECT_CSA_FOLLOW_BSS           = 0x0020,
	MT7697_CONNECT_DO_WPA_OFFLOAD           = 0x0040,
	MT7697_CONNECT_DO_NOT_DEAUTH            = 0x0080,
	MT7697_CONNECT_WPS_FLAG                 = 0x0100,
};

enum mt7697_wifi_cmd_types {
	MT7697_CMD_MAC_ADDR_REQ = 0,
	MT7697_CMD_MAC_ADDR_RSP,
	MT7697_CMD_GET_CFG_REQ,
	MT7697_CMD_GET_CFG_RSP,
	MT7697_CMD_GET_WIRELESS_MODE_REQ,
	MT7697_CMD_GET_WIRELESS_MODE_RSP,
	MT7697_CMD_SET_WIRELESS_MODE_REQ,
	MT7697_CMD_SET_WIRELESS_MODE_RSP,
	MT7697_CMD_SET_OP_MODE_REQ,
	MT7697_CMD_SET_OP_MODE_RSP,
	MT7697_CMD_GET_LISTEN_INTERVAL_REQ,
	MT7697_CMD_GET_LISTEN_INTERVAL_RSP,
	MT7697_CMD_SET_LISTEN_INTERVAL_REQ,
	MT7697_CMD_SET_LISTEN_INTERVAL_RSP,
	MT7697_CMD_SET_SECURITY_MODE_REQ,
	MT7697_CMD_SET_SECURITY_MODE_RSP,
	MT7697_CMD_GET_SECURITY_MODE_REQ,
	MT7697_CMD_GET_SECURITY_MODE_RSP,
	MT7697_CMD_SCAN_IND,
	MT7697_CMD_SCAN_REQ,
	MT7697_CMD_SCAN_RSP,
	MT7697_CMD_SCAN_COMPLETE_IND,
	MT7697_CMD_SCAN_STOP,
	MT7697_CMD_SCAN_STOP_RSP,
	MT7697_CMD_SET_PMK_REQ,
	MT7697_CMD_SET_PMK_RSP,
	MT7697_CMD_SET_CHANNEL_REQ,
	MT7697_CMD_SET_CHANNEL_RSP,
	MT7697_CMD_SET_BSSID_REQ,
	MT7697_CMD_SET_BSSID_RSP,
	MT7697_CMD_SET_SSID_REQ,
	MT7697_CMD_SET_SSID_RSP,
	MT7697_CMD_RELOAD_SETTINGS_REQ,
	MT7697_CMD_RELOAD_SETTINGS_RSP,
	MT7697_CMD_CONNECT_IND,
	MT7697_CMD_DISCONNECT_IND,
	MT7697_CMD_DISCONNECT_REQ,
	MT7697_CMD_DISCONNECT_RSP,
	MT7697_CMD_TX_RAW,
	MT7697_CMD_RX_RAW,
};

struct mt7697_cfg80211_info;
struct cfg80211_scan_request;

struct mt7697_mac_addr_req {
	struct mt7697_cmd_hdr cmd;
	__be32                port;
} __attribute__((packed, aligned(4)));

struct mt7697_mac_addr_rsp {
	struct mt7697_rsp_hdr rsp;
	u8                    addr[LEN32_ALIGNED(ETH_ALEN)];
} __attribute__((packed, aligned(4)));

struct mt7697_get_wireless_mode_req {
	struct mt7697_cmd_hdr cmd;
	__be32                port;
} __attribute__((packed, aligned(4)));

struct mt7697_get_wireless_mode_rsp {
	struct mt7697_rsp_hdr rsp;
	__be32                mode;
} __attribute__((packed, aligned(4)));

struct mt7697_set_wireless_mode_req {
	struct mt7697_cmd_hdr cmd;
	__be32                port;
	__be32                mode;
} __attribute__((packed, aligned(4)));

struct mt7697_cfg_rsp {
	struct mt7697_rsp_hdr       rsp;
	struct mt7697_wifi_config_t cfg;
} __attribute__((packed, aligned(4)));

struct mt7697_set_op_mode_req {
	struct mt7697_cmd_hdr cmd;
	__be32                opmode;
} __attribute__((packed, aligned(4)));

struct mt7697_get_listen_interval_rsp {
	struct mt7697_rsp_hdr rsp;
	__be32                interval;
} __attribute__((packed, aligned(4)));

struct mt7697_set_listen_interval_req {
	struct mt7697_cmd_hdr cmd;
	__be32                interval;
} __attribute__((packed, aligned(4)));

struct mt7697_reload_settings_req {
	struct mt7697_cmd_hdr cmd;
	__be32                if_idx;
} __attribute__((packed, aligned(4)));

struct mt7697_scan_req {
	struct mt7697_cmd_hdr cmd;
	__be32                if_idx;
	__be32                mode;
	__be32                option;
	__be32                bssid_len;
	u8                    bssid[LEN32_ALIGNED(ETH_ALEN)];
	__be32                ssid_len;
	u8                    ssid[LEN32_ALIGNED(IEEE80211_MAX_SSID_LEN)];
} __attribute__((packed, aligned(4)));

struct mt7697_scan_rsp {
	struct mt7697_rsp_hdr rsp;
	__be32                if_idx;
} __attribute__((packed, aligned(4)));

struct mt7697_scan_ind {
	struct mt7697_rsp_hdr rsp;
	__be32                rssi;
	__be32                channel;
	u8                    probe_rsp[];
} __attribute__((packed, aligned(4)));

struct mt7697_scan_complete_ind {
	struct mt7697_rsp_hdr rsp;
	__be32                if_idx;
} __attribute__((packed, aligned(4)));

struct mt7697_set_pmk_req {
	struct mt7697_cmd_hdr cmd;
	__be32                port;
	u8                    pmk[LEN32_ALIGNED(MT7697_WIFI_LENGTH_PASSPHRASE)];
} __attribute__((packed, aligned(4)));

struct mt7697_set_channel_req {
	struct mt7697_cmd_hdr cmd;
	__be32                port;
	__be32                ch;
} __attribute__((packed, aligned(4)));

struct mt7697_set_bssid_req {
	struct mt7697_cmd_hdr cmd;
	u8                    bssid[LEN32_ALIGNED(ETH_ALEN)];
} __attribute__((packed, aligned(4)));

struct mt7697_set_ssid_req {
	struct mt7697_cmd_hdr cmd;
	__be32                port;
	__be32                len;
	u8                    ssid[LEN32_ALIGNED(IEEE80211_MAX_SSID_LEN)];
} __attribute__((packed, aligned(4)));

struct mt7697_set_security_mode_req {
	struct mt7697_cmd_hdr cmd;
	__be32                port;
	__be32                auth_mode;
	__be32                encrypt_type;
} __attribute__((packed, aligned(4)));

struct mt7697_get_security_mode_req {
	struct mt7697_cmd_hdr cmd;
	__be32                if_idx;
	__be32                port;
} __attribute__((packed, aligned(4)));

struct mt7697_get_security_mode_rsp {
	struct mt7697_rsp_hdr rsp;
	__be32                if_idx;
	__be32                auth_mode;
	__be32                encrypt_type;
} __attribute__((packed, aligned(4)));

struct mt7697_connect_ind {
	struct mt7697_rsp_hdr rsp;
	__be32                if_idx;
	__be32                channel;
	u8                    bssid[LEN32_ALIGNED(ETH_ALEN)];
} __attribute__((packed, aligned(4)));

struct mt7697_disconnect_req {
	struct mt7697_cmd_hdr cmd;
	__be32                port;
	u8                    addr[LEN32_ALIGNED(ETH_ALEN)];
} __attribute__((packed, aligned(4)));

struct mt7697_disconnect_ind {
	struct mt7697_rsp_hdr rsp;
	__be32                if_idx;
	u8                    bssid[LEN32_ALIGNED(ETH_ALEN)];
} __attribute__((packed, aligned(4)));

struct mt7697_tx_raw_packet {
	struct mt7697_cmd_hdr cmd;
	__be32                len;
	u8                    data[LEN32_ALIGNED(IEEE80211_MAX_FRAME_LEN)];
} __attribute__((packed, aligned(4)));

struct mt7697_rx_raw_packet {
	struct mt7697_rsp_hdr hdr;
	u8                    data[];
} __attribute__((packed, aligned(4)));

int mt7697_wr_set_wireless_mode_req(const struct mt7697_cfg80211_info*, u8);
int mt7697_wr_get_wireless_mode_req(const struct mt7697_cfg80211_info*);
int mt7697_wr_set_pmk_req(const struct mt7697_cfg80211_info*, const u8*);
int mt7697_wr_set_channel_req(const struct mt7697_cfg80211_info*, u8);
int mt7697_wr_set_bssid_req(const struct mt7697_cfg80211_info*, const u8[ETH_ALEN]);
int mt7697_wr_set_ssid_req(const struct mt7697_cfg80211_info*, u8, const u8[]);
int mt7697_wr_reload_settings_req(const struct mt7697_cfg80211_info*, u8);
int mt7697_wr_mac_addr_req(const struct mt7697_cfg80211_info*);
int mt7697_wr_cfg_req(const struct mt7697_cfg80211_info*);
int mt7697_wr_set_op_mode_req(const struct mt7697_cfg80211_info*);
int mt7697_wr_get_listen_interval_req(const struct mt7697_cfg80211_info*);
int mt7697_wr_set_listen_interval_req(const struct mt7697_cfg80211_info*, u32);
int mt7697_wr_scan_req(const struct mt7697_cfg80211_info*, u32,
	               const struct cfg80211_scan_request*);
int mt7697_wr_set_security_mode_req(const struct mt7697_cfg80211_info*, u8, u8);
int mt7697_wr_get_security_mode_req(const struct mt7697_cfg80211_info*, u32);
int mt7697_wr_scan_stop_req(const struct mt7697_cfg80211_info*);
int mt7697_wr_disconnect_req(const struct mt7697_cfg80211_info*, const u8*);
int mt7697_wr_tx_raw_packet(struct mt7697_cfg80211_info*, const u8*, u32);
int mt7697_proc_data(void*);

#endif
