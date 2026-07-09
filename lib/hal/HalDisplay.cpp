#include <HalDisplay.h>
#include <HalGPIO.h>  // EPD pin defines

#if BOARD_NM_EPD_420
#include <cstring>
#include <esp32-hal-gpio.h>
#include <pgmspace.h>
#endif

// Global HalDisplay instance
HalDisplay display;

#if BOARD_NM_EPD_420

namespace {
template <typename Display>
void initActiveDisplay(Display& epd, bool fastFullUpdate) {
  epd.init(0, true, 2, false);
  epd.epd2.selectFastFullUpdate(fastFullUpdate);
  epd.setRotation(0);
}

template <typename Display>
void flushBitmapToDisplay(Display& epd, const uint8_t* txBuffer, bool turnOffScreen) {
  epd.setFullWindow();
  epd.firstPage();
  do {
    epd.fillScreen(GxEPD_WHITE);
    epd.drawBitmap(0, 0, txBuffer, HalDisplay::DISPLAY_WIDTH, HalDisplay::DISPLAY_HEIGHT, GxEPD_BLACK);
  } while (epd.nextPage());

  if (turnOffScreen) {
    epd.hibernate();
  }
}
}  // namespace

HalDisplay::HalDisplay() { memset(frameBuffer, 0xFF, sizeof(frameBuffer)); }

HalDisplay::~HalDisplay() {}

void HalDisplay::begin() {
  SPI.begin(NM_EPD_SCK, NM_EPD_MISO, NM_EPD_MOSI, NM_EPD_CS);
  selectPanelDriver();
  if (activeIsUc8179) {
    initActiveDisplay(uc8179, false);
  } else {
    initActiveDisplay(ssd1683, true);
  }
  clearScreen();
}

void HalDisplay::selectPanelDriver() {
  activeIsUc8179 = detectUc8179Panel();
}

bool HalDisplay::detectUc8179Panel() {
  pinMode(NM_EPD_BUSY, INPUT_PULLUP);
  pinMode(NM_EPD_RST, OUTPUT);
  digitalWrite(NM_EPD_RST, HIGH);
  delay(5);
  digitalWrite(NM_EPD_RST, LOW);
  delay(10);
  digitalWrite(NM_EPD_RST, HIGH);
  delay(2);

  uint8_t lowCount = 0;
  for (uint8_t i = 0; i < 80; ++i) {
    if (digitalRead(NM_EPD_BUSY) == LOW) {
      ++lowCount;
    }
    delay(1);
  }

  return lowCount < 12;
}

void HalDisplay::clearScreen(uint8_t color) const { memset(frameBuffer, color, BUFFER_SIZE); }

void HalDisplay::writeBitmapToFramebuffer(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                          bool transparent, bool fromProgmem) const {
  if (!imageData || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
    return;
  }

  const uint16_t srcStride = (w + 7) / 8;
  const uint16_t drawW = (x + w > DISPLAY_WIDTH) ? (DISPLAY_WIDTH - x) : w;
  const uint16_t drawH = (y + h > DISPLAY_HEIGHT) ? (DISPLAY_HEIGHT - y) : h;

  for (uint16_t row = 0; row < drawH; ++row) {
    for (uint16_t col = 0; col < drawW; ++col) {
      const uint32_t srcIndex = static_cast<uint32_t>(row) * srcStride + (col / 8);
      const uint8_t srcByte = fromProgmem ? pgm_read_byte(&imageData[srcIndex]) : imageData[srcIndex];
      const bool sourceBlack = ((srcByte >> (7 - (col % 8))) & 0x01) != 0;
      if (transparent && !sourceBlack) {
        continue;
      }

      const uint16_t dstX = x + col;
      const uint16_t dstY = y + row;
      const uint32_t dstIndex = static_cast<uint32_t>(dstY) * DISPLAY_WIDTH_BYTES + (dstX / 8);
      const uint8_t mask = 1 << (7 - (dstX % 8));
      if (sourceBlack) {
        frameBuffer[dstIndex] &= ~mask;
      } else {
        frameBuffer[dstIndex] |= mask;
      }
    }
  }
}

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           bool fromProgmem) const {
  writeBitmapToFramebuffer(imageData, x, y, w, h, false, fromProgmem);
}

void HalDisplay::drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      bool fromProgmem) const {
  writeBitmapToFramebuffer(imageData, x, y, w, h, true, fromProgmem);
}

