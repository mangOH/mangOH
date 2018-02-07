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

#ifndef _MT7697_WIFI_API_H_
#define _MT7697_WIFI_API_H_

#include <linux/types.h>
#include <linux/ieee80211.h>

#define MT7697_WIFI_LENGTH_PASSPHRASE	64
#define MT7697_WIFI_LENGTH_PMK		32

#define MT7697_MAX_WOW_LISTEN_INTL	255

/**@brief Specifies 20MHz bandwidth in the 2.4GHz band.
*/
#define MT7697_WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_20MHZ       (0x00)

/**@brief Specifies 40MHz bandwidth in the 2.4GHz band.
*/
#define MT7697_WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_40MHZ       (0x01)

/**@brief Specifies 20MHz | 40MHz bandwidth in the 2.4GHz band.
*/
#define MT7697_WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_2040MHZ \
	(MT7697_WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_20MHZ | \
	 MT7697_WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_40MHZ)

/**
 * @brief Station operation mode. In this mode the device works as a Wi-Fi
 * client.
 */
#define MT7697_WIFI_MODE_STA_ONLY      (1)

/**
 * @brief SoftAP operation mode. In AP mode, other client devices can connect to
 * the Wi-Fi AP.
 */
#define MT7697_WIFI_MODE_AP_ONLY       (2)

/**
 * @brief Repeater mode. There are two virtual ports in repeater mode, one is
 * #WIFI_PORT_AP, and the other is #WIFI_PORT_APCLI. Both ports should be
 * configured to operate on the same channel with the same bandwidth, while
 * their other settings can be different. For example, both ports can have
 * different MAC addresses and security modes.
 */
#define MT7697_WIFI_MODE_REPEATER      (3)

/**
 * @brief This macro defines the monitoring mode. In this mode it can sniffer
 * the Wi-Fi packet in the air given the specific channel and bandwidth. It is
 * used to enter a hybrid mode and handle a raw packet. Call
 * #wifi_config_register_rx_handler() to register a raw packet handler once this
 *  mode is set.
 */
#define MT7697_WIFI_MODE_MONITOR       (4)

enum mt7697_port_type {
	MT7697_PORT_STA = 0,
	MT7697_PORT_APCLI = MT7697_PORT_STA,
	MT7697_PORT_AP,
};

enum mt7697_scan_mode {
	MT7697_WIFI_SCAN_MODE_FULL = 0,
	MT7697_WIFI_SCAN_MODE_PARTIAL,
};

enum mt7697_scan_option {
	MT7697_WIFI_SCAN_OPTION_ACTIVE = 0,
	MT7697_WIFI_SCAN_OPTION_PASSIVE,
	MT7697_WIFI_SCAN_OPTION_FORCE_ACTIVE,
};

/**
 * @brief This enumeration defines the wireless authentication mode to indicate
 * the Wi-Fi device’s authentication attribute.
 */
enum mt7697_wifi_auth_mode_t {
	MT7697_WIFI_AUTH_MODE_OPEN = 0,         /**< Open mode.     */
	MT7697_WIFI_AUTH_MODE_SHARED,           /**< Not supported. */
	MT7697_WIFI_AUTH_MODE_AUTO_WEP,         /**< Not supported. */
	MT7697_WIFI_AUTH_MODE_WPA,              /**< Not supported. */
	MT7697_WIFI_AUTH_MODE_WPA_PSK,          /**< WPA_PSK.       */
	MT7697_WIFI_AUTH_MODE_WPA_None,         /**< Not supported. */
	MT7697_WIFI_AUTH_MODE_WPA2,             /**< Not supported. */
	MT7697_WIFI_AUTH_MODE_WPA2_PSK,         /**< WPA2_PSK.      */
	MT7697_WIFI_AUTH_MODE_WPA_WPA2,         /**< Not supported. */
	MT7697_WIFI_AUTH_MODE_WPA_PSK_WPA2_PSK, /**< Mixture mode.  */
};

/**
 * @brief This enumeration defines the wireless encryption type to indicate the
 * Wi-Fi device’s encryption attribute.
 */
