// Unit tests for MenuSystem.
//
// Two classes of bug matter here. Navigation bugs are merely annoying — a
// cursor that wraps past "Retour", or lands on a greyed-out entry. Binding bugs
// are not: every entry writes straight into the persisted DryerSettings record
// through a void*, so a type that does not match the field it points at
// silently corrupts the field next to it.

#include <unity.h>
#include <string.h>

#include "MenuSystem.h"

void MenuSetRtcAvailable(bool available);

namespace
{

DryerSettings g_test_settings;
int           g_change_count = 0;

void CountChange() { g_change_count++; }

// Opens the menu with a known configuration.
void Prepare(MenuSystem &menu, bool rtc_available)
{
  g_test_settings.Reset();
  g_change_count = 0;
  MenuSetRtcAvailable(rtc_available);
  menu.Begin(&g_test_settings);
  menu.SetOnChange(CountChange);
  menu.Open();
}

// Walks the cursor to the entry with the given label, returning false if it is
// unreachable — which is itself a meaningful failure.
bool SelectLabel(MenuSystem &menu, const char *label)
{
  const MenuPage *page = menu.GetCurrentPage();
  for (uint8_t i = 0; i < page->count * 2; i++)
  {
    if (strcmp(page->items[menu.GetCursor()].label, label) == 0)
    {
      return true;
    }
    menu.HandleRotation(1);
  }
  return false;
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

// --- Navigation -------------------------------------------------------------

void test_menu_opens_on_the_root_page(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(menu.IsOpen());
  TEST_ASSERT_EQUAL_STRING("Menu", menu.GetCurrentPage()->title);
  TEST_ASSERT_EQUAL_UINT8(0, menu.GetCursor());
}

void test_cursor_stops_at_the_ends_instead_of_wrapping(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  // Wrapping in a long list makes it far too easy to shoot past "Retour".
  menu.HandleRotation(-5);
  TEST_ASSERT_EQUAL_UINT8(0, menu.GetCursor());

  menu.HandleRotation(50);
  TEST_ASSERT_EQUAL_UINT8(menu.GetCurrentPage()->count - 1, menu.GetCursor());
}

void test_submenu_entry_and_back(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Consignes"));
  menu.HandleClick();
  TEST_ASSERT_EQUAL_STRING("Consignes", menu.GetCurrentPage()->title);

  TEST_ASSERT_TRUE(SelectLabel(menu, "< Retour"));
  menu.HandleClick();
  TEST_ASSERT_EQUAL_STRING("Menu", menu.GetCurrentPage()->title);
  TEST_ASSERT_TRUE(menu.IsOpen());
}

void test_back_on_the_root_page_closes_the_menu(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(SelectLabel(menu, "< Retour"));
  menu.HandleClick();
  TEST_ASSERT_FALSE(menu.IsOpen());
}

void test_eco_entries_are_greyed_out_without_an_rtc(void)
{
  MenuSystem menu;
  Prepare(menu, false);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Mode ECO"));
  menu.HandleClick();

  const MenuPage *page = menu.GetCurrentPage();
  TEST_ASSERT_EQUAL_STRING("Mode ECO", page->title);

  // Every ECO entry needs a wall clock, so all of them are unavailable and only
  // "Retour" can be selected — the page is shown rather than hidden so the
  // menu keeps the same shape whatever the hardware.
  for (uint8_t i = 0; i < page->count; i++)
  {
    bool selectable = menu.IsItemSelectable(page->items[i]);
    if (page->items[i].kind == MenuItemKind::kBack)
    {
      TEST_ASSERT_TRUE(selectable);
    }
    else
    {
      TEST_ASSERT_FALSE(selectable);
    }
  }
}

void test_cursor_skips_unavailable_entries(void)
{
  MenuSystem menu;
  Prepare(menu, false);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Mode ECO"));
  menu.HandleClick();

  // Rotating must never leave the cursor sitting on a greyed-out entry.
  for (int i = 0; i < 6; i++)
  {
    menu.HandleRotation(1);
    const MenuPage *page = menu.GetCurrentPage();
    TEST_ASSERT_TRUE(menu.IsItemSelectable(page->items[menu.GetCursor()]));
  }
}

void test_eco_entries_are_reachable_with_an_rtc(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Mode ECO"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Debut"));
}

// --- Value editing ----------------------------------------------------------

void test_editing_a_float_applies_the_step(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Consignes"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Temperature"));

  float before = g_test_settings.target_temperature;

  menu.HandleClick();               // enter edit mode
  TEST_ASSERT_TRUE(menu.IsEditing());
  menu.HandleRotation(2);           // two 0.5 C steps
  TEST_ASSERT_EQUAL_FLOAT(before + 1.0f, g_test_settings.target_temperature);

  menu.HandleClick();               // commit
  TEST_ASSERT_FALSE(menu.IsEditing());
  TEST_ASSERT_EQUAL_INT(1, g_change_count);
}

