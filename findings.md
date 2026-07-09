# NM-EPD-420 Port Assessment Findings

## Local Biscuit Project

- Firmware targets Xteink X4, ESP32-C3, Arduino framework via PlatformIO.
- Existing board config uses `esp32-c3-devkitm-1`, 16 MB flash, USB CDC on boot, Arduino framework, and custom partition table.
- Current hardware assumptions from `.CLAUDE.md` and `README.md`: ESP32-C3, 380 KB SRAM, no PSRAM, 16 MB flash, 4.26 inch 800x480 1-bit e-ink display, 7 physical buttons, WiFi/BLE, MicroSD over SPI.
- Hardware-facing dependencies are symlinked from `open-x4-sdk`: `BatteryMonitor`, `InputManager`, `EInkDisplay`, and `SDCardManager`.
- HAL abstractions exist under `lib/hal`: display, GPIO, storage, power, system.

## Target Hardware

- NM-EPD-420 factory-test firmware targets ESP32-S3 with 16 MB flash and PSRAM, Arduino framework, PlatformIO board `lilygo-t-display-s3`, and `huge_app.csv`.
- Display is a 4.2 inch 400x300 GDEY042Z98 3-color B/W/R e-paper panel on SPI with pins SCK=2, MOSI=1, CS=46, DC=4, RST=5, BUSY=6, MISO unused.
- Display stack uses GxEPD2 3-color classes plus a local `GxEPD2_420c_NM_UC8179` panel implementation; auto-detect switches between SSD1683-style and UC8179-style behavior.
- SD card is on a second SPI bus: SCK=9, MOSI=10, MISO=11, CS=7. LoRa shares the same bus using CS=8, RST=12, BUSY=13, DIO1=14, EN=47.
- Board has only two built-in buttons exposed in the factory firmware: USER on GPIO45 and BOOT on GPIO0, active low.
- Battery sensing uses enable GPIO43 and ADC GPIO3, divider ratio 2, with sampling/settle parameters defined in target `config.h`.
- Additional peripherals exist but are not necessary for a reader-first port: ES8311 codec on I2C/I2S, LMD4737 DMIC, AHT20 temperature/humidity sensor, external audio amp.

## Risks To Verify

- Display controller, SPI pinout, resolution, rotation, color depth, and refresh behavior.
- Input model and button count versus existing 7-button navigation mapping.
- Storage availability: MicroSD versus internal flash/LittleFS.
- Power/battery telemetry support and sleep/wake behavior.
- Flash size and RAM/PSRAM budget versus current firmware footprint.
- NM-EPD-420 UI orientation must stay in native 400x300 landscape for non-reader screens. The old default portrait
  transform maps logical `(x,y)` to physical `(y, height - 1 - x)`, which makes logical x values over 299 produce
  negative physical y values on the 400x300 panel.
- Theme button hints used fixed X4 portrait button coordinates and temporarily forced portrait orientation; on NM this
  can draw outside the panel even after the main activity layout is made landscape.
- ESP32-S3 / Arduino 3 / ESP-IDF 5 does not tolerate `taskENTER_CRITICAL(nullptr)`. The attached
  `spinlock_acquire spinlock.h:84 (lock)` reset after AppsMenu refresh matches the null critical-lock calls in
  `ActivityManager`; a real `portMUX_TYPE` is required.
- GxEPD2 diagnostic serial output during display refresh can interleave with project logging. NM should initialize the
  panel driver with diagnostic baud disabled and avoid periodic MEM logging while the render lock is busy.
- NM-EPD-420 should not currently use fast partial refresh for button feedback. The local UC8179 driver declares
  `hasPartialUpdate = false` and `hasFastPartialUpdate = false`; the GxEPD2 SSD1683/GDEY042Z98 driver supports partial
  RAM window addressing, but its `refresh(x,y,w,h)` still calls full-screen update and `hasFastPartialUpdate = false`.
- NM-EPD-420 SD and LoRa share the peripheral SPI bus. The local `NM-Display-420` SD test parks LoRa NSS high before SD
  access, initializes the SPI bus without passing CS to `SPI.begin()`, and lets the SD driver own CS. Biscuit's NM port
  previously left LoRa NSS high-Z and had no diagnostics for mount/root/list failures, which explains a `No files found`
  UI with no SD-related serial evidence.
- TXT reader indexing runs from the render task on first open. For Chinese TXT files with long no-space lines, the old
  wrap routine repeatedly measured progressively shorter substrings and could keep CPU0 busy long enough to trip the
  task watchdog while building the page index.
- On NM-EPD-420, TXT/EPUB reader activities must not follow the saved X4-style portrait reading orientation. The attached
  retry log showed `Viewport: 284x364`, proving the reader was still portrait while the rest of the NM UI is landscape.
- `esp_task_wdt_reset()` logs `task not found` when called from a task that is not subscribed to the ESP task watchdog.
  Yielding with `vTaskDelay(1)` is the safer servicing mechanism here because the observed watchdog pressure is caused by
  starving the subscribed idle task, not by the render task needing to reset its own watchdog entry.

## Compatibility Analysis

### Good News

