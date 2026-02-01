#include "data-fetcher-esp.h"

#include <string>
#include <esp_log.h>

#include "common.h"
#include "esp_http_client.h"

namespace {
static const char *TAG = "DataFetcherEsp";

RsData incoming_buffer(0);
int received = 0;
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            // Only knows how to handle non-chunked data.
            if (esp_http_client_is_chunked_response(evt->client)) {
              ESP_LOGE(TAG, "Data is chunked!");
              return ESP_FAIL;
            }

            // Allocate the memory if not already done.
            if (incoming_buffer.len == 0) {
                incoming_buffer.len = esp_http_client_get_content_length(evt->client);
                incoming_buffer.data = (uint8_t *) malloc(incoming_buffer.len);
                ESP_LOGI(TAG, "Preparing buffer for %d bytes.", incoming_buffer.len);
                received = 0;
                if (incoming_buffer.data == NULL) {
                    incoming_buffer.len = 0;
                    ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                    return ESP_FAIL;
                }
            }
            ESP_LOGI(TAG, "Received %d of a total %d bytes.", evt->data_len, incoming_buffer.len);
            memcpy(incoming_buffer.data + received, evt->data, evt->data_len);
            received += evt->data_len;

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}
};  // namespace

namespace retrostore {

bool DataFetcherEsp::Fetch(const std::string& path,
                           const RsData& params,
                           RsData* data) const {
  if (data->len > 0 || data->data != NULL) {
    ESP_LOGE(TAG, "Given `data` should be empty!");
    return false;
  }

  esp_http_client_config_t config = {
    .host = host_.c_str(),
    .path = path.c_str(),
    .event_handler = _http_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
  esp_http_client_set_post_field(client, (const char*) params.data, params.len);

  bool success = true;
  // Make GET request.
  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK) {
      ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
              esp_http_client_get_status_code(client),
              esp_http_client_get_content_length(client));
      success = esp_http_client_get_status_code(client) == 200;
  } else {
      ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
      success = false;
  }

  // Transfer data and ownership.
  data->data = incoming_buffer.data;
  data->len = incoming_buffer.len;
  incoming_buffer.data = NULL;
  incoming_buffer.len = 0;

  esp_http_client_cleanup(client);
  return success;
}

}  // namespace retrostore
