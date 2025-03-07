//
// Created by Melanie on 02/03/2025.
//
#include "esp_err.h"

#ifndef LIGHTSWITCH_SPIFFS_OTA_H
#define LIGHTSWITCH_SPIFFS_OTA_H

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ota_spiffs(const char *serverUrl);

#ifdef __cplusplus
}
#endif

#endif //LIGHTSWITCH_SPIFFS_OTA_H
