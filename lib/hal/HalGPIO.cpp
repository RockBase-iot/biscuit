#include <HalGPIO.h>
#include <Logging.h>
#include <Preferences.h>
#include <SPI.h>
#include <Wire.h>
#if BOARD_NM_EPD_420
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#endif
#include <esp_sleep.h>

// Global HalGPIO instance
HalGPIO gpio;

namespace X3GPIO {

struct X3ProbeResult {
  bool bq27220 = false;
  bool ds3231 = false;
  bool qmi8658 = false;

  uint8_t score() const {
    return static_cast<uint8_t>(bq27220) + static_cast<uint8_t>(ds3231) + static_cast<uint8_t>(qmi8658);
  }
};

bool readI2CReg8(uint8_t addr, uint8_t reg, uint8_t* outValue) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(addr, static_cast<uint8_t>(1), static_cast<uint8_t>(true)) < 1) {
    return false;
  }
  *outValue = Wire.read();
  return true;
}

bool readI2CReg16LE(uint8_t addr, uint8_t reg, uint16_t* outValue) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(addr, static_cast<uint8_t>(2), static_cast<uint8_t>(true)) < 2) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }
  const uint8_t lo = Wire.read();
  const uint8_t hi = Wire.read();
  *outValue = (static_cast<uint16_t>(hi) << 8) | lo;
  return true;
}

bool readBQ27220CurrentMA(int16_t* outCurrent) {
  uint16_t raw = 0;
  if (!readI2CReg16LE(I2C_ADDR_BQ27220, BQ27220_CUR_REG, &raw)) {
    return false;
  }
  *outCurrent = static_cast<int16_t>(raw);
  return true;
}

bool probeBQ27220Signature() {
  uint16_t soc = 0;
  uint16_t voltageMv = 0;
  if (!readI2CReg16LE(I2C_ADDR_BQ27220, BQ27220_SOC_REG, &soc)) {
    return false;
  }
  if (soc > 100) {
    return false;
  }
  if (!readI2CReg16LE(I2C_ADDR_BQ27220, BQ27220_VOLT_REG, &voltageMv)) {
    return false;
  }
  return voltageMv >= 2500 && voltageMv <= 5000;
}

bool probeDS3231Signature() {
  uint8_t sec = 0;
  if (!readI2CReg8(I2C_ADDR_DS3231, DS3231_SEC_REG, &sec)) {
    return false;
  }
  const uint8_t tensDigit = (sec >> 4) & 0x07;
  const uint8_t onesDigit = sec & 0x0F;

  return tensDigit <= 5 && onesDigit <= 9;
}

bool probeQMI8658Signature() {
  uint8_t whoami = 0;
  if (readI2CReg8(I2C_ADDR_QMI8658, QMI8658_WHO_AM_I_REG, &whoami) && whoami == QMI8658_WHO_AM_I_VALUE) {
    return true;
  }
  if (readI2CReg8(I2C_ADDR_QMI8658_ALT, QMI8658_WHO_AM_I_REG, &whoami) && whoami == QMI8658_WHO_AM_I_VALUE) {
    return true;
  }
  return false;
}

X3ProbeResult runX3ProbePass() {
  X3ProbeResult result;
  Wire.begin(X3_I2C_SDA, X3_I2C_SCL, X3_I2C_FREQ);
  Wire.setTimeOut(6);

  result.bq27220 = probeBQ27220Signature();
  result.ds3231 = probeDS3231Signature();
  result.qmi8658 = probeQMI8658Signature();

  Wire.end();
  pinMode(20, INPUT);
  pinMode(0, INPUT);
  return result;
}

}  // namespace X3GPIO

