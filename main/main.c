// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "driver/gpio.h"
// #include "driver/adc.h"
// #include "esp_log.h"
// #include "dht11.h"
// #include "bmp180.h"

// static const char * TAG1 = "LDR";
// static const char * TAG2 = "DHT11";
// static const char * TAG3 = "BMP180";

// #define LDR_Pin (ADC1_CHANNEL_0)  // GPIO1
// #define ADC_SIZE (ADC_WIDTH_BIT_12)

// #define DHT11_Pin (GPIO_NUM_15)
// #define REFERENCE_PPRESSURE 101325l
// #define I2C_pin_SDA 8
// #define I2C_pin_SCL 9

// void LDR_task(void *pvParameter)
// {
//     adc1_config_width(ADC_SIZE);
//     adc1_config_channel_atten(LDR_Pin, ADC_ATTEN_DB_11);
//     while (1)
//     {
//         int adc_value = adc1_get_raw(LDR_Pin);

//         ESP_LOGI(TAG1, "light intensity: %d", adc_value);
//         vTaskDelay(1000/portTICK_PERIOD_MS);  //Delay for 1000ms
//     }
//     vTaskDelete(NULL);
// }


// void DHT11(void *pvParameter)
// {
//     DHT11_init(DHT11_Pin);
//     while (1)
//     {

//         ESP_LOGI(TAG2, "Temperature is %d \n", DHT11_read().temperature);
//         ESP_LOGI(TAG2, "Humidity is %d\n", DHT11_read().humidity);

//         vTaskDelay(2000/portTICK_PERIOD_MS);  //Delay for 2000ms
//     }
//     vTaskDelete(NULL);
    
// }

// void bmp180_task(void *pvParameter)
// {
//     esp_err_t err = bmp180_init(I2C_pin_SDA,I2C_pin_SCL);
//         if(err != ESP_OK){
//             ESP_LOGE(TAG3, "Init bmp180 failed = %d", err);
//             vTaskDelete(NULL);
//             return;
//         }
//     while (1)
//     {
//         uint32_t pressure;
//         float altitude;
//         float temperature;

//         err = bmp180_read_pressure(&pressure);
//         if(err != ESP_OK){
//             ESP_LOGE(TAG3, "Reading of pressure from BMP180 failed, err = %d", err);
//         }
    
//         err = bmp180_read_altitude(REFERENCE_PPRESSURE, &altitude);
//         if(err != ESP_OK){
//             ESP_LOGE(TAG3, "Reading of altitude from BMP180 failed, err = %d", err);
//         }

//         err = bmp180_read_temperature(&temperature);
//         if(err != ESP_OK){
//             ESP_LOGE(TAG3, "Reading of temperature from BMP180 failed, err = %d", err);
//         }

//         ESP_LOGI(TAG3, "Pressure %lu Pa, Altitute %.2f m, Temperature : %.1f C", pressure, altitude, temperature);
//         vTaskDelay(3000/portTICK_PERIOD_MS);    //Delay for 3000ms
//     }
    
// }

// void app_main(void)
// {
//     xTaskCreate(&LDR_task, "LDR Task", 2048, NULL, 5, NULL); 
//     xTaskCreate(&DHT11, "DHT11 Task", 2048, NULL, 4, NULL);
//     xTaskCreate(&bmp180_task, "BMP180 Task", 4096, NULL, 3, NULL);
// }


#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "esp_adc/adc_oneshot.h"
#include "dht11.h"
#include "bmp180.h"

static const char *TAG1 = "LDR";
static const char *TAG2 = "DHT11";
static const char *TAG3 = "BMP180";
static const char *TAG_MQTT = "MQTT";
static const char *TAG_WIFI = "WiFi";

TaskHandle_t processorTaskHandle = NULL;

#define LDR_Pin (ADC_CHANNEL_0)
#define DHT11_Pin (GPIO_NUM_15)
#define I2C_pin_SDA 8
#define I2C_pin_SCL 9
#define REFERENCE_PPRESSURE 101325l
#define WIFI_SSID "wong"
#define WIFI_PASSWORD "12341111"
#define TB_MQTT_HOST "mqtt://demo.thingsboard.io"
#define TB_MQTT_TOKEN "UpWkOeKseEMdcqfwuJuH"
#define TB_TOPIC_TELEMETRY "v1/devices/me/telemetry"

