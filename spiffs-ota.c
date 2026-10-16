#include <esp_partition.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include <freertos/task.h>
#include <esp_http_client.h>
#include "esp_err.h"
#include "spiffs-ota.h"

#define TAG "spiffs_ota"

/**

@brief Task to update spiffs partition
@param pvParameter
*/

#define BUFFSIZE (8192)
// Give up when the download makes no progress for this long
#define STALL_TIMEOUT_MS (60000)

static char ota_write_data[BUFFSIZE];

esp_err_t ota_spiffs(const char *serverUrl)
{
    printf("Starting update of SPIFFS data via OTA\r\n");

    esp_err_t err;
    // update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
//    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;
    esp_partition_t *spiffs_partition = NULL;
    ESP_LOGI(TAG, "Starting OTA SPIFFS...");

//    const esp_partition_t *configured = esp_ota_get_boot_partition();
//    const esp_partition_t *running = esp_ota_get_running_partition();

    /*Update SPIFFS : 1/ First we need to find SPIFFS partition  */

    esp_partition_iterator_t spiffs_partition_iterator = esp_partition_find(ESP_PARTITION_TYPE_DATA,
                                                                            ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    while (spiffs_partition_iterator != NULL)
    {
        spiffs_partition = (esp_partition_t *) esp_partition_get(spiffs_partition_iterator);
        printf("main: partition type = %d.\n", spiffs_partition->type);
        printf("main: partition subtype = %d.\n", spiffs_partition->subtype);
        printf("main: partition starting address = %x.\n", spiffs_partition->address);
        printf("main: partition size = %x.\n", spiffs_partition->size);
        printf("main: partition label = %s.\n", spiffs_partition->label);
        printf("main: partition subtype = %d.\n", spiffs_partition->encrypted);
        printf("\n");
        printf("\n");
        spiffs_partition_iterator = esp_partition_next(spiffs_partition_iterator);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_partition_iterator_release(spiffs_partition_iterator);

    if (spiffs_partition == NULL)
    {
        ESP_LOGE(TAG, "No SPIFFS partition found");
        return ESP_ERR_NOT_FOUND;
    }

    esp_http_client_config_t config = {
        .url = serverUrl,
        // TCP keepalive, so a dead server is noticed as an error rather than endless timeouts
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        return ESP_ERR_HTTP_CONNECT;
    }
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_CONNECT;
    }
    int64_t content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);

    // Check before erasing, or a failed request wipes the current web interface
    if (status != 200 || content_length > (int64_t) spiffs_partition->size)
    {
        ESP_LOGE(TAG, "Not updating, status %d, length %lld, partition size %lu", status, content_length,
                 (unsigned long) spiffs_partition->size);
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_FETCH_HEADER;
    }

    printf("Deleting old contents, %lx (%lx)\r\n", spiffs_partition->address, spiffs_partition->size);

    /* 2: Delete SPIFFS Partition  */
    esp_partition_erase_range(spiffs_partition, 0, spiffs_partition->size);


    int binary_file_length = 0;
    /*deal with all receive packet*/
    int offset = 0;

    printf("Downloading and writing new image\r\n");

    TickType_t last_progress = xTaskGetTickCount();
    while (1)
    {
        int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        if (data_read == -ESP_ERR_HTTP_EAGAIN)
        {
            // Read timeout: a slow link is not an error until it stalls
            if ((xTaskGetTickCount() - last_progress) * portTICK_PERIOD_MS > STALL_TIMEOUT_MS)
            {
                ESP_LOGE(TAG, "Download stalled at %d bytes", binary_file_length);
                esp_http_client_cleanup(client);
                return ESP_ERR_TIMEOUT;
            }
            continue;
        }
        if (data_read < 0)
        {
            ESP_LOGE(TAG, "Error: SSL data read error");
            esp_http_client_cleanup(client);
            return ESP_ERR_HTTP_INVALID_TRANSPORT;
        }
        else if (data_read > 0)
        {
            /* 3 : WRITE SPIFFS PARTITION */
            err = esp_partition_write(spiffs_partition, offset, (const void *) ota_write_data, data_read);

            if (err != ESP_OK)
            {
                esp_http_client_cleanup(client);
                return ESP_ERR_HTTP_WRITE_DATA;
            }

            offset += data_read;
            binary_file_length += data_read;
            last_progress = xTaskGetTickCount();
            ESP_LOGD(TAG, "Written image length %d", binary_file_length);
        }
        else
        {
            ESP_LOGI(TAG, "Connection closed,all data received");
            break;
        }
    }

    ESP_LOGI(TAG, "Total Write binary data length : %d", binary_file_length);

    bool complete = esp_http_client_is_complete_data_received(client);
    esp_http_client_cleanup(client); // Was leaked on success

    if (!complete)
    {
        ESP_LOGE(TAG, "Download incomplete, %d bytes", binary_file_length);
        return ESP_FAIL;
    }

    return ESP_OK;
}
