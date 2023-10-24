/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "tm1637.h"
#include "DHT22.h"

const gpio_num_t LED_CLK = CONFIG_TM1637_CLK_PIN;
const gpio_num_t LED_DTA = CONFIG_TM1637_DIO_PIN;
const gpio_num_t DHT_DTA = CONFIG_DHT22_DATA_PIN;

void lcd_tm1637_task(void * arg)
{
	tm1637_led_t * lcd = tm1637_init(LED_CLK, LED_DTA);
	setenv("TZ", "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", 1);
	tzset();

	while (true)
	{
		// Test segment control
		uint8_t seg_data[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};
		for (uint8_t x=0; x<32; ++x)
		{
			uint8_t v_seg_data = seg_data[x%6];
			tm1637_set_segment_raw(lcd, 0, v_seg_data);
			tm1637_set_segment_raw(lcd, 1, v_seg_data);
			tm1637_set_segment_raw(lcd, 2, v_seg_data);
			tm1637_set_segment_raw(lcd, 3, v_seg_data);
			vTaskDelay(100 / portTICK_PERIOD_MS);
		}

		// Test brightness
		for (int x=0; x<7; x++) {
			tm1637_set_brightness(lcd, x);
			tm1637_set_number(lcd, 8888);
			vTaskDelay(300 / portTICK_PERIOD_MS);
		}

		for (uint8_t x=0; x<3; ++x)
		{
			// Set random system time
			struct timeval tm_test = {1517769863 + (x*3456), 0};
			settimeofday(&tm_test, NULL);

			// Get current system time
			time_t now = 0;
			struct tm timeinfo = { 0 };
			time(&now);
			localtime_r(&now, &timeinfo);
			int time_number = 100 * timeinfo.tm_hour + timeinfo.tm_min;

			// Display time with blinking dots
			for (int z=0; z<5; ++z) {
				tm1637_set_number_lead_dot(lcd, time_number, true, z%2 ? 0xFF : 0x00);
				vTaskDelay(500 / portTICK_PERIOD_MS);
			}
		}

		// Test display numbers
		for (int x=0; x<16; ++x) {
			bool show_dot = x%2; // Show dot every 2nd cycle
			tm1637_set_segment_number(lcd, 0, x, show_dot);
			tm1637_set_segment_number(lcd, 1, x, show_dot); // On my display "dot" (clock symbol ":") connected only here
			tm1637_set_segment_number(lcd, 2, x, show_dot);
			tm1637_set_segment_number(lcd, 3, x, show_dot);
			vTaskDelay(100 / portTICK_PERIOD_MS);
		}

	}
}

void DHT_reader_task(void *pvParameter)
{
	setDHTgpio(DHT_DTA);

	while(1) {
	
		printf("DHT Sensor Readings\n" );
		int ret = readDHT();
		
		errorHandler(ret);

		printf("Humidity %.2f %%\n", getHumidity());
		printf("Temperature %.2f degC\n\n", getTemperature());
		
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    for (int i = 3; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    //fflush(stdout);
    //esp_restart();

    xTaskCreate(&lcd_tm1637_task, "lcd_tm1637_task", 4096, NULL, 5, NULL);
	xTaskCreate(&DHT_reader_task, "DHT_reader_task", 2048, NULL, 5, NULL );

	printf("End of main\n");
	fflush(stdout);
//	esp_restart();
}