enum mt7697_wifi_encrypt_type_t {
	MT7697_WIFI_ENCRYPT_TYPE_WEP_ENABLED           = 0,                                          /**< WEP encryption type. */
	MT7697_WIFI_ENCRYPT_TYPE_ENCRYPT1_ENABLED      = MT7697_WIFI_ENCRYPT_TYPE_WEP_ENABLED,       /**< WEP encryption type. */
	MT7697_WIFI_ENCRYPT_TYPE_WEP_DISABLED          = 1,                                          /**< No encryption.       */
	MT7697_WIFI_ENCRYPT_TYPE_ENCRYPT_DISABLED      = MT7697_WIFI_ENCRYPT_TYPE_WEP_DISABLED,      /**< No encryption.       */
	MT7697_WIFI_ENCRYPT_TYPE_WEP_KEY_ABSENT        = 2,                                          /**< Not supported.       */
	MT7697_WIFI_ENCRYPT_TYPE_ENCRYPT_KEY_ABSENT    = MT7697_WIFI_ENCRYPT_TYPE_WEP_KEY_ABSENT,    /**< Not supported.       */
	MT7697_WIFI_ENCRYPT_TYPE_WEP_NOT_SUPPORTED     = 3,                                          /**< Not supported.       */
	MT7697_WIFI_ENCRYPT_TYPE_ENCRYPT_NOT_SUPPORTED = MT7697_WIFI_ENCRYPT_TYPE_WEP_NOT_SUPPORTED, /**< Not supported.       */
	MT7697_WIFI_ENCRYPT_TYPE_TKIP_ENABLED          = 4,                                          /**< TKIP encryption.     */
	MT7697_WIFI_ENCRYPT_TYPE_ENCRYPT2_ENABLED      = MT7697_WIFI_ENCRYPT_TYPE_TKIP_ENABLED,      /**< TKIP encryption.     */
	MT7697_WIFI_ENCRYPT_TYPE_AES_ENABLED           = 6,                                          /**< AES encryption.      */
	MT7697_WIFI_ENCRYPT_TYPE_ENCRYPT3_ENABLED      = MT7697_WIFI_ENCRYPT_TYPE_AES_ENABLED,       /**< AES encryption.      */
	MT7697_WIFI_ENCRYPT_TYPE_AES_KEY_ABSENT        = 7,                                          /**< Not supported.       */
	MT7697_WIFI_ENCRYPT_TYPE_TKIP_AES_MIX          = 8,                                          /**< TKIP or AES mix.     */
	MT7697_WIFI_ENCRYPT_TYPE_ENCRYPT4_ENABLED      = MT7697_WIFI_ENCRYPT_TYPE_TKIP_AES_MIX,      /**< TKIP or AES mix.     */
	MT7697_WIFI_ENCRYPT_TYPE_TKIP_AES_KEY_ABSENT   = 9,                                          /**< Not supported.       */
	MT7697_WIFI_ENCRYPT_TYPE_GROUP_WEP40_ENABLED   = 10,                                         /**< Not supported.       */
	MT7697_WIFI_ENCRYPT_TYPE_GROUP_WEP104_ENABLED  = 11,                                         /**< Not supported.       */
#ifdef WAPI_SUPPORT
	MT7697_WIFI_ENCRYPT_TYPE_ENCRYPT_SMS4_ENABLED,                                               /**< WPI SMS4 support.    */
#endif /* WAPI_SUPPORT */
};

/** @brief This enumeration defines the wireless physical mode.
*/
enum mt7697_wifi_phy_mode_t {
	MT7697_WIFI_PHY_11BG_MIXED = 0,     /**<  0, 2.4GHz band. */
	MT7697_WIFI_PHY_11B,                /**<  1, 2.4GHz band. */
	MT7697_WIFI_PHY_11A,                /**<  2, 5GHz band. */
	MT7697_WIFI_PHY_11ABG_MIXED,        /**<  3, both 2.4G and 5G band. */
	MT7697_WIFI_PHY_11G,                /**<  4, 2.4GHz band. */
	MT7697_WIFI_PHY_11ABGN_MIXED,       /**<  5, both 2.4G and 5G band. */
	MT7697_WIFI_PHY_11N_2_4G,           /**<  6, 11n-only with 2.4GHz band. */
	MT7697_WIFI_PHY_11GN_MIXED,         /**<  7, 2.4GHz band. */
	MT7697_WIFI_PHY_11AN_MIXED,         /**<  8, 5GHz band. */
	MT7697_WIFI_PHY_11BGN_MIXED,        /**<  9, 2.4GHz band. */
	MT7697_WIFI_PHY_11AGN_MIXED,        /**< 10, both 2.4G and 5G band. */
	MT7697_WIFI_PHY_11N_5G,             /**< 11, 11n-only with 5GHz band. */
};

/**
 * @brief This enumeration defines the RX filter register's bitmap. Each bit
 * indicates a specific drop frame.
 */
