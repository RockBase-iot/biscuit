# NM-EPD-420 Port Assessment Progress

## 2026-07-08

- Started assessment in `E:\github\biscuit`.
- Read mandatory repo instructions in `.CLAUDE.md`: do not compile; avoid specified manually maintained wireless files.
- Listed project files and confirmed HAL-style structure with `lib/hal`, app activities, reader modules, and PlatformIO configuration.
- Tried planning catchup script; failed because `python` is not on PATH.
- Cloned `https://github.com/RockBase-iot/NM-EPD-420` into `.codex/tmp_nm_epd_420` for local inspection.
- Read NM-EPD-420 README, `platformio.ini`, `src/config.h`, and EPD helper headers.
- Read Biscuit HAL display/input/storage/power/system code, renderer orientation code, reader input utilities, screenshot utility, and architecture/scope docs.
- Recorded final compatibility analysis and porting checklist in `task_plan.md` and `findings.md`.
- Added `env:nm_epd_420` to `platformio.ini`.
- Added NM-EPD-420 pin definitions, UC8179 display driver files, conditional display HAL, conditional SdFat storage HAL, conditional two-button GPIO/input path, battery ADC handling, and ESP32-S3-safe panic wrapper behavior.
- Added native unit test source for `TwoButtonInputMapper`, but did not run it because project instructions forbid `pio test`.
- Updated two-button mapping for NM-EPD-420 board structure: BOOT short emits down/next/page-forward, USER short emits up/previous/page-back, BOOT long emits confirm/open, USER long emits back.
- Tried `pio test -e native -f test_two_button_input_mapper`; it cannot build because `gcc`/`g++` are not available in PATH.
- Tried COM12 upload with `pio run -e nm_epd_420 --target upload --upload-port COM12`; using a short `PLATFORMIO_CORE_DIR` fixed the original Windows path unpack issue, but package downloads repeatedly produced bad/corrupt zip errors before project compilation started.
- Fixed NM compile errors reported by owner: removed invalid `GxEPD2_GFX*` polymorphism from `HalDisplay`, switched NM deep-sleep wake to EXT1 low-level wake, guarded ESP32-C3/RISC-V private panic includes out of NM builds, and added `CROSSPOINT_VERSION` to `env:nm_epd_420`. Did not run compilation per owner request.
- Compared current NM-EPD-420 port with local `RockBase-iot/ESP32-Dashboard` reference. Aligned EPD page height with the reference half-screen page buffer, added the reference peripheral power pins, added GPIO hold release/idle setup/deep-sleep shutdown for external modules, made NM input mapping ignore four-front-button remap settings, removed short-press page-turn aliases, and hid controls that do not apply to the two-button NM layout. Did not run compilation per owner request.
- Fixed the BLE scanner compile error from the updated Arduino BLE client signature by storing each scanned device address type and passing it to `BLEClient::connect()` instead of the old integer wildcard argument. Did not run compilation per owner request.
- Fixed the NM-EPD-420 FreeRTOS port macro configuration error by aligning the PlatformIO board target with the ESP32-Dashboard reference: switched `env:nm_epd_420` to `esp32-s3-devkitc-1`, removed the LilyGo-specific `qio_opi` memory profile, and set the CPU frequency to 80MHz. Did not run compilation per owner request.
- Fixed the remaining NM-EPD-420 FreeRTOS port macro error by ensuring activity headers include `freertos/FreeRTOS.h` before `freertos/portmacro.h`. Also updated `ScanActivity` for the Arduino BLE 3.x API: use the pointer-to-address-array returned by `BLEAddress::getNative()`, remove the missing `BLEScan::isScanning()` call, and start the BLE phase with the non-blocking scan overload. Did not run compilation per owner request.
- Fixed NM-EPD-420 link errors by defining the `MySerialImpl` serial wrapper symbols in `Logging.cpp` and adding a `lib/hal/library.json` source filter so PlatformIO compiles the custom UC8179 panel driver in `lib/hal/nm_epd_420`. Did not run compilation per owner request.
- Fixed the NM-EPD-420 program-size check failure by replacing Arduino's 3MB `huge_app.csv` with a board-specific 16MB flash partition table that provides a 12MB app slot, while retaining a SPIFFS data partition and coredump area. Did not run compilation per owner request.
- Investigated the NM-EPD-420 physical display abnormality and reboot log. The GFX outside-range pattern matched the renderer's portrait transform on a 400x300 panel, where logical x values over 299 become negative physical y values.
- Adjusted NM-EPD-420 UI orientation and layout: default non-reader screens now return to native landscape, AppsMenu uses a compact 4x2 grid, tile/status text is truncated to fit, theme button hints use a compact 4-slot bottom bar, side hints are suppressed on NM, Lyra/Noir list icons use 24px on NM, and the NM PlatformIO environment no longer defines PSRAM after the attached log showed PSRAM initialization failure.
- Ran `git diff --check` only; no PlatformIO build/test was run because `.CLAUDE.md` forbids build commands in this repo.