namespace {
#if BOARD_NM_EPD_420
constexpr uint8_t kNmHoldPins[] = {NM_PA_CTRL,  NM_LORA_EN,  NM_LORA_NSS,  NM_LORA_SCK, NM_LORA_MOSI,
                                   NM_LORA_MISO, NM_LORA_RST, NM_LORA_BUSY, NM_LORA_DIO1, NM_SD_CS,
                                   NM_EPD_RST,  NM_TEMP_SDA,  NM_TEMP_SCL,  NM_CODEC_EN, NM_ADC_EN,
                                   NM_TEMP_CTL};

constexpr uint8_t kNmHiZSleepPins[] = {
    NM_LORA_NSS, NM_LORA_SCK, NM_LORA_RST, NM_LORA_BUSY, NM_LORA_DIO1, NM_LORA_MISO, NM_LORA_MOSI,
    NM_EPD_BUSY, NM_EPD_CS,   NM_EPD_DC,   NM_EPD_SCK,   NM_EPD_MOSI,  NM_EPD_RST,  NM_TEMP_SCL,
    NM_TEMP_SDA, NM_SD_CS,    NM_I2S_SCLK, NM_I2S_ASDOUT, NM_I2S_LRCK, NM_I2S_DSIN, NM_I2S_MCLK,
    NM_BATT_ADC, NM_USER_BTN};

void releaseNmGpioHolds() {
  gpio_deep_sleep_hold_dis();
  for (const uint8_t pin : kNmHoldPins) {
    gpio_hold_dis(static_cast<gpio_num_t>(pin));
  }
  rtc_gpio_hold_dis(static_cast<gpio_num_t>(NM_LORA_MOSI));
  rtc_gpio_hold_dis(static_cast<gpio_num_t>(NM_LORA_MISO));
  rtc_gpio_deinit(static_cast<gpio_num_t>(NM_LORA_MOSI));
  rtc_gpio_deinit(static_cast<gpio_num_t>(NM_LORA_MISO));
}

void configureNmPeripheralIdlePins() {
  pinMode(NM_LORA_EN, OUTPUT);
  digitalWrite(NM_LORA_EN, LOW);
  pinMode(NM_CODEC_EN, OUTPUT);
  digitalWrite(NM_CODEC_EN, LOW);
  pinMode(NM_ADC_EN, OUTPUT);
  digitalWrite(NM_ADC_EN, LOW);
  pinMode(NM_TEMP_CTL, OUTPUT);
  digitalWrite(NM_TEMP_CTL, LOW);
  pinMode(NM_PA_CTRL, OUTPUT);
  digitalWrite(NM_PA_CTRL, HIGH);

  pinMode(NM_LORA_NSS, INPUT);
  pinMode(NM_LORA_SCK, INPUT);
  pinMode(NM_LORA_MOSI, INPUT);
  pinMode(NM_LORA_MISO, INPUT);
  pinMode(NM_LORA_RST, INPUT);
  pinMode(NM_LORA_BUSY, INPUT);
  pinMode(NM_LORA_DIO1, INPUT);
}

void configureNmPeripheralSleepPins() {
  pinMode(NM_LORA_EN, OUTPUT);
  digitalWrite(NM_LORA_EN, LOW);
  pinMode(NM_CODEC_EN, OUTPUT);
  digitalWrite(NM_CODEC_EN, LOW);
  pinMode(NM_ADC_EN, OUTPUT);
  digitalWrite(NM_ADC_EN, LOW);
  pinMode(NM_TEMP_CTL, OUTPUT);
  digitalWrite(NM_TEMP_CTL, LOW);
  pinMode(NM_PA_CTRL, OUTPUT);
  digitalWrite(NM_PA_CTRL, LOW);
  delay(10);

  for (const uint8_t pin : kNmHiZSleepPins) {
    pinMode(pin, INPUT);
  }

  gpio_hold_en(static_cast<gpio_num_t>(NM_LORA_EN));
  gpio_hold_en(static_cast<gpio_num_t>(NM_CODEC_EN));
  gpio_hold_en(static_cast<gpio_num_t>(NM_ADC_EN));
  gpio_hold_en(static_cast<gpio_num_t>(NM_TEMP_CTL));
  gpio_hold_en(static_cast<gpio_num_t>(NM_PA_CTRL));
  gpio_deep_sleep_hold_en();
}

void configureNmButtonSleepPins() {
  pinMode(NM_USER_BTN, INPUT_PULLUP);
  pinMode(NM_BOOT_BTN, INPUT_PULLUP);
  gpio_pullup_en(static_cast<gpio_num_t>(NM_USER_BTN));
  gpio_pullup_en(static_cast<gpio_num_t>(NM_BOOT_BTN));
  gpio_pulldown_dis(static_cast<gpio_num_t>(NM_USER_BTN));
  gpio_pulldown_dis(static_cast<gpio_num_t>(NM_BOOT_BTN));
}

uint64_t getNmDeepSleepWakeMask() {
  constexpr uint8_t kWakeButtonPins[] = {NM_BOOT_BTN, NM_USER_BTN};
  uint64_t wakeMask = 0;

  for (const uint8_t pin : kWakeButtonPins) {
    const auto gpioNum = static_cast<gpio_num_t>(pin);
    if (esp_sleep_is_valid_wakeup_gpio(gpioNum)) {
      wakeMask |= (1ULL << pin);
    } else {
      LOG_INF("GPIO", "NM deep sleep wake unsupported on GPIO%u", pin);
    }
  }

  return wakeMask;
}
#endif

constexpr char HW_NAMESPACE[] = "cphw";
constexpr char NVS_KEY_DEV_OVERRIDE[] = "dev_ovr";  // 0=auto, 1=x4, 2=x3
constexpr char NVS_KEY_DEV_CACHED[] = "dev_det";    // 0=unknown, 1=x4, 2=x3

enum class NvsDeviceValue : uint8_t { Unknown = 0, X4 = 1, X3 = 2 };

NvsDeviceValue readNvsDeviceValue(const char* key, NvsDeviceValue defaultValue) {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, true)) {
    return defaultValue;
  }
  const uint8_t raw = prefs.getUChar(key, static_cast<uint8_t>(defaultValue));
  prefs.end();
  if (raw > static_cast<uint8_t>(NvsDeviceValue::X3)) {
    return defaultValue;
  }
  return static_cast<NvsDeviceValue>(raw);
}

