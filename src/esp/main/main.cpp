
#include "io.h"
#include "led.h"
#include "button.h"
#include "wifi.h"
#include "http.h"
#include "ota.h"
#include "settings.h"
#include "event.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp32/spiram.h"
#include "esp_ota_ops.h"

#include "trs-io.h"
#include "trs-fs.h"
#include "spi.h"
#include "keyb.h"
#include "ptrs.h"
#include "xflash.h"
#include "spiffs.h"

#include "esp_idf_version.h"

#include "ssd1306.h"

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(4, 4, 7)
#error Please use ESP-IDF version 4.4.7
#endif

#define TAG "TRS-IO"

#ifdef CONFIG_TRS_IO_MODEL_1
#define CONFIG "TRS-IO (Model 1)"
#endif
#ifdef CONFIG_TRS_IO_MODEL_3
#define CONFIG "TRS-IO (Model III)"
#endif
#ifdef CONFIG_TRS_IO_PP
#define CONFIG "TRS-IO++"
#endif

extern const char* GIT_REV;
extern const char* GIT_BRANCH;


static void check()
{
#ifdef CONFIG_TRS_IO_PP
  size_t flash_size = spi_flash_get_chip_size() / (1024 * 1024);
  size_t psram_size = esp_spiram_get_size() / (1024 * 1024);

  if (flash_size == 16) {
    ESP_LOGI(TAG, "Found 16MB flash size");
  } else {
    ESP_LOGE(TAG, "Flash size needs to be configured to 16MB. Found %dMB", flash_size);
  }
  if (psram_size == 8) {
    ESP_LOGI(TAG, "Found 8MB of PSRAM");
  } else {
    ESP_LOGE(TAG, "Need 8MB of PSRAM. Found %dMB", psram_size);
  }

  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
      if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        // Assume everything is peachy just because we made it here
        ESP_LOGI(TAG, "Marking OTA update as valid");
        esp_ota_mark_app_valid_cancel_rollback();
      }
  }
  ESP_LOGI(TAG, "Running partition: %s", running->label);
#endif
}


extern "C" {

int custom_log_vprintf(const char* fmt, va_list args) {
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char time_str[64];
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

  printf("[%s] ", time_str); // prepend wall-clock time
  return vprintf(fmt, args); // print the actual log message
}




void app_main(void)
{
  esp_log_set_vprintf(custom_log_vprintf);
  ESP_LOGI(TAG, "TRS-IO branch=%s, rev=%s", GIT_BRANCH, GIT_REV);
  ESP_LOGI(TAG, "Configured for %s", CONFIG);

  // Silence "spi_master: device5 locked the bus" debug messages when
  // accessing the SD card.
  esp_log_level_set("spi_master", ESP_LOG_INFO);

  check();

  init_led();  // setup LED or SSD1306
  #ifdef CONFIG_TRS_IO_SSD1306
    draw_text("Booting: Events  ", 0, 3); 
  #endif 
  init_events();        // Create the FreeRTOS EventGroup used for inter-module signaling and to track/send ESP status bits.
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("Booting: TRS-IO  ", 0, 3); 
  #endif 
  init_trs_io();        // Initialize the TrsIO subsystem: call TrsIO::init() to initialize registered modules and reset I/O state.
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("Booting: Button  ", 0, 3); 
  #endif 
  init_button();        // Configure the status (and reset on PP) GPIO pins and install ISR handlers to detect short/long button presses.
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("Booting: Settings", 0, 3); 
  #endif
  init_settings();      // Initialize NVS (non-volatile storage), open the settings namespace and initialize stored settings (tz, WiFi/SMB creds, ROM, screen/printer/keyboard and update flags).
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("Booting: SPI     ", 0, 3); 
  #endif 
  init_spi();           // Initialize the SPI bus, create a mutex for SPI transactions and register the FPGA/Cmod SPI device with its configuration.
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("Booting: TRS FS  ", 0, 3); 
  #endif 
  init_trs_fs_posix();  // Create/attach the POSIX-backed TRS file system (SD card), set it as the active TRS FS, and return any error string.
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("Booting: SPIFFS  ", 0, 3); 
  #endif 
  init_spiffs();        // Mount the SPIFFS partitions used for web/html (and other PP partitions), register a custom stat() adapter and initialize ROMs if configured.
  #ifdef CONFIG_TRS_IO_PP
    init_keyb();
  #endif
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("Booting: FPGA    ", 0, 3); 
  #endif 
  init_fpga();          // Wait for and verify the FPGA, upload/refresh FPGA firmware if needed (PP special handling), set FPGA-related SPI settings (screen color/printer) and clear ESP status.
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("Booting: WiFi    ", 0, 3); 
  #endif 
  init_wifi();          // Initialize esp-netif/event loop, register WiFi/IP event handlers, configure STA or AP mode based on stored credentials, and start WiFi.
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("Booting: IO      ", 0, 3); 
  #endif
   init_io();            // Configure TRS/ESP GPIO lines (addr/data/select/ESP_REQ/ESP_DONE etc.), install related ISRs and start the io and action FreeRTOS tasks.
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("Booting: HTTP    ", 0, 3); 
  #endif 
  init_http();          // Start the Mongoose web server task (mg_task), initialize mdns/time and begin listening for HTTP requests.
  #ifdef CONFIG_TRS_IO_SSD1306 
    draw_text("                 ", 0, 3); 
  #endif                       
  vTaskDelete(NULL);
}

} // extern "C"