esp_mqtt_client_handle_t mqtt_client = NULL;
adc_oneshot_unit_handle_t adc_handle;

void init_adc(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config, &adc_handle);

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_11,
    };
    adc_oneshot_config_channel(adc_handle, LDR_Pin, &channel_config);
}

void init_mqtt(void) {
    esp_mqtt_client_config_t mqtt_config = {
        .broker = {
            .address = {
                .uri = TB_MQTT_HOST
            }
        },
        .credentials = {
            .username = TB_MQTT_TOKEN
        },
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_start(mqtt_client);
}

void publish_to_thingsboard(const char* sensor, float value) {
    char telemetry_payload[128];
    snprintf(telemetry_payload, sizeof(telemetry_payload), "{\"%s\": %.2f}", sensor, value);

    esp_mqtt_client_publish(mqtt_client, TB_TOPIC_TELEMETRY, telemetry_payload, 0, 1, 0);
    ESP_LOGI(TAG_MQTT, "Published telemetry: %s", telemetry_payload);
}

void LDR_task(void *pvParameter) {
    const int min_adc_value = 0;
    const int max_adc_value = 4095;

    while (1) {
        int adc_value = 0;
        adc_oneshot_read(adc_handle, LDR_Pin, &adc_value);

        float percentage = ((float)(adc_value - min_adc_value) / (max_adc_value - min_adc_value)) * 100;
        ESP_LOGI(TAG1, "Light intensity: %d (%.2f%%)", adc_value, percentage);

        // Publish LDR data to ThingsBoard
        publish_to_thingsboard("LDR", percentage);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

void DHT11_task(void *pvParameter) {
    DHT11_init(DHT11_Pin);
    while (1) {
        int temperature = DHT11_read().temperature;
        int humidity = DHT11_read().humidity;

        ESP_LOGI(TAG2, "Temperature: %d°C, Humidity: %d%%", temperature, humidity);

        // Publish DHT11 data to ThingsBoard
        publish_to_thingsboard("DHT11_Temperature", temperature);
        publish_to_thingsboard("DHT11_Humidity", humidity);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    vTaskDelete(NULL);
}

void bmp180_task(void *pvParameter) {
    esp_err_t err = bmp180_init(I2C_pin_SDA, I2C_pin_SCL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG3, "Init bmp180 failed = %d", err);
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        uint32_t pressure;
        float altitude;
        float temperature;

        err = bmp180_read_pressure(&pressure);
        if (err != ESP_OK) {
            ESP_LOGE(TAG3, "Reading of pressure from BMP180 failed, err = %d", err);
        }

        err = bmp180_read_altitude(REFERENCE_PPRESSURE, &altitude);
        if (err != ESP_OK) {
            ESP_LOGE(TAG3, "Reading of altitude from BMP180 failed, err = %d", err);
        }

        err = bmp180_read_temperature(&temperature);
        if (err != ESP_OK) {
            ESP_LOGE(TAG3, "Reading of temperature from BMP180 failed, err = %d", err);
        }

        ESP_LOGI(TAG3, "Pressure: %lu Pa, Altitude: %.2f m, Temperature: %.1f°C", pressure, altitude, temperature);

        // Publish BMP180 data to ThingsBoard
        publish_to_thingsboard("BMP180_Pressure", pressure);
        publish_to_thingsboard("BMP180_Altitude", altitude);
        publish_to_thingsboard("BMP180_Temperature", temperature);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    vTaskDelete(NULL);
}

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG_WIFI, "Wi-Fi disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_WIFI, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void init_wifi(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_wifi();
    init_adc();
    init_mqtt();

    xTaskCreate(&LDR_task, "LDR Task", 4096, NULL, 5, NULL);
    xTaskCreate(&DHT11_task, "DHT11 Task", 4096, NULL, 5, NULL);
    xTaskCreate(&bmp180_task, "BMP180 Task", 4096, NULL, 3, NULL);
}
