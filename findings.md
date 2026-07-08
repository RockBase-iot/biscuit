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
