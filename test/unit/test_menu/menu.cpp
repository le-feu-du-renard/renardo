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

namespace
{

DryerSettings g_test_settings;
int           g_change_count = 0;

void CountChange() { g_change_count++; }

// Stand-in for the RTC: the clock page reads it on entry and writes it back
// only when the user validates.
MenuClock g_fake_rtc{2026, 8, 18, 14, 32};
int       g_clock_writes = 0;

bool ReadFakeClock(MenuClock &clock)
{
  clock = g_fake_rtc;
  return true;
}

void WriteFakeClock(const MenuClock &clock)
{
  g_fake_rtc = clock;
  g_clock_writes++;
}

// Opens the menu with a known configuration.
void Prepare(MenuSystem &menu, bool rtc_available)
{
  g_test_settings.Reset();
  g_change_count = 0;
  g_fake_rtc = MenuClock{2026, 8, 18, 14, 32};
  g_clock_writes = 0;
  MenuSetRtcAvailable(rtc_available);
  MenuSetClockHooks(ReadFakeClock, WriteFakeClock);
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

// Walks into Systeme > Registres, where every register test starts.
bool OpenDamperPage(MenuSystem &menu)
{
  if (!SelectLabel(menu, "Systeme"))
  {
    return false;
  }
  menu.HandleClick();
  if (!SelectLabel(menu, "Registres"))
  {
    return false;
  }
  menu.HandleClick();
  return strcmp(menu.GetCurrentPage()->title, "Registres") == 0;
}

// Finds an entry by label without moving the cursor, so a greyed-out row can be
// inspected at all — SelectLabel walks the cursor, which skips them.
const MenuItem *FindItem(const MenuPage *page, const char *label)
{
  for (uint8_t i = 0; i < page->count; i++)
  {
    if (strcmp(page->items[i].label, label) == 0)
    {
      return &page->items[i];
    }
  }
  return nullptr;
}

// Walks into Systeme > Date / Heure, where every clock test starts.
bool OpenClockPage(MenuSystem &menu)
{
  if (!SelectLabel(menu, "Systeme"))
  {
    return false;
  }
  menu.HandleClick();
  if (!SelectLabel(menu, "Date / Heure"))
  {
    return false;
  }
  menu.HandleClick();
  return strcmp(menu.GetCurrentPage()->title, "Date / Heure") == 0;
}

// Sets one entry of the clock page to an absolute value, by winding it to its
// minimum first — the entries only move by steps.
void SetClockField(MenuSystem &menu, const char *label, int32_t steps_from_min)
{
  menu.HandleRotation(-100); // back to the top: SelectLabel only walks forward
  TEST_ASSERT_TRUE(SelectLabel(menu, label));
  menu.HandleClick();          // enter edit mode
  menu.HandleRotation(-10000); // wind down to the minimum
  menu.HandleRotation(steps_from_min);
  menu.HandleClick();          // commit
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
  TEST_ASSERT_TRUE(SelectLabel(menu, "Registres"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(SelectLabel(menu, "Extrac. maxi"));
  menu.HandleClick();
  menu.HandleRotation(10000);

  TEST_ASSERT_EQUAL_UINT16(4095, g_test_settings.extraction_raw_max);
  // The neighbouring fields must be untouched — the four calibration values sit
  // side by side, so an over-wide binding would land on one of them first.
  TEST_ASSERT_EQUAL_UINT16(DAMPER_RAW_MIN_DEFAULT,
                           g_test_settings.extraction_raw_min);
  TEST_ASSERT_EQUAL_UINT16(DAMPER_RAW_MIN_DEFAULT,
                           g_test_settings.recycling_raw_min);
  TEST_ASSERT_EQUAL_UINT16(DAMPER_RAW_MAX_DEFAULT,
                           g_test_settings.recycling_raw_max);
}

void test_recycling_entries_follow_the_register_count(void)
{
  // The count describes the machine, so everything about the second register
  // hangs off it. Greyed out rather than hidden: a page whose entries move
  // around depending on a setting is much harder to learn.
  MenuSystem menu;
  Prepare(menu, true);
  TEST_ASSERT_TRUE(OpenDamperPage(menu));

  const MenuPage *page = menu.GetCurrentPage();
  const MenuItem *recycling_min = FindItem(page, "Recycl. mini");
  const MenuItem *extraction_min = FindItem(page, "Extrac. mini");
  TEST_ASSERT_NOT_NULL(recycling_min);
  TEST_ASSERT_NOT_NULL(extraction_min);

  // One register out of the box.
  TEST_ASSERT_EQUAL_UINT8(1, g_test_settings.damper_count);
  TEST_ASSERT_FALSE(menu.IsItemSelectable(*recycling_min));
  TEST_ASSERT_TRUE(menu.IsItemSelectable(*extraction_min));
  TEST_ASSERT_FALSE(menu.IsItemSelectable(*FindItem(page, "Sens recycl.")));

  g_test_settings.damper_count = 2;
  TEST_ASSERT_TRUE(menu.IsItemSelectable(*recycling_min));
  TEST_ASSERT_TRUE(menu.IsItemSelectable(*FindItem(page, "Sens recycl.")));
}

void test_the_register_count_cannot_leave_its_range(void)
{
  MenuSystem menu;
  Prepare(menu, true);
  TEST_ASSERT_TRUE(OpenDamperPage(menu));
  TEST_ASSERT_TRUE(SelectLabel(menu, "Nb registres"));

  menu.HandleClick();
  menu.HandleRotation(10000);
  TEST_ASSERT_EQUAL_UINT8(DAMPER_COUNT_MAX, g_test_settings.damper_count);

  menu.HandleRotation(-10000);
  TEST_ASSERT_EQUAL_UINT8(1, g_test_settings.damper_count);
}

void test_the_direction_toggles_read_as_words(void)
{
  // Three booleans on one page, and getting one of them backwards on screen
  // would send an operator to invert a switch that was already right.
  MenuSystem menu;
  Prepare(menu, true);
  TEST_ASSERT_TRUE(OpenDamperPage(menu));

  const MenuPage *page = menu.GetCurrentPage();
  char text[24];

  g_test_settings.damper_feedback_low_is_open = true;
  menu.FormatItemValue(*FindItem(page, "Sens signal"), text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Bas=ouvert", text);

  g_test_settings.damper_feedback_low_is_open = false;
  menu.FormatItemValue(*FindItem(page, "Sens signal"), text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Bas=ferme", text);

  g_test_settings.damper_extraction_inverted = false;
  menu.FormatItemValue(*FindItem(page, "Sens extrac."), text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Normal", text);

  g_test_settings.damper_recycling_inverted = true;
  menu.FormatItemValue(*FindItem(page, "Sens recycl."), text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Inverse", text);
}

void test_a_page_with_a_live_row_repaints_on_its_own(void)
{
  // The register page's signal rows read the ADC, and nothing about an ADC
  // moving marks the menu dirty. Without this the rows freeze on whatever they
  // read when the cursor last moved — and reading a raw value off that page as
  // it settles is the whole calibration procedure, so a frozen row hands out a
  // number that was true seconds ago and looks exactly like one that is true now.
  MenuSystem menu;
  TestSetMillis(0);
  Prepare(menu, true);
  TEST_ASSERT_TRUE(OpenDamperPage(menu));

  TEST_ASSERT_TRUE(menu.ConsumeDirty());  // the navigation that got us here
  TEST_ASSERT_FALSE(menu.ConsumeDirty()); // idle: nothing has changed yet

  TestAdvanceMillis(DAMPER_SAMPLE_INTERVAL + 1);
  TEST_ASSERT_TRUE(menu.ConsumeDirty());
  TEST_ASSERT_FALSE(menu.ConsumeDirty());
}

void test_a_page_without_a_live_row_stays_idle(void)
{
  // The cost side of the same rule: a page that is only settings must not
  // repaint thirteen sprites twice a second for nothing.
  MenuSystem menu;
  TestSetMillis(0);
  Prepare(menu, true);
  TEST_ASSERT_TRUE(SelectLabel(menu, "Consignes"));
  menu.HandleClick();
  TEST_ASSERT_TRUE(menu.ConsumeDirty());

  TestAdvanceMillis(DAMPER_SAMPLE_INTERVAL * 10);
  TEST_ASSERT_FALSE(menu.ConsumeDirty());
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

// --- Clock ------------------------------------------------------------------

void test_clock_page_loads_the_rtc_on_entry(void)
{
  MenuSystem menu;
  Prepare(menu, true);

  TEST_ASSERT_TRUE(OpenClockPage(menu));

  // The entries show what the RTC held when the page opened, not zeros.
  const MenuPage *page = menu.GetCurrentPage();
  char text[24];
  menu.FormatItemValue(page->items[0], text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("18/08/26 14:32", text);

  TEST_ASSERT_TRUE(SelectLabel(menu, "Heure"));
  menu.FormatItemValue(page->items[menu.GetCursor()], text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("14 h", text);

  TEST_ASSERT_EQUAL_INT(0, g_clock_writes);
}

void test_editing_the_clock_does_not_touch_the_rtc_until_validated(void)
{
  MenuSystem menu;
  Prepare(menu, true);
  TEST_ASSERT_TRUE(OpenClockPage(menu));

  SetClockField(menu, "Heure", 7); // 0 + 7 -> 07 h

  // Leaving without validating must leave the chip exactly as it was: a
  // half-typed date is never what the user meant.
  TEST_ASSERT_EQUAL_INT(0, g_clock_writes);
  TEST_ASSERT_EQUAL_UINT8(14, g_fake_rtc.hour);

  TEST_ASSERT_TRUE(SelectLabel(menu, "< Retour"));
  menu.HandleClick();
  TEST_ASSERT_EQUAL_INT(0, g_clock_writes);
  TEST_ASSERT_EQUAL_UINT8(14, g_fake_rtc.hour);
}

void test_validating_writes_the_edited_clock_once(void)
{
  MenuSystem menu;
  Prepare(menu, true);
  TEST_ASSERT_TRUE(OpenClockPage(menu));

  SetClockField(menu, "Jour", 6);    // 1 + 6  -> 7
  SetClockField(menu, "Mois", 2);    // 1 + 2  -> 3
  SetClockField(menu, "Annee", 7);   // 2020+7 -> 2027
  SetClockField(menu, "Heure", 21);  // 0 + 21 -> 21
  SetClockField(menu, "Minute", 5);  // 0 + 5  -> 5

  TEST_ASSERT_TRUE(SelectLabel(menu, "Valider"));
  menu.HandleClick();

  TEST_ASSERT_EQUAL_INT(1, g_clock_writes);
  TEST_ASSERT_EQUAL_UINT16(2027, g_fake_rtc.year);
  TEST_ASSERT_EQUAL_UINT8(3, g_fake_rtc.month);
  TEST_ASSERT_EQUAL_UINT8(7, g_fake_rtc.day);
  TEST_ASSERT_EQUAL_UINT8(21, g_fake_rtc.hour);
  TEST_ASSERT_EQUAL_UINT8(5, g_fake_rtc.minute);

  // The read-only row reflects what the chip took.
  char text[24];
  menu.FormatItemValue(menu.GetCurrentPage()->items[0], text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("07/03/27 21:05", text);
}

void test_impossible_days_are_clamped_to_the_month(void)
{
  MenuSystem menu;
  Prepare(menu, true);
  TEST_ASSERT_TRUE(OpenClockPage(menu));

  // 31 February in a common year: the entry range cannot know the month, so the
  // date is only corrected on commit.
  SetClockField(menu, "Jour", 30);  // 1 + 30 -> 31
  SetClockField(menu, "Mois", 1);   // 1 + 1  -> February
  SetClockField(menu, "Annee", 6);  // 2020+6 -> 2026, common year
  TEST_ASSERT_TRUE(SelectLabel(menu, "Valider"));
  menu.HandleClick();
  TEST_ASSERT_EQUAL_UINT8(28, g_fake_rtc.day);

  // The same day in a leap year keeps the 29th.
  SetClockField(menu, "Jour", 30);
  SetClockField(menu, "Mois", 1);
  SetClockField(menu, "Annee", 8);  // 2028, leap year
  TEST_ASSERT_TRUE(SelectLabel(menu, "Valider"));
  menu.HandleClick();
  TEST_ASSERT_EQUAL_UINT8(29, g_fake_rtc.day);
}

void test_clock_entries_are_greyed_out_without_an_rtc(void)
{
  MenuSystem menu;
  Prepare(menu, false);

  // Without a clock the whole page is unreachable, exactly like ECO: the entry
  // stays visible in Systeme so the menu keeps its shape.
  TEST_ASSERT_TRUE(SelectLabel(menu, "Systeme"));
  menu.HandleClick();
  const MenuPage *system_page = menu.GetCurrentPage();
  for (uint8_t i = 0; i < system_page->count; i++)
  {
    if (strcmp(system_page->items[i].label, "Date / Heure") == 0)
    {
      TEST_ASSERT_FALSE(menu.IsItemSelectable(system_page->items[i]));
    }
  }
}

void test_the_clock_row_is_read_only(void)
{
  MenuSystem menu;
  Prepare(menu, true);
  TEST_ASSERT_TRUE(OpenClockPage(menu));

  // The page opens below the read-only row rather than on it, and rotating can
  // never bring the cursor back onto it.
  const MenuPage *page = menu.GetCurrentPage();
  TEST_ASSERT_FALSE(menu.IsItemSelectable(page->items[0]));
  TEST_ASSERT_TRUE(menu.GetCursor() > 0);

  menu.HandleRotation(-10);
  TEST_ASSERT_TRUE(menu.GetCursor() > 0);
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
  RUN_TEST(test_recycling_entries_follow_the_register_count);
  RUN_TEST(test_the_register_count_cannot_leave_its_range);
  RUN_TEST(test_the_direction_toggles_read_as_words);
  RUN_TEST(test_a_page_with_a_live_row_repaints_on_its_own);
  RUN_TEST(test_a_page_without_a_live_row_stays_idle);
  RUN_TEST(test_eco_hours_stay_within_a_day);
  RUN_TEST(test_factory_reset_restores_defaults);

  RUN_TEST(test_clock_page_loads_the_rtc_on_entry);
  RUN_TEST(test_editing_the_clock_does_not_touch_the_rtc_until_validated);
  RUN_TEST(test_validating_writes_the_edited_clock_once);
  RUN_TEST(test_impossible_days_are_clamped_to_the_month);
  RUN_TEST(test_clock_entries_are_greyed_out_without_an_rtc);
  RUN_TEST(test_the_clock_row_is_read_only);

  RUN_TEST(test_boolean_entries_read_as_words);
  RUN_TEST(test_fractional_steps_keep_a_decimal);

  return UNITY_END();
}
