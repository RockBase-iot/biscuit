#pragma once
#include <Arduino.h>
#if BOARD_NM_EPD_420
#include <GxEPD2_3C.h>
#include <GxEPD2_GFX.h>
#include <SPI.h>
#include <gdey3c/GxEPD2_420c_GDEY042Z98.h>

#include "NmEpd420Pins.h"
#include "nm_epd_420/epd_uc8179_420c.h"
#else
#include <EInkDisplay.h>
#endif

class HalDisplay {
 public:
  // Constructor with pin configuration
  HalDisplay();

  // Destructor
  ~HalDisplay();

  // Refresh modes
  enum RefreshMode {
    FULL_REFRESH,  // Full refresh with complete waveform
    HALF_REFRESH,  // Half refresh (1720ms) - balanced quality and speed
    FAST_REFRESH   // Fast refresh using custom LUT
  };

  // Initialize the display hardware and driver
  void begin();

  // Display dimensions
#if BOARD_NM_EPD_420
  static constexpr uint16_t DISPLAY_WIDTH = 400;
  static constexpr uint16_t DISPLAY_HEIGHT = 300;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;
#else
  static constexpr uint16_t DISPLAY_WIDTH = EInkDisplay::DISPLAY_WIDTH;
  static constexpr uint16_t DISPLAY_HEIGHT = EInkDisplay::DISPLAY_HEIGHT;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;
#endif

  // Frame buffer operations
  void clearScreen(uint8_t color = 0xFF) const;
  void drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 bool fromProgmem = false) const;
  void drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            bool fromProgmem = false) const;

  void displayBuffer(RefreshMode mode = RefreshMode::FAST_REFRESH, bool turnOffScreen = false);
  void refreshDisplay(RefreshMode mode = RefreshMode::FAST_REFRESH, bool turnOffScreen = false);

  // Power management
  void deepSleep();

  // Access to frame buffer
  uint8_t* getFrameBuffer() const;

  void copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer);
  void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer);
  void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer);
  void cleanupGrayscaleBuffers(const uint8_t* bwBuffer);

  void displayGrayBuffer(bool turnOffScreen = false);

  // Runtime geometry passthrough
  uint16_t getDisplayWidth() const;
  uint16_t getDisplayHeight() const;
  uint16_t getDisplayWidthBytes() const;
  uint32_t getBufferSize() const;

 private:
#if BOARD_NM_EPD_420
  static constexpr uint16_t EPD_PAGE_HEIGHT = DISPLAY_HEIGHT;
  using Ssd1683Display = GxEPD2_3C<GxEPD2_420c_GDEY042Z98, EPD_PAGE_HEIGHT>;
  using Uc8179Display = GxEPD2_3C<GxEPD2_420c_NM_UC8179, EPD_PAGE_HEIGHT>;
  using EpdDisplay = GxEPD2_GFX;

  mutable uint8_t frameBuffer[BUFFER_SIZE];
  mutable uint8_t txBuffer[BUFFER_SIZE];
  GxEPD2_420c_GDEY042Z98 ssd1683Driver{NM_EPD_CS, NM_EPD_DC, NM_EPD_RST, NM_EPD_BUSY};
  GxEPD2_420c_NM_UC8179 uc8179Driver{NM_EPD_CS, NM_EPD_DC, NM_EPD_RST, NM_EPD_BUSY};
  Ssd1683Display ssd1683{ssd1683Driver};
  Uc8179Display uc8179{uc8179Driver};
  EpdDisplay* activeDisplay = &ssd1683;
  bool activeIsUc8179 = false;

  void selectPanelDriver();
  bool detectUc8179Panel();
  EpdDisplay& rawDisplay() const { return *activeDisplay; }
  void writeBitmapToFramebuffer(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                bool transparent, bool fromProgmem) const;
#else
  EInkDisplay einkDisplay;
#endif
};

extern HalDisplay display;
