#include "CombainRequestBuilder.h"
#include <jansson.h>
#include <sstream>
#include <iomanip>

static std::string macAddrToString(const uint8_t *mac);
static std::string ssidToString(const uint8_t *ssid, size_t ssidLen);
static std::string cellularTechnologyToString(ma_combainLocation_CellularTech_t cellTech);

WifiApScanItem::WifiApScanItem(
    const uint8_t *bssid,
    size_t bssidLen,
    const uint8_t *ssid,
    size_t ssidLen,
    int16_t signalStrength)
{
    if (bssidLen != 6)
    {
        throw std::runtime_error("BSSID length must be 6");
    }
    std::copy(&bssid[0], &bssid[6], &this->bssid[0]);

    if (ssidLen > sizeof(this->ssid))
    {
        throw std::runtime_error("SSID is too long");
    }
    std::copy(&ssid[0], &ssid[ssidLen], &this->ssid[0]);
    this->ssidLen = ssidLen;

    if (signalStrength >= 0)
    {
        throw std::runtime_error("Signal strength should be negative");
    }
    this->signalStrength = signalStrength;
}

void CombainRequestBuilder::appendWifiAccessPoint(const WifiApScanItem& ap)
{
    this->wifiAps.push_back(ap);
}

void CombainRequestBuilder::appendCellTower(const CellTowerScanItem& tower)
{
    this->cellTowers.push_back(tower);
}

std::string CombainRequestBuilder::generateRequestBody(void) const
{
    json_t *body = json_object();
    if (!this->wifiAps.empty())
    {
        json_t *wifiApsArray = json_array();
        for (auto const& ap : this->wifiAps)
        {
            json_t *jsonAp = json_object();
            json_object_set_new(jsonAp, "macAddress", json_string(macAddrToString(ap.bssid).c_str()));
            json_object_set_new(jsonAp, "ssid", json_string(ssidToString(ap.ssid, ap.ssidLen).c_str()));
            json_object_set_new(jsonAp, "signalStrength", json_integer(ap.signalStrength));
            json_array_append_new(wifiApsArray, jsonAp);
        }
        json_object_set_new(body, "wifiAccessPoints", wifiApsArray);
    }

    if (!this->cellTowers.empty())
    {
        json_t *cellTowersArray = json_array();
        for (auto const& tower: this->cellTowers)
        {
            json_t *jsonTower = json_object();
            json_object_set_new(jsonTower, "radioType", json_string(cellularTechnologyToString(tower.cellularTechnology).c_str()));
            json_object_set_new(jsonTower, "mobileCountryCode", json_integer(tower.mcc));
            json_object_set_new(jsonTower, "mobileNetworkCode", json_integer(tower.mnc));
            json_object_set_new(jsonTower, "locationAccessCode", json_integer(tower.lac));
            json_object_set_new(jsonTower, "cellId", json_integer(tower.cellId));
        }
        json_object_set_new(body, "cellTowers", cellTowersArray);
    }

    char* s = json_dumps(body, JSON_COMPACT);
    LE_ASSERT(s != NULL);
    std::string res(s);
    free(s);
    json_decref(body);
    return res;
}


//----------------- STATIC
static std::string macAddrToString(const uint8_t *mac)
{
    std::ostringstream s;
    for (auto i = 0; i < 6; i++)
    {
        s << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned int>(mac[i]);
        if (i != 5)
        {
            s << ':';
        }
    }
    return s.str();
}

static std::string ssidToString(const uint8_t *ssid, size_t ssidLen)
{
    std::string s(reinterpret_cast<const char *>(ssid), ssidLen);
    return s;
}

static std::string cellularTechnologyToString(ma_combainLocation_CellularTech_t cellTech)
{
    switch (cellTech)
    {
    case MA_COMBAINLOCATION_CELL_TECH_GSM:
        return "gsm";
        break;

    case MA_COMBAINLOCATION_CELL_TECH_CDMA:
        return "cdma";
        break;

    case MA_COMBAINLOCATION_CELL_TECH_LTE:
        return "lte";
        break;

    case MA_COMBAINLOCATION_CELL_TECH_WCDMA:
        return "wcdma";
        break;

    default:
        throw std::runtime_error("Invalid cellular technology");
        break;
    }

    return "";
}
