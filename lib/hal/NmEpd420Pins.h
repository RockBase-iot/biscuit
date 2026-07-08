#pragma once

#if BOARD_NM_EPD_420

// EPD on the dedicated SPI controller.
#define NM_EPD_SCK 2
#define NM_EPD_MOSI 1
#define NM_EPD_MISO -1
#define NM_EPD_CS 46
#define NM_EPD_DC 4
#define NM_EPD_RST 5
#define NM_EPD_BUSY 6

// microSD on the peripheral SPI bus.
#define NM_SD_SCK 9
#define NM_SD_MOSI 10
#define NM_SD_MISO 11
#define NM_SD_CS 7

// Two-button local input, active low.
#define NM_BOOT_BTN 0
#define NM_USER_BTN 45

// Battery ADC path.
#define NM_ADC_EN 43
#define NM_BATT_ADC 3
#define NM_BATT_ADC_DIV 2
#define NM_BATT_MIN_MV 2500
#define NM_BATT_MAX_MV 4500
#define NM_BATT_ADC_SETTLE_MS 80
#define NM_BATT_ADC_SAMPLES 24

// External peripheral power-control pins used by NM-EPD-420.
#define NM_TEMP_SDA 39
#define NM_TEMP_SCL 38
#define NM_TEMP_CTL 40
#define NM_PA_CTRL 41
#define NM_CODEC_EN 44
#define NM_LORA_EN 47

// Shared LoRa/TF SPI and auxiliary pins. Keep high-Z when unused.
#define NM_LORA_NSS 8
#define NM_LORA_SCK 9
#define NM_LORA_MOSI 10
#define NM_LORA_MISO 11
#define NM_LORA_RST 12
#define NM_LORA_BUSY 13
#define NM_LORA_DIO1 14

// I2S / audio codec signal pins. Keep high-Z when unused.
#define NM_I2S_SCLK 15
#define NM_I2S_ASDOUT 16
#define NM_I2S_LRCK 17
#define NM_I2S_DSIN 18
#define NM_I2S_MCLK 21

#endif
