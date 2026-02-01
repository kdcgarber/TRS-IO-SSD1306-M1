#include "retrostore.h"
#include "wifi.h"
#include "spi.h"
#include "printer.h"
#include "ntp_sync.h"
#include "trs-fs.h"
#ifdef CONFIG_TRS_IO_PP
#include "fs-roms.h"
#include "fs-spiffs.h"
#endif
#include "smb.h"
#include "ota.h"
#include "led.h"
#include "io.h"
#include "settings.h"
#include "event.h"
#include "ntp_sync.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "esp_mock.h"
#include "version.h"
#include "mdns.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mongoose.h"
#include "web_debugger.h"
#include <string.h>
#include "cJSON.h"

#include "ssd1306.h"
#include <limits.h>

#define SSID "TRS-IO"
#define TAG "WIFI"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "trs-fs.h"
#include <cstdio>

#include "ssd1306.h"


static TaskHandle_t s_rssi_task_handle = NULL;

static uint8_t status = RS_STATUS_WIFI_CONNECTING;

char currentFreeHeap[32];


uint8_t* get_wifi_status()
{
  return &status;
}


static char ip[16] = {'-', '\0'};

const char* get_wifi_ip()
{
  return ip;
}


//----------------------------------------------------------------------------------------------------------
#ifdef CONFIG_TRS_IO_SSD1306  
// Update the SSD1306 with current IP and RSSI bars/numeric value.
// And free heap and current date/time
  void update_rssi_display(void)
  {
    
    // ---- Display IP on line 0 --------
    char display_text[16];
    //snprintf(display_text, sizeof(display_text), "%-15.15s", ip);
    snprintf(display_text, sizeof(display_text), "%-*.*s", (int)(sizeof(display_text) - 1), (int)(sizeof(display_text) - 1), ip);
    //snprintf(display_text, sizeof(display_text), "%-15.15s", ip);
    draw_text(display_text, 0, 0);
    // -------------------------------------------------------------


    //----  Show wifi as a 5-bar graph and numeric dBm on line 1 --------
    int rssi = INT_MIN;
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
      rssi = ap_info.rssi;
    }

    if (rssi != INT_MIN) {
      const int min_dbm = -100;
      const int max_dbm = -30;
      int bars = 0;
      if (rssi <= min_dbm) bars = 0;
      else if (rssi >= max_dbm) bars = 5;
      else bars = (rssi - min_dbm) * 5 / (max_dbm - min_dbm);

      char bars_str[16];
      int p = 0;
      bars_str[p++] = '[';
      for (int i = 0; i < 5; i++) {
        bars_str[p++] = (i < bars) ? (unsigned char)130 : ' ';
      }
      bars_str[p++] = ']';
      bars_str[p] = '\0';

      char net_strength[10];
      sprintf(net_strength, "%3d dBm", rssi);
      //sprintf(net_strength, "%-8.8s %ddBm   ", bars_str,rssi);
      draw_text(bars_str, (0*6), 1);  // Display bars at column 17 * 5 pixels per char
      draw_text(net_strength, (8*6), 1);

    } else {
      draw_text("[     ]", (0*6), 1);  // Display no bars at column 17 * 5 pixels per char
    }
      // -------------------------------------------------------------


    // ----Heap display for debugging purposes  on line 2-----------
    char temp[8];
    snprintf(temp, sizeof(temp), "%u", esp_get_free_heap_size());
    snprintf(currentFreeHeap, sizeof(currentFreeHeap), "%s", temp);
    draw_text(currentFreeHeap, (16*6),3); //Display free heap on OLED  125 is the length in pixels - (5 pixels per char * 8 chars)
    // -------------------------------------------------------------


    // ------------ Date/Time display on line 3---------------------
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char date_time[32];
    strftime(date_time, sizeof(date_time), "%H:%M", &timeinfo);
    (now > 100000) ? draw_text(date_time, (0*6), 3): (void)0; // display date/time if time is valid at  column 19 * 5 pixels per char
    strftime(date_time, sizeof(date_time), "%m/%d/%y", &timeinfo);
    (now > 100000) ? draw_text(date_time, (6*6), 3): (void)0; // display date/time if time is valid at  column 14 * 5 pixels per char
    // -------------------------------------------------------------


    //---------   Show curent status   -----------------------------
    char wifiStat[128];
    char smbStat[128];
    char sdStat[128];
    char chbuf[2];

    chbuf[1] = '\0';  

    // WIFI
    uint8_t* wifi_ptr = get_wifi_status();
    int wifi_status = wifi_ptr ? *wifi_ptr : -1;
    bool wifiConnected = (wifi_status == RS_STATUS_WIFI_CONNECTED);
    const char* ip = get_wifi_ip();
    std::string wifiMsg = wifiConnected ? (ip ? ip : "Connected (no IP)") : "Not connected";
    sprintf(wifiStat,"WiFi: %s (status=%d) - %s\n", wifiConnected ? "Connected" : "Not connected", wifi_status, wifiMsg.c_str());
    //ESP_LOGI(TAG, "%s", wifiStat);
    chbuf[0] = (char)135; // ASCII code 135 which is the custom wifi connected icon
    wifiConnected ? draw_text(chbuf, (16*6), 0): draw_text(" ",(16*6) , 0);
    


      // SMB
    const char* smb_err = get_smb_err_msg(); // returns NULL if no error
    bool smbConnected = (smb_err == NULL || smb_err[0] == '\0');
    const char* smbMsg = smb_err ? smb_err : "Connected";
    sprintf(smbStat,"SMB: %s - %s\n", smbConnected ? "Connected" : "Disconnected", smbMsg);
    //ESP_LOGI(TAG, "%s", smbStat);
    chbuf[0] = (char)136; // ASCII code 135 which is the custom smg connected icon
    smbConnected ? draw_text(chbuf, (17*6), 0): draw_text(" ", (17*6), 0);
    


    // SD card
    bool has_sd = trs_fs_has_sd_card_reader();
    const char* posix_err = get_posix_err_msg(); // NULL if OK
    bool posix_ok = (posix_err == NULL || posix_err[0] == '\0');
    bool sdCardMounted = has_sd && posix_ok;
    const char* sdMsg = posix_err ? posix_err : (has_sd ? "Mounted" : "No SD reader");
    sprintf(sdStat,"SD: %s - %s\n", sdCardMounted ? "Mounted" : "Not mounted", sdMsg);
    //ESP_LOGI(TAG, "%s", sdStat);
    chbuf[0] = (char)137; // ASCII code 135 which is the custom sd connected icon
    sdCardMounted ? draw_text(chbuf, (18*6), 0): draw_text(" ", (18*6), 0);


    //-------------------------------------------------------------------
    // TRIS-IO version
    char target[40];
    uint8_t fpga_version = spi_get_fpga_version();
    #ifdef CONFIG_TRS_IO_MODEL_1
    strcpy(target, "TRS-IO M1");
    #endif
    #ifdef CONFIG_TRS_IO_MODEL_3
    strcpy(target, "TRS-IO M3");
    #endif
    #ifdef CONFIG_TRS_IO_PP
    strcpy(target, "TRS-IO++");
    #endif
    char temp2[40];
    sprintf(temp2,"%9.9s V:%1.1d.%1.1d F:%1d.%1d", target, TRS_IO_VERSION_MAJOR,TRS_IO_VERSION_MINOR,fpga_version >> 4,fpga_version & 0x0F);
    draw_text(temp2, (0*6), 2 );        // Display major version at column 9 * 5 pixels per char
    // -------------------------------------------------------------

  }


  static void rssi_monitor_task(void* arg)
  {
      while (1) {
          update_rssi_display();
          vTaskDelay(pdMS_TO_TICKS(5000));
      }
  }

  void start_rssi_monitor(void)
  {
      if (s_rssi_task_handle == NULL) {
          xTaskCreate(&rssi_monitor_task, "rssi_mon", 4096, NULL, 5, &s_rssi_task_handle);
      }
  }

  void stop_rssi_monitor(void)
  {
      if (s_rssi_task_handle) {
          vTaskDelete(s_rssi_task_handle);
          s_rssi_task_handle = NULL;
      }
  }

  //----------------------------------------------------------------------------------------------------------