void writeNvsDeviceValue(const char* key, NvsDeviceValue value) {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, false)) {
    return;
  }
  prefs.putUChar(key, static_cast<uint8_t>(value));
  prefs.end();
}

HalGPIO::DeviceType nvsToDeviceType(NvsDeviceValue value) {
  return value == NvsDeviceValue::X3 ? HalGPIO::DeviceType::X3 : HalGPIO::DeviceType::X4;
}

HalGPIO::DeviceType detectDeviceTypeWithFingerprint() {
  // Explicit override for recovery/support:
  // 0 = auto, 1 = force X4, 2 = force X3
  const NvsDeviceValue overrideValue = readNvsDeviceValue(NVS_KEY_DEV_OVERRIDE, NvsDeviceValue::Unknown);
  if (overrideValue == NvsDeviceValue::X3 || overrideValue == NvsDeviceValue::X4) {
    LOG_INF("HW", "Device override active: %s", overrideValue == NvsDeviceValue::X3 ? "X3" : "X4");
    return nvsToDeviceType(overrideValue);
  }

  const NvsDeviceValue cachedValue = readNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::Unknown);
  if (cachedValue == NvsDeviceValue::X3 || cachedValue == NvsDeviceValue::X4) {
    LOG_INF("HW", "Using cached device type: %s", cachedValue == NvsDeviceValue::X3 ? "X3" : "X4");
    return nvsToDeviceType(cachedValue);
  }

  // No cache yet: run active X3 fingerprint probe and persist result.
  const X3GPIO::X3ProbeResult pass1 = X3GPIO::runX3ProbePass();
  delay(2);
  const X3GPIO::X3ProbeResult pass2 = X3GPIO::runX3ProbePass();

  const uint8_t score1 = pass1.score();
  const uint8_t score2 = pass2.score();
  LOG_INF("HW", "X3 probe scores: pass1=%u(bq=%d rtc=%d imu=%d) pass2=%u(bq=%d rtc=%d imu=%d)", score1, pass1.bq27220,
          pass1.ds3231, pass1.qmi8658, score2, pass2.bq27220, pass2.ds3231, pass2.qmi8658);
  const bool x3Confirmed = (score1 >= 2) && (score2 >= 2);
  const bool x4Confirmed = (score1 == 0) && (score2 == 0);

  if (x3Confirmed) {
    writeNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::X3);
    return HalGPIO::DeviceType::X3;
  }

  if (x4Confirmed) {
    writeNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::X4);
    return HalGPIO::DeviceType::X4;
  }

  // Conservative fallback for first boot with inconclusive probes.
  return HalGPIO::DeviceType::X4;
}

}  // namespace

