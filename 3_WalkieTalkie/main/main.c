#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_log.h"

// --- WALKIE-TALKIE PINOUT (ESP32-S3) ---
#define I2S_WS      5   // Word Select (Shared Clock for Mic & Amp)
#define I2S_SCK     7   // Bit Clock (Shared Clock for Mic & Amp)
#define I2S_SD      6   // Serial Data IN (From INMP441)
#define I2S_DOUT    8   // Serial Data OUT (To MAX98357A)
#define PTT_PIN     4   // Push-To-Talk Button

// --- AUDIO PARAMETERS ---
#define SAMPLE_RATE 16000
#define CHUNK_SIZE  240

static const char *TAG = "WALKIE_TALKIE";

uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// I2S Handles for both Microphone (RX) and Amplifier (TX)
i2s_chan_handle_t tx_chan;
i2s_chan_handle_t rx_chan;

// Queue for incoming audio
QueueHandle_t audio_queue;

typedef struct {
    uint8_t payload[CHUNK_SIZE];
    size_t length;
} audio_packet_t;

// =========================================================
// 1. ESP-NOW Callback
// =========================================================
void espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    if (data_len <= CHUNK_SIZE) {
        audio_packet_t packet;
        memcpy(packet.payload, data, data_len);
        packet.length = data_len;
        
        // Only queue incoming audio if we are NOT pressing PTT (prevents hearing yourself/echo)
        if (gpio_get_level(PTT_PIN) == 1) {
            xQueueSend(audio_queue, &packet, 0);
        }
    }
}

// =========================================================
// 2. Wi-Fi and ESP-NOW Initialization
// =========================================================
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

    // Disable Wi-Fi power save to ensure minimal latency for real-time audio
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK(esp_now_init());

    // Register TX Peer
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
    peer_info.channel = 0; 
    peer_info.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    // Register RX Callback
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    
    ESP_LOGI(TAG, "ESP-NOW Half-Duplex Initialized!");
}

// =========================================================
// 3. I2S Initialization (Combined TX & RX)
// =========================================================
void i2s_init(void) {
    // 1. Allocate BOTH channels simultaneously on the same I2S Port
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    
    // Automatically clear TX DMA buffer with zeroes on underrun to prevent buzzing
    chan_cfg.auto_clear = true;
    
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan));

    // 2. Configure the shared settings (Clocks and Data pins)
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK,
            .ws   = I2S_WS,
            .dout = I2S_DOUT, // Amp out
            .din  = I2S_SD,   // Mic in
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    // 3. Apply config and enable both channels
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    
    ESP_LOGI(TAG, "I2S Shared Clocks Initialized for Mic & Amp!");
}

// =========================================================
// 4. Transmission Task (Microphone -> ESP-NOW)
// =========================================================
void tx_task(void *pvParameters) {
    uint8_t audio_buffer[CHUNK_SIZE];
    size_t bytes_read;

    while (1) {
        // Always read to keep DMA buffer fresh
        if (i2s_channel_read(rx_chan, audio_buffer, CHUNK_SIZE, &bytes_read, portMAX_DELAY) == ESP_OK) {
            
            // If button is pressed, transmit!
            if (gpio_get_level(PTT_PIN) == 0) {
                esp_now_send(broadcast_mac, audio_buffer, bytes_read);
            }
        }
    }
}

// =========================================================
// 5. Reception Task (ESP-NOW -> Amplifier)
// =========================================================
void rx_task(void *pvParameters) {
    audio_packet_t packet;
    size_t bytes_written;

    while (1) {
        // Wait for incoming audio packets
        if (xQueueReceive(audio_queue, &packet, portMAX_DELAY) == pdTRUE) {
            
            // Double check PTT to ensure we don't play audio while transmitting
            if (gpio_get_level(PTT_PIN) == 1) {
                i2s_channel_write(tx_chan, packet.payload, packet.length, &bytes_written, portMAX_DELAY);
            }
        }
    }
}

// =========================================================
// 6. Main Entry Point
// =========================================================
void app_main(void) {
    ESP_LOGI(TAG, "Starting EchoNow Full Walkie-Talkie...");

    gpio_set_direction(PTT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PTT_PIN, GPIO_PULLUP_ONLY);

    audio_queue = xQueueCreate(20, sizeof(audio_packet_t));

    wifi_espnow_init();
    i2s_init();

    // Pin TX to Core 1 and RX to Core 0 to balance the CPU load
    xTaskCreatePinnedToCore(tx_task, "tx_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(rx_task, "rx_task", 4096, NULL, 5, NULL, 0);
}