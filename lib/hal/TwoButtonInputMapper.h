#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

class TwoButtonInputMapper {
 public:
  enum class Button : uint8_t { Back = 0, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward, Count };

  static constexpr uint16_t DEBOUNCE_MS = 30;
  static constexpr uint16_t LONG_PRESS_MS = 550;
  static constexpr uint16_t CHORD_POWER_MS = 700;

  void update(bool userPressed, bool bootPressed, uint32_t nowMs) {
    pressed.fill(false);
    released.fill(false);

    const bool userReleased = updatePhysical(user, userPressed, nowMs);
    const bool bootReleased = updatePhysical(boot, bootPressed, nowMs);

    if (user.stablePressed && boot.stablePressed &&
        (nowMs - std::max(user.pressedSinceMs, boot.pressedSinceMs)) >= CHORD_POWER_MS) {
      if (!powerChordActive) {
        emitPress(Button::Power);
        powerChordActive = true;
      }
      current[index(Button::Power)] = true;
      suppressSinglesUntilBothReleased = true;
      return;
    }

    const bool releasedPowerChord = powerChordActive && (!user.stablePressed || !boot.stablePressed);
    if (releasedPowerChord) {
      emitRelease(Button::Power);
      powerChordActive = false;
    }

    if (!suppressSinglesUntilBothReleased && !releasedPowerChord) {
      if (userReleased) {
        emitUserRelease(nowMs);
      }
      if (bootReleased) {
        emitBootRelease(nowMs);
      }
    }

    if (!user.stablePressed && !boot.stablePressed) {
      suppressSinglesUntilBothReleased = false;
    }
  }

  bool wasPressed(Button button) const { return pressed[index(button)]; }
  bool wasReleased(Button button) const { return released[index(button)]; }
  bool isPressed(Button button) const { return current[index(button)]; }
  bool wasAnyPressed() const {
    for (bool value : pressed) {
      if (value) return true;
    }
    return false;
  }
  bool wasAnyReleased() const {
    for (bool value : released) {
      if (value) return true;
    }
    return false;
  }

  uint32_t heldTime(uint32_t nowMs) const {
    uint32_t held = 0;
    if (user.stablePressed) held = std::max(held, nowMs - user.pressedSinceMs);
    if (boot.stablePressed) held = std::max(held, nowMs - boot.pressedSinceMs);
    return held;
  }

 private:
  struct PhysicalButton {
    bool rawPressed = false;
    bool stablePressed = false;
    uint32_t lastRawChangeMs = 0;
    uint32_t pressedSinceMs = 0;
    uint32_t releasedDurationMs = 0;
  };

  static constexpr size_t BUTTON_COUNT = static_cast<size_t>(Button::Count);

  PhysicalButton user;
  PhysicalButton boot;
  std::array<bool, BUTTON_COUNT> current{};
  std::array<bool, BUTTON_COUNT> pressed{};
  std::array<bool, BUTTON_COUNT> released{};
  bool powerChordActive = false;
  bool suppressSinglesUntilBothReleased = false;

  static constexpr size_t index(Button button) { return static_cast<size_t>(button); }

  bool updatePhysical(PhysicalButton& button, bool rawPressed, uint32_t nowMs) {
    if (rawPressed != button.rawPressed) {
      button.rawPressed = rawPressed;
      button.lastRawChangeMs = nowMs;
    }

    if ((nowMs - button.lastRawChangeMs) < DEBOUNCE_MS || button.stablePressed == button.rawPressed) {
      return false;
    }

    button.stablePressed = button.rawPressed;
    if (button.stablePressed) {
      button.pressedSinceMs = nowMs;
      button.releasedDurationMs = 0;
      return false;
    }

    button.releasedDurationMs = nowMs - button.pressedSinceMs;
    return true;
  }

  void emitClick(Button button) {
    pressed[index(button)] = true;
    released[index(button)] = true;
    current[index(button)] = false;
  }

  void emitPress(Button button) {
    pressed[index(button)] = true;
    current[index(button)] = true;
  }

  void emitRelease(Button button) {
    released[index(button)] = true;
    current[index(button)] = false;
  }

  void emitUserRelease(uint32_t nowMs) {
    (void)nowMs;
    if (user.releasedDurationMs >= LONG_PRESS_MS) {
      emitClick(Button::Confirm);
      return;
    }
    emitClick(Button::Down);
    emitClick(Button::Right);
    emitClick(Button::PageForward);
  }

  void emitBootRelease(uint32_t nowMs) {
    (void)nowMs;
    if (boot.releasedDurationMs >= LONG_PRESS_MS) {
      emitClick(Button::Back);
      return;
    }
    emitClick(Button::Up);
    emitClick(Button::Left);
    emitClick(Button::PageBack);
  }
};