void HalGPIO::begin() {
#if BOARD_NM_EPD_420
  _deviceType = DeviceType::NmEpd420;
  releaseNmGpioHolds();
  configureNmPeripheralIdlePins();
  pinMode(NM_USER_BTN, INPUT_PULLUP);
  pinMode(NM_BOOT_BTN, INPUT_PULLUP);
  pinMode(NM_EPD_CS, OUTPUT);
  digitalWrite(NM_EPD_CS, HIGH);
  pinMode(NM_SD_CS, OUTPUT);
  digitalWrite(NM_SD_CS, HIGH);
  lastInputUpdateMs = millis();
  update();
#else
  inputMgr.begin();
  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);

  _deviceType = detectDeviceTypeWithFingerprint();

  if (deviceIsX4()) {
    pinMode(BAT_GPIO0, INPUT);
    pinMode(UART0_RXD, INPUT);
  }
#endif
}

void HalGPIO::update() {
#if BOARD_NM_EPD_420
  lastInputUpdateMs = millis();
  const bool userPressed = digitalRead(NM_USER_BTN) == LOW;
  const bool bootPressed = digitalRead(NM_BOOT_BTN) == LOW;
  twoButtonInput.update(userPressed, bootPressed, lastInputUpdateMs);
  usbStateChanged = false;
#else
  inputMgr.update();
  const bool connected = isUsbConnected();
  usbStateChanged = (connected != lastUsbConnected);
  lastUsbConnected = connected;
#endif
}

bool HalGPIO::wasUsbStateChanged() const { return usbStateChanged; }

namespace {
#if BOARD_NM_EPD_420
TwoButtonInputMapper::Button toTwoButton(uint8_t buttonIndex) {
  using Button = TwoButtonInputMapper::Button;
  switch (buttonIndex) {
    case HalGPIO::BTN_BACK:
      return Button::Back;
    case HalGPIO::BTN_CONFIRM:
      return Button::Confirm;
    case HalGPIO::BTN_LEFT:
      return Button::Left;
    case HalGPIO::BTN_RIGHT:
      return Button::Right;
    case HalGPIO::BTN_UP:
      return Button::Up;
    case HalGPIO::BTN_DOWN:
      return Button::Down;
    case HalGPIO::BTN_POWER:
    default:
      return Button::Power;
  }
}
#endif
}  // namespace

bool HalGPIO::isPressed(uint8_t buttonIndex) const {
#if BOARD_NM_EPD_420
  return twoButtonInput.isPressed(toTwoButton(buttonIndex));
#else
  return inputMgr.isPressed(buttonIndex);
#endif
}

bool HalGPIO::wasPressed(uint8_t buttonIndex) const {
#if BOARD_NM_EPD_420
  return twoButtonInput.wasPressed(toTwoButton(buttonIndex));
#else
  return inputMgr.wasPressed(buttonIndex);
#endif
}

bool HalGPIO::wasAnyPressed() const {
#if BOARD_NM_EPD_420
  return twoButtonInput.wasAnyPressed();
#else
  return inputMgr.wasAnyPressed();
#endif
}

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
#if BOARD_NM_EPD_420
  return twoButtonInput.wasReleased(toTwoButton(buttonIndex));
#else
  return inputMgr.wasReleased(buttonIndex);
#endif
}

bool HalGPIO::wasAnyReleased() const {
#if BOARD_NM_EPD_420
  return twoButtonInput.wasAnyReleased();
#else
  return inputMgr.wasAnyReleased();
#endif
}

