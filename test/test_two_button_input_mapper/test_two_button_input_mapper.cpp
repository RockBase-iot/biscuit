#include <unity.h>

#include <TwoButtonInputMapper.h>

namespace {
using Button = TwoButtonInputMapper::Button;

void advance(TwoButtonInputMapper& mapper, bool user, bool boot, uint32_t fromMs, uint32_t toMs) {
  for (uint32_t now = fromMs; now <= toMs; now += 10) {
    mapper.update(user, boot, now);
  }
}
}  // namespace

void test_boot_short_press_emits_down_and_next_page_without_confirm() {
  TwoButtonInputMapper mapper;

  advance(mapper, false, true, 0, 80);
  advance(mapper, false, false, 120, 150);

  TEST_ASSERT_TRUE(mapper.wasPressed(Button::Down));
  TEST_ASSERT_TRUE(mapper.wasPressed(Button::Right));
  TEST_ASSERT_TRUE(mapper.wasPressed(Button::PageForward));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Confirm));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Back));
}

void test_user_short_press_emits_up_and_previous_page_without_back() {
  TwoButtonInputMapper mapper;

  advance(mapper, true, false, 0, 80);
  advance(mapper, false, false, 120, 150);

  TEST_ASSERT_TRUE(mapper.wasPressed(Button::Up));
  TEST_ASSERT_TRUE(mapper.wasPressed(Button::Left));
  TEST_ASSERT_TRUE(mapper.wasPressed(Button::PageBack));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Back));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Confirm));
}

void test_boot_long_press_emits_confirm_without_navigation() {
  TwoButtonInputMapper mapper;

  advance(mapper, false, true, 0, 700);
  advance(mapper, false, false, 760, 790);

  TEST_ASSERT_TRUE(mapper.wasPressed(Button::Confirm));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Down));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Right));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::PageForward));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Back));
}

void test_user_long_press_emits_back_without_navigation() {
  TwoButtonInputMapper mapper;

  advance(mapper, true, false, 0, 700);
  advance(mapper, false, false, 760, 790);

  TEST_ASSERT_TRUE(mapper.wasPressed(Button::Back));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Up));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Left));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::PageBack));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Confirm));
}

void test_two_button_hold_emits_power_and_suppresses_single_actions() {
  TwoButtonInputMapper mapper;

  advance(mapper, true, true, 0, 680);
  mapper.update(true, true, 720);

  TEST_ASSERT_TRUE(mapper.wasPressed(Button::Power));
  TEST_ASSERT_TRUE(mapper.isPressed(Button::Power));

  advance(mapper, false, false, 960, 990);

  TEST_ASSERT_TRUE(mapper.wasReleased(Button::Power));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Confirm));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Back));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Down));
  TEST_ASSERT_FALSE(mapper.wasPressed(Button::Up));
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_short_press_emits_down_and_next_page_without_confirm);
  RUN_TEST(test_user_short_press_emits_up_and_previous_page_without_back);
  RUN_TEST(test_boot_long_press_emits_confirm_without_navigation);
  RUN_TEST(test_user_long_press_emits_back_without_navigation);
  RUN_TEST(test_two_button_hold_emits_power_and_suppresses_single_actions);
  return UNITY_END();
}
