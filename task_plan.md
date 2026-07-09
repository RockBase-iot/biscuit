# NM-EPD-420 Port Assessment Plan

Goal: Analyze the local biscuit firmware and evaluate the work required to port it from Xteink X4 to NM-EPD-420.

## Phases

| Phase | Status | Notes |
| --- | --- | --- |
| 1. Project constraints | complete | Read repo instructions, build config, architecture, and HAL boundaries. |
| 2. Target hardware facts | complete | Read RockBase-iot/NM-EPD-420 docs and source examples. |
| 3. Compatibility analysis | complete | Compared MCU, display, input, storage, power, radio, flash, and boot/upload assumptions. |
| 4. Porting checklist | complete | Produced implementation work list, risks, and suggested order. |
| 5. Next Version: LoRa Support | pending | Evaluate LoRa support on NM-EPD-420 and plan for future porting. |

## Porting Verdict

Feasible, but it is a board-support/HAL port with UI adaptation, not a simple pin-map change.

Primary blockers:

1. Replace Xteink X4 hardware SDK dependencies with NM-EPD-420 equivalents.
2. Adapt display backend from 800x480 1-bit X4 display to 400x300 3-color GDEY042Z98/UC8179.
3. Redesign input mapping for 2 buttons, or require an external input add-on.
4. Retarget power, battery, sleep, and panic/backtrace code for ESP32-S3.
5. Re-evaluate feature set and layout density for smaller screen and slower 3-color refresh.

## Recommended Work Order

1. Add an `nm-epd-420` PlatformIO environment and board feature macros.
2. Introduce a board abstraction layer that can select X4 or NM-EPD-420 at compile time.
3. Port display first and bring up a black/white full-screen render path.
4. Port storage and boot sequence next so settings/cache paths work.
5. Port input with a minimal 2-button navigation policy.
6. Port battery/sleep/USB detection and remove X4-specific latch assumptions.
7. Audit UI density and reader layout on 400x300.
8. Only then enable optional peripherals such as sensor, audio, LoRa, and red channel.

## Implementation Progress

| Item | Status | Notes |
| --- | --- | --- |
| PlatformIO NM environment | complete | Added `env:nm_epd_420` with ESP32-S3, PSRAM, GxEPD2, and SdFat dependencies. |
| Board pin map | complete | Added `NmEpd420Pins.h` for EPD, SD, USER/BOOT, and battery ADC pins. |
| Display HAL | complete | Added conditional GxEPD2 backend with 400x300 1-bit framebuffer and local UC8179 driver. |
| Two-button input | complete | Added `TwoButtonInputMapper` and conditional `HalGPIO` integration. |
| Storage HAL | complete | Added conditional SdFat/HSPI implementation for NM SD card. |
| Power/system | complete | Added NM ADC battery path, simple deep sleep wake, and ESP32-S3-safe panic wrapper behavior. |
| Landscape UI pass | complete | Set NM non-reader UI to native landscape, changed AppsMenu to 4x2 compact layout, and added NM-safe compact button hints. |
| NM two-button UX pass | complete | Bottom hints now match USER/BOOT short/hold behavior, and menu screens prioritize BOOT short confirm over same-event navigation. |
| NM render/reset stabilization | complete | Replaced ESP32-S3-incompatible null FreeRTOS critical locks and reduced serial/log activity during display refresh. |
| Verification | blocked | Repo instructions prohibit PlatformIO build/test; only static checks were run. |

## Constraints

- Do not run PlatformIO build or test commands; project owner builds manually.
- Avoid editing or reading manually maintained high-risk wireless activity files named in `.CLAUDE.md`.
- PlatformIO build/test remains owner-run only.

## Errors Encountered

| Error | Attempt | Resolution |
| --- | --- | --- |
| `python` not found in PATH | Tried planning-with-files catchup script | Logged environment issue and continued with manual project inspection. |
| Build/test forbidden by repo rules | Needed for compile validation | Added test source and ran static checks only; owner must run PlatformIO validation. |
| Native mapper test cannot start | `pio test -e native -f test_two_button_input_mapper` | PlatformIO reaches build stage but host lacks `gcc`/`g++` in PATH. |
| COM12 upload blocked before compile | `pio run -e nm_epd_420 --target upload --upload-port COM12` | Long user-profile PlatformIO path failed unpacking; short `PLATFORMIO_CORE_DIR` fixed that, but repeated GitHub package downloads ended as corrupt/bad zip before project compilation. |
| NM AppsMenu display overflow | Attached boot log showed many GFX outside-range messages after entering AppsMenu | Root cause was portrait coordinate rotation plus fixed X4-style button hint coordinates on a 400x300 panel; NM UI now defaults to native landscape and uses compact hints. |
| NM reset after AppsMenu refresh | Attached boot log ended in `spinlock_acquire spinlock.h:84 (lock)` | Root cause matched `taskENTER_CRITICAL(nullptr)` / `taskEXIT_CRITICAL(nullptr)` on ESP32-S3; replaced with a real `portMUX_TYPE` and reduced render-time serial logging. |