void HalDisplay::displayBuffer(HalDisplay::RefreshMode mode, bool turnOffScreen) {
  (void)mode;
  for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
    txBuffer[i] = static_cast<uint8_t>(~frameBuffer[i]);
  }

  if (activeIsUc8179) {
    flushBitmapToDisplay(uc8179, txBuffer, turnOffScreen);
  } else {
    flushBitmapToDisplay(ssd1683, txBuffer, turnOffScreen);
  }
}

void HalDisplay::refreshDisplay(HalDisplay::RefreshMode mode, bool turnOffScreen) { displayBuffer(mode, turnOffScreen); }

void HalDisplay::deepSleep() {
  if (activeIsUc8179) {
    uc8179.hibernate();
  } else {
    ssd1683.hibernate();
  }
}

uint8_t* HalDisplay::getFrameBuffer() const { return frameBuffer; }

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  (void)msbBuffer;
  copyGrayscaleLsbBuffers(lsbBuffer);
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) {
  if (lsbBuffer) {
    memcpy(frameBuffer, lsbBuffer, BUFFER_SIZE);
  }
}

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) {
  if (msbBuffer) {
    memcpy(frameBuffer, msbBuffer, BUFFER_SIZE);
  }
}

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) {
  if (bwBuffer) {
    memcpy(frameBuffer, bwBuffer, BUFFER_SIZE);
  }
}

void HalDisplay::displayGrayBuffer(bool turnOffScreen) { displayBuffer(FULL_REFRESH, turnOffScreen); }

uint16_t HalDisplay::getDisplayWidth() const { return DISPLAY_WIDTH; }

uint16_t HalDisplay::getDisplayHeight() const { return DISPLAY_HEIGHT; }

uint16_t HalDisplay::getDisplayWidthBytes() const { return DISPLAY_WIDTH_BYTES; }

uint32_t HalDisplay::getBufferSize() const { return BUFFER_SIZE; }

#else

#define SD_SPI_MISO 7

HalDisplay::HalDisplay() : einkDisplay(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY) {}

HalDisplay::~HalDisplay() {}

void HalDisplay::begin() {
  einkDisplay.begin();
}

void HalDisplay::clearScreen(uint8_t color) const { einkDisplay.clearScreen(color); }

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           bool fromProgmem) const {
  einkDisplay.drawImage(imageData, x, y, w, h, fromProgmem);
}

void HalDisplay::drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      bool fromProgmem) const {
  einkDisplay.drawImageTransparent(imageData, x, y, w, h, fromProgmem);
}

EInkDisplay::RefreshMode convertRefreshMode(HalDisplay::RefreshMode mode) {
  switch (mode) {
    case HalDisplay::FULL_REFRESH:
      return EInkDisplay::FULL_REFRESH;
    case HalDisplay::HALF_REFRESH:
      return EInkDisplay::HALF_REFRESH;
    case HalDisplay::FAST_REFRESH:
    default:
      return EInkDisplay::FAST_REFRESH;
  }
}

void HalDisplay::displayBuffer(HalDisplay::RefreshMode mode, bool turnOffScreen) {
  einkDisplay.displayBuffer(convertRefreshMode(mode), turnOffScreen);
}

void HalDisplay::refreshDisplay(HalDisplay::RefreshMode mode, bool turnOffScreen) {
  einkDisplay.refreshDisplay(convertRefreshMode(mode), turnOffScreen);
}

void HalDisplay::deepSleep() { einkDisplay.deepSleep(); }

uint8_t* HalDisplay::getFrameBuffer() const { return einkDisplay.getFrameBuffer(); }

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  einkDisplay.copyGrayscaleBuffers(lsbBuffer, msbBuffer);
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) { einkDisplay.copyGrayscaleLsbBuffers(lsbBuffer); }

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) { einkDisplay.copyGrayscaleMsbBuffers(msbBuffer); }

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) { einkDisplay.cleanupGrayscaleBuffers(bwBuffer); }

void HalDisplay::displayGrayBuffer(bool turnOffScreen) { einkDisplay.displayGrayBuffer(turnOffScreen); }

uint16_t HalDisplay::getDisplayWidth() const { return EInkDisplay::DISPLAY_WIDTH; }

uint16_t HalDisplay::getDisplayHeight() const { return EInkDisplay::DISPLAY_HEIGHT; }

uint16_t HalDisplay::getDisplayWidthBytes() const { return EInkDisplay::DISPLAY_WIDTH_BYTES; }

uint32_t HalDisplay::getBufferSize() const { return EInkDisplay::BUFFER_SIZE; }

#endif
