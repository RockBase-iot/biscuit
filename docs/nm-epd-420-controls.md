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
| USER short press | Back / previous item |
| BOOT short press | Confirm / next item |
| USER hold and release | Up / previous page |
| BOOT hold and release | Down / next page |
| USER + BOOT hold | Power action |

Short press means releasing the button before the long-press threshold. Hold means keeping the button down for about 550 ms or longer before release. The USER + BOOT power chord is recognized after both buttons are held together for about 700 ms.

## On-Screen Hints

Compact NM-EPD-420 screens use this bottom hint order:

```text
U hold ^ | USER < | BOOT > | B hold v
```

The labels are intentionally short to fit the 400x300 landscape display:

| Hint | Meaning |
| --- | --- |
| `U hold ^` | Hold USER for Up / previous page |
| `USER <` | Short press USER for Back / previous item |
| `BOOT >` | Short press BOOT for Confirm / next item |
| `B hold v` | Hold BOOT for Down / next page |

## Menu Behavior

In menus, USER and BOOT are designed to keep one-handed navigation usable:

| Screen type | USER short | BOOT short | USER hold | BOOT hold |
| --- | --- | --- | --- | --- |
| Dashboard grid | Move left / previous tile | Enter selected tile | Move up / previous row | Move down / next row |
| Category and list screens | Back or previous item | Open selected item or next item | Up / previous page | Down / next page |
| Reader screens | Previous page / previous action | Next page / open action | Previous page or previous chapter, depending on reader settings | Next page or next chapter, depending on reader settings |

On dashboard and category menus, confirm has priority over same-event navigation. This prevents a BOOT short press from moving the selection before opening the highlighted tile or entry.

## Power and Sleep

The NM-EPD-420 does not have a separate dedicated power button in this port. USER + BOOT acts as the logical Power button:

| Action | Result |
| --- | --- |
| Hold USER + BOOT for about 700 ms | Emits the logical Power button |
| Hold the logical Power button for the configured power duration | Enter deep sleep |
| Press USER or BOOT while sleeping | Wake from deep sleep |

When the power chord is active, single-button USER and BOOT actions are suppressed until both physical buttons are released.

## Implementation Notes

- Debounce threshold: 30 ms.
- Long-press threshold: 550 ms.
- Power chord threshold: 700 ms.
- The mapping lives in `lib/hal/TwoButtonInputMapper.h`.
- NM-specific logical button mapping is handled in `src/MappedInputManager.cpp`.
- NM bottom button hints are drawn by the compact path in `src/components/themes/BaseTheme.cpp`.