unsigned long HalGPIO::getHeldTime() const {
#if BOARD_NM_EPD_420
  return twoButtonInput.heldTime(lastInputUpdateMs);
#else
  return inputMgr.getHeldTime();
#endif
}

void HalGPIO::startDeepSleep() {
#if BOARD_NM_EPD_420
  while (digitalRead(NM_USER_BTN) == LOW || digitalRead(NM_BOOT_BTN) == LOW) {
    delay(50);
    update();
  }
  configureNmPeripheralSleepPins();
  configureNmButtonSleepPins();
  Serial.flush();

  const uint64_t wakeMask = getNmDeepSleepWakeMask();
  if (wakeMask == 0) {
    LOG_ERR("GPIO", "No valid NM deep sleep wake pins; sleep cancelled");
    return;
  }

  const esp_err_t wakeErr = esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_LOW);
  if (wakeErr != ESP_OK) {
    LOG_ERR("GPIO", "NM deep sleep wake setup failed: %d", static_cast<int>(wakeErr));
    return;
  }

  esp_deep_sleep_start();
#else
  // Ensure that the power button has been released to avoid immediately turning back on if you're holding it
  while (inputMgr.isPressed(BTN_POWER)) {
    delay(50);
    inputMgr.update();
  }
  // Arm the wakeup trigger *after* the button is released
  esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  // Enter Deep Sleep
  esp_deep_sleep_start();
#endif
}

void HalGPIO::verifyPowerButtonWakeup(uint16_t requiredDurationMs, bool shortPressAllowed) {
#if BOARD_NM_EPD_420
  (void)requiredDurationMs;
  (void)shortPressAllowed;
#else
  if (shortPressAllowed) {
    // Fast path - no duration check needed
    return;
  }
  // TODO: Intermittent edge case remains: a single tap followed by another single tap
  // can still power on the device. Tighten wake debounce/state handling here.

  // Calibrate: subtract boot time already elapsed, assuming button held since boot
  const uint16_t calibration = millis();
  const uint16_t calibratedDuration = (calibration < requiredDurationMs) ? (requiredDurationMs - calibration) : 1;

  const auto start = millis();
  inputMgr.update();
  // inputMgr.isPressed() may take up to ~500ms to return correct state
  while (!inputMgr.isPressed(BTN_POWER) && millis() - start < 1000) {
    delay(10);
    inputMgr.update();
  }
  if (inputMgr.isPressed(BTN_POWER)) {
    do {
      delay(10);
      inputMgr.update();
    } while (inputMgr.isPressed(BTN_POWER) && inputMgr.getHeldTime() < calibratedDuration);
    if (inputMgr.getHeldTime() < calibratedDuration) {
      startDeepSleep();
    }
  } else {
    startDeepSleep();
  }
#endif
}

bool HalGPIO::isUsbConnected() const {
#if BOARD_NM_EPD_420
  return true;
#else
  if (deviceIsX3()) {
    // X3: infer USB/charging via BQ27220 Current() register (0x0C, signed mA).
    // Positive current means charging.
    for (uint8_t attempt = 0; attempt < 2; ++attempt) {
      int16_t currentMa = 0;
      if (X3GPIO::readBQ27220CurrentMA(&currentMa)) {
        return currentMa > 0;
      }
      delay(2);
    }
    return false;
  }
  // U0RXD/GPIO20 reads HIGH when USB is connected
  return digitalRead(UART0_RXD) == HIGH;
#endif
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
#if BOARD_NM_EPD_420
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1 ? WakeupReason::PowerButton : WakeupReason::Other;
#else
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();

  const bool usbConnected = isUsbConnected();

  if ((wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && !usbConnected) ||
      (wakeupCause == ESP_SLEEP_WAKEUP_GPIO && resetReason == ESP_RST_DEEPSLEEP && usbConnected)) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN && usbConnected) {
    return WakeupReason::AfterFlash;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && usbConnected) {
    return WakeupReason::AfterUSBPower;
  }
  return WakeupReason::Other;
#endif
}
