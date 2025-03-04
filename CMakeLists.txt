idf_component_register(SRCS "spiffs-ota.c"
                    INCLUDE_DIRS .
                    REQUIRES esp_https_ota esp_partition
                    )