#endif


static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGI(TAG,"connect to the AP fail");
    status = RS_STATUS_WIFI_NOT_CONNECTED;
    evt_signal(EVT_WIFI_DOWN);
    esp_wifi_connect();
    set_led(true, false, false, false, false);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Got IP: %s", ip);



    status = RS_STATUS_WIFI_CONNECTED;
    set_led(false, true, false, false, true);
    evt_signal(EVT_WIFI_UP);
 
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
    evt_signal(EVT_START_MG);
    wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
    char stationInfo[64];
    snprintf(stationInfo, sizeof(stationInfo), "Station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    ESP_LOGI(TAG, "%s", stationInfo);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
    char stationInfo[64];
    snprintf(stationInfo, sizeof(stationInfo), "Station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    ESP_LOGI(TAG, "%s", stationInfo);
    evt_signal(EVT_WIFI_DOWN);
    status = RS_STATUS_WIFI_NOT_CONNECTED;
    ip[0] = '-';
    ip[1] = '\0';
    set_led(true, false, false, false, false);
    esp_wifi_connect();
  }
}



void wifi_init_ap()
{
  wifi_config_t wifi_config = {
    .ap = {0}
  };

  esp_netif_create_default_wifi_ap();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  strcpy((char*) wifi_config.ap.ssid, SSID);
  wifi_config.ap.ssid_len = strlen(SSID);
  strcpy((char*) wifi_config.ap.password, "");
  wifi_config.ap.max_connection = 5;              //<<<<<   ----- moved from 1 to 5  TRG
  wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  wifi_config.ap.ssid_hidden = 0;                 // added TRG
  wifi_config.ap.beacon_interval = 10;            // added default is 100ms   TRG
  wifi_config.ap.channel = 6;                     // added Try 6 or 11, avoid auto  TRG
  
  ESP_LOGI(TAG, "------ AP wifi init -------");

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  
}

