# NM-EPD-420 Controls

This page documents the Biscuit input mapping for the NM-EPD-420 port. The board has two built-in buttons:

| Physical button | GPIO | Electrical level |
| --- | --- | --- |
| USER | GPIO45 | Active low |
| BOOT | GPIO0 | Active low |

The firmware maps these two buttons onto the larger Biscuit navigation model.

## Quick Reference

| Action | Meaning |
| --- | --- |
| USER short press | Up / previous item / previous page |
| BOOT short press | Down / next item / next page |
| USER hold and release | Back |
| BOOT hold and release | Confirm / open |
| USER + BOOT hold | Power action |

Short press means releasing the button before the long-press threshold. Hold means keeping the button down for about 550 ms or longer before release. The USER + BOOT power chord is recognized after both buttons are held together for about 700 ms.

## On-Screen Hints

Compact NM-EPD-420 screens use this bottom hint order:

```text
U hold < | USER ^ | BOOT v | B hold >
```

The labels are intentionally short to fit the 400x300 landscape display:

| Hint | Meaning |
| --- | --- |
| `U hold <` | Hold USER for Back |
| `USER ^` | Short press USER for Up / previous item / previous page |
| `BOOT v` | Short press BOOT for Down / next item / next page |
| `B hold >` | Hold BOOT for Confirm / open |

## Menu Behavior

In menus, USER and BOOT are designed to keep one-handed navigation usable:

| Screen type | USER short | BOOT short | USER hold | BOOT hold |
| --- | --- | --- | --- | --- |
| Dashboard grid | Previous tile | Next tile | Back action is ignored on the dashboard | Enter selected tile |
| Category and list screens | Previous item | Next item | Back one level | Open selected item |
| Reader screens | Previous page / previous action | Next page / next action | Back to menu or previous screen | Open menu / confirm action |

Short USER/BOOT presses also emit the logical previous/next aliases used by older screens, so confirmation dialogs and horizontal controls can keep using the same two physical buttons.

## Power and Sleep

The NM-EPD-420 does not have a separate dedicated power button in this port. USER + BOOT acts as the logical Power button:

| Action | Result |
| --- | --- |
| Hold USER + BOOT for about 700 ms | Emits the logical Power button |
| Hold the logical Power button for the configured power duration | Enter deep sleep |
| Press BOOT while sleeping | Wake from deep sleep |

BOOT is GPIO0, which is an ESP32-S3 deep-sleep wake-capable GPIO. USER is GPIO45, which is not in the ESP32-S3 deep-sleep wake GPIO range, so USER cannot wake the device from deep sleep in this hardware wiring. The firmware filters unsupported wake pins before sleeping so USER no longer prevents BOOT wake from being configured.

When the power chord is active, single-button USER and BOOT actions are suppressed until both physical buttons are released.

## Implementation Notes

- Debounce threshold: 30 ms.
- Long-press threshold: 550 ms.
- Power chord threshold: 700 ms.
- The mapping lives in `lib/hal/TwoButtonInputMapper.h`.
- NM-specific logical button mapping is handled in `src/MappedInputManager.cpp`.
- NM bottom button hints are drawn by the compact path in `src/components/themes/BaseTheme.cpp`.
- Deep sleep wake setup lives in `lib/hal/HalGPIO.cpp` and only arms GPIOs accepted by `esp_sleep_is_valid_wakeup_gpio()`.
