#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_log.h"

// --- TRANSMITTER PINOUT (ESP32-S3) ---
#define I2S_WS      5   // Word Select (L/R Clock)
#define I2S_SD      6   // Serial Data (Microphone In)
#define I2S_SCK     7   // Bit Clock
#define PTT_PIN     4   // Push-To-Talk Button

// --- AUDIO PARAMETERS ---
#define SAMPLE_RATE 16000
#define CHUNK_SIZE  240  // (value in bytes) 120 samples of 16-bit fits the 250-byte ESP-NOW limit

static const char *TAG = "TX_NODE";

// Broadcast Address: Sends to any device on the network
uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Global handle for the I2S Receive Channel
i2s_chan_handle_t rx_chan;


// 1 - Wi-Fi and ESP-NOW Initialization
void wifi_espnow_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));     // Disable Wi-Fi power save to ensure minimal latency for real-time audio
    ESP_ERROR_CHECK(esp_now_init());

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
    peer_info.channel = 0; 
    peer_info.encrypt = false;
    
    if (esp_now_add_peer(&peer_info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add broadcast peer");
        return;
    }
    ESP_LOGI(TAG, "ESP-NOW Broadcast Initialized!");
}

// 2 - I2S Microphone Initialization (ESP-IDF v5 API)
void i2s_init(void) {
    // Allocate the I2S channel
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    // Configure the channel for standard I2S
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK,
            .ws   = I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_SD,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    
    // Ensure it reads from the Left slot (INMP441 L/R pin to GND)
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    // Initialize and enable the channel
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    
    ESP_LOGI(TAG, "I2S v5 Initialized for INMP441!");
}

// 3 - Main Transmission Task
void tx_task(void *pvParameters) {
    uint8_t audio_buffer[CHUNK_SIZE];
    size_t bytes_read;
    uint32_t packet_count = 0;

    while (1) {
        // Continuously read microphone data
        if (i2s_channel_read(rx_chan, audio_buffer, CHUNK_SIZE, &bytes_read, portMAX_DELAY) == ESP_OK) {
            
            // Only send via ESP-NOW if PTT button is pressed (LOW)
            if (gpio_get_level(PTT_PIN) == 0) {
                
                esp_err_t result = esp_now_send(broadcast_mac, audio_buffer, bytes_read);
                
                if (result == ESP_OK) {
                    packet_count++;
                    if (packet_count % 50 == 0) {
                        ESP_LOGI(TAG, "Transmitting... 50 packets sent.");
                    }
                } else {
                    ESP_LOGW(TAG, "Error sending ESP-NOW packet");
                }
            } else {
                packet_count = 0;
            }
        }
    }
}


// 4 - ESP32 Main Entry Point

void app_main(void) {
    ESP_LOGI(TAG, "Starting EchoNow Transmitter...");

    gpio_set_direction(PTT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PTT_PIN, GPIO_PULLUP_ONLY);

    wifi_espnow_init();
    i2s_init();

    xTaskCreatePinnedToCore(tx_task, "tx_task", 4096, NULL, 5, NULL, 1);
}