enum mt7697_wifi_rx_filter_t {
    	MT7697_WIFI_RX_FILTER_DROP_STBC_BCN_BC_MC,     /**< bit 0   Drops the STBC beacon/BC/MC frames. */
    	MT7697_WIFI_RX_FILTER_DROP_FCS_ERR,            /**< bit 1   Drops the FCS error frames. */
    	MT7697_WIFI_RX_FILTER_RESERVED,                /**< bit 2   A reserved bit, not used. */
    	MT7697_WIFI_RX_FILTER_DROP_VER_NOT_0,          /**< bit 3   Drops the version field of Frame Control field. It cannot be 0. */
    	MT7697_WIFI_RX_FILTER_DROP_PROBE_REQ,          /**< bit 4   Drops the probe request frame. */
    	MT7697_WIFI_RX_FILTER_DROP_MC_FRAME,           /**< bit 5   Drops multicast frame. */
    	MT7697_WIFI_RX_FILTER_DROP_BC_FRAME,           /**< bit 6   Drops broadcast frame. */
    	MT7697_WIFI_RX_FILTER_RM_FRAME_REPORT_EN = 12, /**< bit 12  Enables report frames. */
    	MT7697_WIFI_RX_FILTER_DROP_CTRL_RSV,           /**< bit 13  Drops reserved definition control frames. */
    	MT7697_WIFI_RX_FILTER_DROP_CTS,                /**< bit 14  Drops CTS frames. */
    	MT7697_WIFI_RX_FILTER_DROP_RTS,                /**< bit 15  Drops RTS frames. */
    	MT7697_WIFI_RX_FILTER_DROP_DUPLICATE,          /**< bit 16  Drops duplicate frames. */
    	MT7697_WIFI_RX_FILTER_DROP_NOT_MY_BSSID,       /**< bit 17  Drops not my BSSID frames. */
    	MT7697_WIFI_RX_FILTER_DROP_NOT_UC2ME,          /**< bit 18  Drops not unicast to me frames. */
    	MT7697_WIFI_RX_FILTER_DROP_DIFF_BSSID_BTIM,    /**< bit 19  Drops different BSSID TIM (Traffic Indication Map) Broadcast frame. */
    	MT7697_WIFI_RX_FILTER_DROP_NDPA                /**< bit 20  Drops the NDPA or not. */
};

/**
 * @brief This structure is the Wi-Fi configuration for initialization in STA
 * mode.
 */
struct mt7697_wifi_sta_config_t {
	u8 ssid[IEEE80211_MAX_SSID_LEN];      /**< The SSID of the target AP. */
	u8 ssid_len;                          /**< The length of the SSID. */
	u8 bssid_present;                     /**< The BSSID is present if it is set to 1. Otherwise, it is set to 0. */
	u8 bssid[ETH_ALEN];                   /**< The MAC address of the target AP. */
	u8 pw[MT7697_WIFI_LENGTH_PASSPHRASE]; /**< The password of the target AP. */
	u8 pw_len;                            /**< The length of the password. */
} __attribute__((__packed__));

/** @brief This structure is the Wi-Fi configuration for initialization in AP mode.
*/
struct mt7697_wifi_ap_config_t {
	u8 ssid[IEEE80211_MAX_SSID_LEN];      /**< The SSID of the AP. */
	u8 ssid_len;                          /**< The length of the SSID. */
	u8 pw[MT7697_WIFI_LENGTH_PASSPHRASE]; /**< The password of the AP. */
	u8 pw_len;                            /**< The length of the password. */
	u8 auth_mode;                         /**< The authentication mode. */
	u8 encrypt_type;                      /**< The encryption mode. */
	u8 ch;                                /**< The channel. */
	u8 bandwidth;                         /**< The bandwidth that is either #WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_20MHZ or #WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_40MHZ. */
	u8 bandwidth_ext;                     /**< The bandwidth extension. It is only applicable when the bandwidth is set to #WIFI_IOT_COMMAND_CONFIG_BANDWIDTH_40MHZ. */
} __attribute__((__packed__));

/** @brief Wi-Fi configuration for initialization.
*/
struct mt7697_wifi_config_t {
	u8 opmode;                           /**< The operation mode. The value should be #WIFI_MODE_STA_ONLY, #WIFI_MODE_AP_ONLY, #WIFI_MODE_REPEATER or #WIFI_MODE_MONITOR*/
	struct mt7697_wifi_sta_config_t sta; /**< The configurations for the STA. It should be set when the OPMODE is #WIFI_MODE_STA_ONLY or #WIFI_MODE_REPEATER. */
	struct mt7697_wifi_ap_config_t ap;   /**< The configurations for the AP. It should be set when the OPMODE is #WIFI_MODE_AP_ONLY or #WIFI_MODE_REPEATER. */
} __attribute__((__packed__));

#endif