static void wifi_init_sta()
{
  wifi_config_t wifi_config = {.sta = {0} };

  status = RS_STATUS_WIFI_CONNECTING;

  string& ssid = settings_get_wifi_ssid();
  string& passwd = settings_get_wifi_passwd();
  strcpy((char*) wifi_config.sta.ssid, ssid.c_str());
  strcpy((char*) wifi_config.sta.password, passwd.c_str());

  ESP_LOGI(TAG, "wifi_init_sta: SSID=%s", (char*) wifi_config.sta.ssid);
  ESP_LOGI(TAG, "wifi_init_sta: Passwd=%s", (char*) wifi_config.sta.password);

  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

}


void init_wifi()
{
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                ESP_EVENT_ANY_ID,
                                                event_handler,
                                                NULL,
                                                NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                IP_EVENT_STA_GOT_IP,
                                                event_handler,
                                                NULL,
                                                NULL));

  #ifdef CONFIG_TRS_IO_USE_COMPILE_TIME_WIFI_CREDS
    storage_set_str(WIFI_KEY_SSID, CONFIG_TRS_IO_SSID);
    storage_set_str(WIFI_KEY_PASSWD, CONFIG_TRS_IO_PASSWD);
  #endif

  if (settings_has_wifi_credentials()) {
    wifi_init_sta();
  } else {
    status = RS_STATUS_WIFI_NOT_CONFIGURED;
    set_led(false, true, true, true, false);
    wifi_init_ap();
  }
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(78));    //  move from 8 to 78 to increase signal     TRG
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

  // ------------------- start RSSI monitor and update display  -------------------  
  // this may not be the best place to start the RSSI monitor, but it works since IP is updated here
  #ifdef CONFIG_TRS_IO_SSD1306  
    start_rssi_monitor();
  #endif
  // ------------------------------------------------------------------------------
       



}