## 2026-07-09

- Fine-tuned the NM-EPD-420 bottom button hint row for the physical bezel: moved the compact hint bar upward, reserved more AppsMenu bottom space, and changed the NM AppsMenu labels to `U hold < | USER ^ | BOOT v | B hold >`.
- Reworked the requested two-button semantics so short USER/BOOT presses navigate naturally and long USER/BOOT presses provide back/confirm. Short presses also keep previous/next aliases for screens that use horizontal navigation.
- Fixed NM-EPD-420 deep sleep wake setup by filtering wake GPIOs through `esp_sleep_is_valid_wakeup_gpio()` before enabling EXT1 wake. This keeps BOOT/GPIO0 wake armed and avoids USER/GPIO45 invalidating the wake configuration.
- Adjusted the NM-EPD-420 AppsMenu layout from device photo feedback: moved the status row up, moved the tile grid down, centered compact hint text better, and made short USER/BOOT navigation move linearly through the 8 dashboard tiles.
- Simplified the NM-EPD-420 AppsMenu tile rendering to avoid text overlap: compact 4x2 tiles now draw centered title, centered count, and a single selected-status line instead of title + subtitle + badge + count + status.
- Lowered the compact NM bottom button hint bar while keeping a small bottom safety margin.
- Investigated the attached `spinlock_acquire spinlock.h:84 (lock)` reset log. Found ESP32-S3-incompatible `taskENTER_CRITICAL(nullptr)` calls in `ActivityManager`; replaced them with a real `portMUX_TYPE` guard for the render-wait state.
- Reduced render/log contention on NM-EPD-420 by disabling GxEPD2 diagnostic serial output and skipping periodic MEM log prints while the render lock is busy.
- Checked the current NM display drivers for fast partial refresh: UC8179 has no partial/fast partial support, and GDEY042Z98 has partial RAM addressing but still performs full-screen refresh; no button-click partial refresh path was enabled.
- Static verification only; no PlatformIO build/test was run because `.CLAUDE.md` forbids build commands in this repo.

## 2026-07-09 Controls Documentation

- Added `docs/nm-epd-420-controls.md` as the canonical NM-EPD-420 USER/BOOT operation guide.
- Updated `USER_GUIDE.md` with the NM two-button layout, dashboard/list notes, and reader-mode notes.
- Updated `README.md` with a short NM-EPD-420 port controls table and a link to the full control guide.

## 2026-07-09 SD Card Diagnostics

- Investigated NM-EPD-420 SD access showing `No files found` with no SD-related serial logs.
- Compared the local `NM-Display-420` SD test: it parks LoRa NSS high before SD access and initializes the shared
  peripheral SPI bus without assigning CS to `SPI.begin()`.
- Updated NM GPIO idle handling to keep LoRa NSS high, so the LoRa device does not contend with SD on the shared
  SCK/MOSI/MISO bus.
- Updated NM `HalStorage::begin()` to log SD pin/frequency settings, mount failure error/data, card/volume info, and
  root-directory readiness; the NM SD bus now begins without passing CS and uses `SHARED_SPI`.
- Added diagnostics in `FileBrowserActivity` and `SdFileBrowserActivity` so empty screens report whether the directory
  could not be opened or whether all entries were hidden/unsupported after filtering.
- No PlatformIO build/test was run because `.CLAUDE.md` forbids build commands in this repo.

## 2026-07-09 TXT Reader Watchdog

- Investigated the reboot while opening a 217925 byte Chinese TXT file on NM-EPD-420. The log reaches `Building page
  index...` and then the task watchdog reports CPU0 running `ActivityManager`, matching long synchronous TXT page-index
  work inside the render task.
- Updated TXT wrapping so long no-space UTF-8 lines use a boundary-safe binary search for the visible prefix instead of
  decrementing one byte/character at a time and remeasuring every substring.
- Added timed watchdog/CPU servicing during TXT page-index building and page loading so long SD reads, width measurements,
  and wrapping loops yield to other FreeRTOS tasks.
- Static verification only; no PlatformIO build/test was run because `.CLAUDE.md` forbids build commands in this repo.

## 2026-07-09 TXT Reader Retry Fixes

- Investigated the follow-up log where `INDEXING` appeared portrait and `esp_task_wdt_reset()` printed `task not found`.
- Removed the direct `esp_task_wdt_reset()` call from TXT indexing because the render task is not subscribed to the task
  watchdog; retained timed `vTaskDelay(1)` servicing so the idle task can run.
- Forced NM-EPD-420 reader orientation through `ReaderUtils` to native landscape for TXT and EPUB content, including the
  indexing popup and EPUB reflow after menu actions.
- Hid the device UI orientation setting and EPUB reader rotate-screen menu item on NM-EPD-420 because the board now uses
  a fixed landscape reader layout.
- Static verification only; no PlatformIO build/test was run because `.CLAUDE.md` forbids build commands in this repo.
