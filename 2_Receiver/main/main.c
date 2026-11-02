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

// --- RECEIVER PINOUT (ESP32-S3) ---
#define I2S_WS      5   // Word Select (Shared Clock)
#define I2S_SCK     7   // Bit Clock (Shared Clock)
#define I2S_DOUT    8   // Serial Data OUT (To MAX98357A DIN)

// --- AUDIO PARAMETERS ---
#define SAMPLE_RATE 16000
#define CHUNK_SIZE  240  // Fits the 250-byte ESP-NOW limit

static const char *TAG = "RX_NODE";

// I2S Transmit Channel (ESP32 -> Amplifier)
i2s_chan_handle_t tx_chan;

// FreeRTOS Queue to hold incoming audio packets
QueueHandle_t audio_queue;

// Structure for the queue elements
typedef struct {
    uint8_t payload[CHUNK_SIZE];
    size_t length;
} audio_packet_t;

// =========================================================
// 1. ESP-NOW Receive Callback
// =========================================================
// This function is triggered automatically when a packet arrives
void espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    if (data_len <= CHUNK_SIZE) {
        audio_packet_t packet;
        memcpy(packet.payload, data, data_len);
        packet.length = data_len;
        
        // Send to queue without blocking (timeout 0) to avoid crashing the Wi-Fi task
        xQueueSend(audio_queue, &packet, 0);
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
    
    // Register the callback for incoming packets
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_LOGI(TAG, "ESP-NOW Receiver Initialized!");
}

// =========================================================
// 3. I2S Amplifier Initialization (MAX98357A)
// =========================================================
void i2s_init(void) {
    // 1. Allocate the I2S channel (TX mode)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    
    // Automatically clear TX DMA buffer with zeroes on underrun to prevent buzzing
    chan_cfg.auto_clear = true;
    
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    // 2. Configure the channel for standard I2S
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK,
            .ws   = I2S_WS,
            .dout = I2S_DOUT, // Transmitting data out
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    // 3. Initialize and enable the channel
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    
    ESP_LOGI(TAG, "I2S Initialized for MAX98357A!");
}

// =========================================================
// 4. Audio Playback Task
// =========================================================
void rx_task(void *pvParameters) {
    audio_packet_t packet;
    size_t bytes_written;
    uint32_t packet_count = 0;

    while (1) {
        // Wait until a packet arrives in the queue
        if (xQueueReceive(audio_queue, &packet, portMAX_DELAY) == pdTRUE) {
            
            // Push the audio chunk to the I2S Amplifier
            if (i2s_channel_write(tx_chan, packet.payload, packet.length, &bytes_written, portMAX_DELAY) == ESP_OK) {
                packet_count++;
                if (packet_count % 50 == 0) {
                    ESP_LOGI(TAG, "Playing... 50 packets processed.");
                }
            } else {
                ESP_LOGW(TAG, "I2S Write Error");
            }
        }
    }
}

// =========================================================
// 5. ESP32 Main Entry Point
// =========================================================
void app_main(void) {
    ESP_LOGI(TAG, "Starting EchoNow Receiver...");

    // Create a queue capable of holding 20 audio chunks (approx 150ms of buffer)
    audio_queue = xQueueCreate(20, sizeof(audio_packet_t));

    wifi_espnow_init();
    i2s_init();

    // Create playback task pinned to Core 1
    xTaskCreatePinnedToCore(rx_task, "rx_task", 4096, NULL, 5, NULL, 1);
}