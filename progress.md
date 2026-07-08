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
- Updated two-button mapping for NM-EPD-420 board structure: BOOT short emits confirm/next cluster, USER short emits back/previous cluster, BOOT long emits down/page-forward, USER long emits up/page-back.
- Tried `pio test -e native -f test_two_button_input_mapper`; it cannot build because `gcc`/`g++` are not available in PATH.
- Tried COM12 upload with `pio run -e nm_epd_420 --target upload --upload-port COM12`; using a short `PLATFORMIO_CORE_DIR` fixed the original Windows path unpack issue, but package downloads repeatedly produced bad/corrupt zip errors before project compilation started.
- Fixed NM compile errors reported by owner: removed invalid `GxEPD2_GFX*` polymorphism from `HalDisplay`, switched NM deep-sleep wake to EXT1 low-level wake, guarded ESP32-C3/RISC-V private panic includes out of NM builds, and added `CROSSPOINT_VERSION` to `env:nm_epd_420`. Did not run compilation per owner request.
- Compared current NM-EPD-420 port with local `RockBase-iot/ESP32-Dashboard` reference. Aligned EPD page height with the reference half-screen page buffer, added the reference peripheral power pins, added GPIO hold release/idle setup/deep-sleep shutdown for external modules, made NM input mapping ignore four-front-button remap settings, removed short-press page-turn aliases, and hid controls that do not apply to the two-button NM layout. Did not run compilation per owner request.
- Fixed the BLE scanner compile error from the updated Arduino BLE client signature by storing each scanned device address type and passing it to `BLEClient::connect()` instead of the old integer wildcard argument. Did not run compilation per owner request.
