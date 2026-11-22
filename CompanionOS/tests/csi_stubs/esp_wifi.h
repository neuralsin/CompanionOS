#pragma once
using esp_err_t = int;
constexpr int ESP_OK = 0;
struct wifi_csi_config_t {
  bool lltf_en = false, htltf_en = false, stbc_htltf2_en = false, ltf_merge_en = false;
};
struct wifi_csi_info_t {
  int8_t* buf;
  unsigned len;
  struct { int8_t rssi; uint8_t channel; int8_t noise_floor; } rx_ctrl;
};
using CsiCallback = void (*)(void*, wifi_csi_info_t*);
static CsiCallback callback = nullptr;
static int configError = 0, callbackError = 0, enableError = 0;
static bool csiEnabled = false;
inline int esp_wifi_set_csi_config(wifi_csi_config_t*) { return configError; }
inline int esp_wifi_set_csi_rx_cb(CsiCallback fn, void*) {
  if (fn && callbackError) return callbackError;
  callback = fn; return 0;
}
inline int esp_wifi_set_csi(bool enable) {
  if (enable && enableError) return enableError;
  csiEnabled = enable; return 0;
}