void test_rotation_moves_the_cursor_when_not_editing(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Consignes"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Temperature"));

  float before = g_test_settings.target_temperature;
  menu.HandleRotation(1);
  TEST_ASSERT_EQUAL_FLOAT(before, g_test_settings.target_temperature);
}

void test_values_are_clamped_to_their_range(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Consignes"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Temperature"));
  menu.HandleClick();

  menu.HandleRotation(1000);
  TEST_ASSERT_EQUAL_FLOAT(TARGET_TEMP_MAX, g_test_settings.target_temperature);

  menu.HandleRotation(-1000);
  TEST_ASSERT_EQUAL_FLOAT(TARGET_TEMP_MIN, g_test_settings.target_temperature);
}

void test_toggle_flips_on_a_single_click(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Sources"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Hydraulique"));

  bool before = g_test_settings.hydraulic_enabled;
  menu.HandleClick();

  // A boolean has nothing to scroll through, so it must not enter edit mode.
  TEST_ASSERT_FALSE(menu.IsEditing());
  TEST_ASSERT_EQUAL_INT(!before, g_test_settings.hydraulic_enabled);
  TEST_ASSERT_EQUAL_INT(1, g_change_count);
}

void test_narrow_bindings_do_not_overflow_their_field(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  // The damper calibration entries bind to uint16_t fields and the ECO hours to
  // uint8_t. If a binding declared a wider type, writing a large value here
  // would run past the field and corrupt its neighbour in the record.
  TEST_ASSERT_TRUE(SelectLabel(menu, "Systeme"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Registre ouvert"));
  menu.HandleClick();
  menu.HandleRotation(10000);

  TEST_ASSERT_EQUAL_UINT16(4095, g_test_settings.damper_raw_open);
  // The neighbouring field must be untouched.
  TEST_ASSERT_EQUAL_UINT16(DAMPER_RAW_CLOSED_DEFAULT, g_test_settings.damper_raw_closed);
  TEST_ASSERT_EQUAL_UINT32(LORA_TELEMETRY_INTERVAL_MS,
                           g_test_settings.lora_telemetry_interval_ms);
}

void test_eco_hours_stay_within_a_day(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Mode ECO"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Debut"));
  menu.HandleClick();
  menu.HandleRotation(500);

  TEST_ASSERT_EQUAL_UINT8(23, g_test_settings.eco_start_hour);
  TEST_ASSERT_EQUAL_UINT8(ECO_END_HOUR, g_test_settings.eco_end_hour);
}

void test_factory_reset_restores_defaults(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  g_test_settings.target_temperature = 44.0f;

  TEST_ASSERT_TRUE(SelectLabel(menu, "Systeme"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Reinit. usine"));
  menu.HandleClick();

  TEST_ASSERT_EQUAL_FLOAT(TEMPERATURE_TARGET, g_test_settings.target_temperature);
  TEST_ASSERT_TRUE(IsRecordValid(g_test_settings, static_cast<uint16_t>(SETTINGS_VERSION)));
}

// --- Display formatting -----------------------------------------------------

void test_boolean_entries_read_as_words(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Sources"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Chauffage elec."));

  const MenuItem &item = menu.GetCurrentPage()->items[menu.GetCursor()];
  char text[24];

  g_test_settings.electric_enabled = true;
  menu.FormatItemValue(item, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Actif", text);

  g_test_settings.electric_enabled = false;
  menu.FormatItemValue(item, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Inactif", text);
}

void test_fractional_steps_keep_a_decimal(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Consignes"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Temperature"));

  const MenuItem &item = menu.GetCurrentPage()->items[menu.GetCursor()];
  char text[24];

  // A 0.5 C step would be invisible without the decimal.
  g_test_settings.target_temperature = 40.5f;
  menu.FormatItemValue(item, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("40.5 C", text);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_menu_opens_on_the_root_page);
  RUN_TEST(test_cursor_stops_at_the_ends_instead_of_wrapping);
  RUN_TEST(test_submenu_entry_and_back);
  RUN_TEST(test_back_on_the_root_page_closes_the_menu);
  RUN_TEST(test_eco_entries_are_greyed_out_without_an_rtc);
  RUN_TEST(test_cursor_skips_unavailable_entries);
  RUN_TEST(test_eco_entries_are_reachable_with_an_rtc);

  RUN_TEST(test_editing_a_float_applies_the_step);
  RUN_TEST(test_rotation_moves_the_cursor_when_not_editing);
  RUN_TEST(test_values_are_clamped_to_their_range);
  RUN_TEST(test_toggle_flips_on_a_single_click);
  RUN_TEST(test_narrow_bindings_do_not_overflow_their_field);
  RUN_TEST(test_eco_hours_stay_within_a_day);
  RUN_TEST(test_factory_reset_restores_defaults);

  RUN_TEST(test_boolean_entries_read_as_words);
  RUN_TEST(test_fractional_steps_keep_a_decimal);

  return UNITY_END();
}
