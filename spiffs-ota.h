//
// Created by Melanie on 02/03/2025.
//
#include "esp_err.h"

#ifndef LIGHTSWITCH_SPIFFS_OTA_H
#define LIGHTSWITCH_SPIFFS_OTA_H

#ifdef __cplusplus
extern "C" {
#endif

// Downloads CONFIG_SPIFFS_OTA_URI/<label>.bin into the SPIFFS partition <label>.
// NULL selects the first SPIFFS partition and spiffs.bin.
esp_err_t ota_spiffs(const char *label);
// Same, from the given URL
esp_err_t ota_spiffs_url(const char *url, const char *label);

#ifdef __cplusplus
}
#endif

#endif //LIGHTSWITCH_SPIFFS_OTA_H