- The app architecture is already activity-driven and hardware access is mostly funneled through `HalDisplay`, `HalGPIO`, `HalStorage`, and `HalPowerManager`.
- `GfxRenderer` reads runtime panel dimensions from `HalDisplay` during `begin()`, so many activities using `renderer.getScreenWidth()` / `getScreenHeight()` should scale automatically after the display backend is replaced.
- NM-EPD-420 has more compute headroom than X4: ESP32-S3, dual-core class MCU, PSRAM, and 16 MB flash.
- NM-EPD-420 has microSD, WiFi, and BLE, so the core reader/storage/network model can survive.

### Major Mismatches

- Biscuit currently depends on `open-x4-sdk` libraries for display, input, SD, and battery. The local submodule path exists in `.gitmodules`, but `open-x4-sdk/libs` was not present in this checkout.
- The display backend assumes an `EInkDisplay` class with a 1-bit framebuffer and fast/half/full refresh modes. NM-EPD-420 uses GxEPD2 3-color panel classes and exposes no partial refresh support in its custom UC8179 class.
- Existing UX assumes 7 physical buttons: Back, Confirm, Left, Right, Up, Down, Power. NM-EPD-420 factory firmware exposes USER and BOOT only.
- NM-EPD-420 control docs now live in `docs/nm-epd-420-controls.md`: USER short = up/previous/page-back,
  BOOT short = down/next/page-forward, USER hold = back, BOOT hold = confirm/open, USER+BOOT hold = logical Power.
- NM-EPD-420 deep sleep wake cannot use USER/GPIO45 on ESP32-S3 because deep-sleep wake GPIOs are limited to
  RTC-capable pins. BOOT/GPIO0 is wake-capable and must be armed without GPIO45 in the EXT1 wake mask.
- Existing `GfxRenderer` orientation comments and some utilities still describe 480x800 / 800x480 logical modes. They may compute correctly from runtime dimensions, but the 400x300 UX must be verified screen-by-screen.
- `HalPowerManager` currently contains X4-specific power latch behavior and X3/X4 battery detection. NM-EPD-420 needs an ADC-enable sampling path using GPIO43/GPIO3.
- `HalSystem` panic wrapping copies ESP32-C3/RISC-V backtrace behavior; ESP32-S3 uses a different CPU architecture, so this must be conditional or disabled initially.
- Current partition table gives each OTA app slot about 6.25 MB. Biscuit is already flash-heavy on X4; NM-EPD-420's example uses `huge_app.csv`, so OTA support versus app-size budget must be decided early.

## Work Checklist

### Phase A: Board Configuration

- Add `BOARD_NM_EPD_420` build macro and `[env:nm_epd_420]` in `platformio.ini`.
- Use ESP32-S3 board settings, PSRAM flags, flash mode, and memory type from the target repo as a starting point.
- Decide partition strategy: keep OTA and reduce features, or use a huge single app for first bring-up.
- Vendor or copy NM-EPD-420 EPD support files into a local board-support library.

### Phase B: Display

- Replace or conditionalize `HalDisplay` so NM-EPD-420 uses GxEPD2 and the custom UC8179 class.
- Provide the same public methods expected by Biscuit: framebuffer access, clear, draw image, transparent image, refresh modes, grayscale copy/cleanup, deep sleep.
- Decide whether red channel is ignored initially, mapped as an accent later, or disabled entirely for speed.
- Re-test orientation transform, screenshot export, image decoding, and direct framebuffer writers against 400x300.
- Treat partial/fast refresh as best-effort aliases to full refresh until hardware behavior is proven.

### Phase C: Input

- Implement a two-button `HalGPIO`/input adapter for USER and BOOT.
- Define a usable navigation policy: short/long press, chord, or require external keys.
- Keep `USER short`, `BOOT short`, `USER hold`, `BOOT hold`, and `USER+BOOT hold` documentation in sync with `TwoButtonInputMapper`.
- Disable or hide button remap settings if only two physical buttons are present.
- Revisit text entry and games, because many flows assume directional input.

### Phase D: Storage

- Replace `SDCardManager` dependency with Arduino `SD` or a thin wrapper over NM-EPD-420's HSPI bus.
- Initialize SD on SCK=9, MOSI=10, MISO=11, CS=7 without conflicting with EPD SPI.
- Preserve `HalFile` and `Storage` API so reader caches and settings keep working.

### Phase E: Power and System

- Implement battery percentage from GPIO43/GPIO3 ADC sampling.
- Replace X4 latch and USB-detect behavior with NM-EPD-420-specific sleep/wake policy.
- Conditionalize ESP32-C3-specific panic stack capture in `HalSystem`.
- Ensure CPU frequency and WiFi/BLE behavior are valid on ESP32-S3.

### Phase F: Feature Triage and UX

- Bring up reader, file browser, settings, WiFi transfer, and basic tools first.
- Audit home/dashboard and list layouts at 400x300.
- Disable or postpone flows that depend on many buttons, frequent refresh, or very dense screens.
- Add `BOARD_NM_EPD_420` guards for optional audio, sensor, and LoRa features only after the base firmware works.

### Phase G: Verification

- Compile only when the owner is ready; repo instructions say not to run build commands here.
- Hardware smoke tests: boot screen, full white/black render, SD mount, two-button input, battery reading, sleep/wake.
- App smoke tests: settings load/save, file browser, open EPUB/TXT, page turn, WiFi setup, web transfer.
- Performance checks: page turn latency, heap high-water mark, refresh timing, SD contention with EPD SPI.
