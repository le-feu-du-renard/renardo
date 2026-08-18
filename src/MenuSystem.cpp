#include <stdio.h>
#include <string.h>

#include "MenuSystem.h"
#include "Logger.h"

// The pages bind to fields of a single DryerSettings record. It is captured
// here at Begin() so the static page tables can point into it: the tables are
// built once and never change shape, only the values behind them do.
static DryerSettings *g_settings = nullptr;
static bool           g_rtc_available = false;

// The wall clock lives in the RTC, not in the settings record, so the clock
// page edits a staging copy and hands it back through these hooks. Function
// pointers rather than a TimeManager reference: RTClib must not reach this
// file, or the host tests stop building.
static bool (*g_clock_read)(MenuClock &) = nullptr;
static void (*g_clock_write)(const MenuClock &) = nullptr;

static MenuClock g_clock_edit{2026, 1, 1, 0, 0};  // bound to the page entries
static MenuClock g_clock_shown{2026, 1, 1, 0, 0}; // last read, shown as-is

void MenuSetRtcAvailable(bool available) { g_rtc_available = available; }

void MenuSetClockHooks(bool (*read)(MenuClock &), void (*write)(const MenuClock &))
{
  g_clock_read = read;
  g_clock_write = write;
}

namespace
{

bool RtcPresent() { return g_rtc_available; }
bool AlwaysAvailable() { return true; }

// --- Page tables ------------------------------------------------------------
//
// Declared as file-scope arrays so the tree is visible in one place and costs
// no heap. Bindings are filled in Begin(), once g_settings is known.

MenuItem g_setpoint_items[4];
MenuItem g_source_items[3];
MenuItem g_eco_items[5];
MenuItem g_phase_items[5];
MenuItem g_control_items[10];
MenuItem g_clock_items[8];
MenuItem g_system_items[4];
MenuItem g_damper_items[5];
MenuItem g_root_items[7];

MenuPage g_setpoint_page{"Consignes", g_setpoint_items, 4};
MenuPage g_source_page{"Sources", g_source_items, 3};
MenuPage g_eco_page{"Mode ECO", g_eco_items, 5};
MenuPage g_phase_page{"Phases", g_phase_items, 5};
MenuPage g_control_page{"Regulation", g_control_items, 10};
MenuPage g_clock_page{"Date / Heure", g_clock_items, 8};
MenuPage g_damper_page{"Registres", g_damper_items, 5};
MenuPage g_system_page{"Systeme", g_system_items, 4};
MenuPage g_root_page{"Menu", g_root_items, 7};

MenuItem MakeValue(const char *label, MenuValueType type, void *binding,
                   float min_value, float max_value, float step, const char *unit,
                   bool (*available)() = AlwaysAvailable)
{
  MenuItem item{};
  item.label        = label;
  item.kind         = MenuItemKind::kValue;
  item.value_type   = type;
  item.binding      = binding;
  item.min_value    = min_value;
  item.max_value    = max_value;
  item.step         = step;
  item.unit         = unit;
  item.is_available = available;
  return item;
}

MenuItem MakeToggle(const char *label, bool *binding,
                    const char *true_label = "Actif",
                    const char *false_label = "Inactif",
                    bool (*available)() = AlwaysAvailable)
{
  MenuItem item{};
  item.label        = label;
  item.kind         = MenuItemKind::kValue;
  item.value_type   = MenuValueType::kBool;
  item.binding      = binding;
  item.true_label   = true_label;
  item.false_label  = false_label;
  item.is_available = available;
  return item;
}

MenuItem MakeSubmenu(const char *label, const MenuPage *page,
                     bool (*available)() = AlwaysAvailable,
                     void (*on_enter)() = nullptr)
{
  MenuItem item{};
  item.label        = label;
  item.kind         = MenuItemKind::kSubmenu;
  item.submenu      = page;
  item.is_available = available;
  item.on_enter     = on_enter;
  return item;
}

MenuItem MakeInfo(const char *label, const char *(*text)())
{
  MenuItem item{};
  item.label        = label;
  item.kind         = MenuItemKind::kInfo;
  item.text         = text;
  item.is_available = AlwaysAvailable;
  return item;
}

MenuItem MakeBack()
{
  MenuItem item{};
  item.label        = "< Retour";
  item.kind         = MenuItemKind::kBack;
  item.is_available = AlwaysAvailable;
  return item;
}

MenuItem MakeAction(const char *label, void (*action)(),
                    bool (*available)() = AlwaysAvailable)
{
  MenuItem item{};
  item.label        = label;
  item.kind         = MenuItemKind::kAction;
  item.action       = action;
  item.is_available = available;
  return item;
}

uint8_t DaysInMonth(uint16_t year, uint8_t month)
{
  static const uint8_t kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12)
  {
    return 31;
  }
  if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
  {
    return 29;
  }
  return kDays[month - 1];
}

// Entering the clock page reads the RTC once: the entries then edit a copy, so
// a half-finished date never reaches the chip.
void LoadClockFromRtc()
{
  MenuClock clock{2026, 1, 1, 0, 0};
  if (g_clock_read != nullptr)
  {
    // A failed read leaves the fallback in place rather than zeros, which would
    // show as month 0 on screen.
    g_clock_read(clock);
  }
  g_clock_shown = clock;
  g_clock_edit  = clock;
}

const char *ClockText()
{
  static char text[16];
  snprintf(text, sizeof(text), "%02u/%02u/%02u %02u:%02u",
           g_clock_shown.day, g_clock_shown.month,
           static_cast<unsigned>(g_clock_shown.year % 100),
           g_clock_shown.hour, g_clock_shown.minute);
  return text;
}

void ApplyClockToRtc()
{
  // The day is clamped here rather than while editing: its bound depends on the
  // month, and moving a value under the user's fingers as they turn the knob
  // past February is worse than correcting an impossible date once, on commit.
  uint8_t last_day = DaysInMonth(g_clock_edit.year, g_clock_edit.month);
  if (g_clock_edit.day > last_day)
  {
    g_clock_edit.day = last_day;
  }

  if (g_clock_write != nullptr)
  {
    g_clock_write(g_clock_edit);
  }

  // Read back, so the Horloge row shows what the RTC actually took.
  LoadClockFromRtc();
  Logger::Info("Menu: clock set to %s", ClockText());
}

void ResetToFactoryDefaults()
{
  if (g_settings != nullptr)
  {
    g_settings->Reset();
    Logger::Warning("Menu: settings reset to factory defaults");
  }
}

} // namespace

MenuSystem::MenuSystem()
    : settings_(nullptr),
      on_change_(nullptr),
      open_(false),
      editing_(false),
      dirty_(true),
      depth_(0),
      scroll_(0)
{
  for (uint8_t i = 0; i < kMaxDepth; i++)
  {
    stack_[i] = nullptr;
    cursor_stack_[i] = 0;
  }
}

void MenuSystem::Begin(DryerSettings *settings)
{
  settings_ = settings;
  g_settings = settings;

  DryerSettings &s = *settings;

  g_setpoint_items[0] = MakeValue("Temperature", MenuValueType::kFloat,
                                  &s.target_temperature,
                                  TARGET_TEMP_MIN, TARGET_TEMP_MAX, 0.5f, " C");
  g_setpoint_items[1] = MakeValue("Hygrometrie", MenuValueType::kFloat,
                                  &s.target_humidity,
                                  TARGET_HUM_MIN, TARGET_HUM_MAX, 1.0f, " %HR");
  g_setpoint_items[2] = MakeValue("Eau (module)", MenuValueType::kFloat,
                                  &s.water_target,
                                  WATER_TARGET_MIN, WATER_TARGET_MAX, 1.0f, " C");
  g_setpoint_items[3] = MakeBack();

  g_source_items[0] = MakeToggle("Chauffage elec.", &s.electric_enabled);
  g_source_items[1] = MakeToggle("Hydraulique", &s.hydraulic_enabled);
  g_source_items[2] = MakeBack();

  // Every ECO entry depends on a wall clock, so all of them are greyed out
  // together when no RTC answered at boot.
  g_eco_items[0] = MakeToggle("Mode ECO", &s.eco_enabled, "Actif", "Inactif", RtcPresent);
  g_eco_items[1] = MakeValue("Debut", MenuValueType::kUint8, &s.eco_start_hour,
                             0.0f, 23.0f, 1.0f, " h", RtcPresent);
  g_eco_items[2] = MakeValue("Fin", MenuValueType::kUint8, &s.eco_end_hour,
                             0.0f, 23.0f, 1.0f, " h", RtcPresent);
  g_eco_items[3] = MakeValue("Consigne nuit", MenuValueType::kFloat,
                             &s.eco_target_percentage,
                             50.0f, 100.0f, 5.0f, " %", RtcPresent);
  g_eco_items[4] = MakeBack();

  g_phase_items[0] = MakeValue("Init", MenuValueType::kUint32, &s.init_phase_duration,
                               300.0f, 21600.0f, 300.0f, " s");
  g_phase_items[1] = MakeValue("Brassage", MenuValueType::kUint32,
                               &s.brassage_phase_duration,
                               60.0f, 7200.0f, 60.0f, " s");
  g_phase_items[2] = MakeValue("Extraction", MenuValueType::kUint32,
                               &s.extraction_phase_duration,
                               30.0f, 1800.0f, 30.0f, " s");
  g_phase_items[3] = MakeValue("Ouv. registre", MenuValueType::kUint32,
                               &s.extraction_damper_open_duration,
                               30.0f, 900.0f, 30.0f, " s");
  g_phase_items[4] = MakeBack();

  g_control_items[0] = MakeValue("Bande hydro", MenuValueType::kFloat,
                                 &s.band_hydraulic, 0.2f, 5.0f, 0.1f, " C");
  g_control_items[1] = MakeValue("Bande elec", MenuValueType::kFloat,
                                 &s.band_electric, 0.1f, 3.0f, 0.1f, " C");
  g_control_items[2] = MakeValue("Horizon hydro", MenuValueType::kFloat,
                                 &s.horizon_hydraulic, 0.0f, 600.0f, 10.0f, " s");
  g_control_items[3] = MakeValue("Horizon elec", MenuValueType::kFloat,
                                 &s.horizon_electric, 0.0f, 300.0f, 10.0f, " s");
  g_control_items[4] = MakeValue("Hydro ON min", MenuValueType::kFloat,
                                 &s.hydraulic_t_on_min, 0.0f, 1800.0f, 30.0f, " s");
  g_control_items[5] = MakeValue("Hydro OFF min", MenuValueType::kFloat,
                                 &s.hydraulic_t_off_min, 0.0f, 1800.0f, 30.0f, " s");
  g_control_items[6] = MakeValue("Elec ON min", MenuValueType::kFloat,
                                 &s.electric_t_on_min, 0.0f, 600.0f, 5.0f, " s");
  g_control_items[7] = MakeValue("Elec OFF min", MenuValueType::kFloat,
                                 &s.electric_t_off_min, 0.0f, 600.0f, 5.0f, " s");
  g_control_items[8] = MakeValue("Securite max", MenuValueType::kFloat,
                                 &s.safety_max, 30.0f, 70.0f, 1.0f, " C");
  g_control_items[9] = MakeBack();

  // These entries edit the staging copy, not the RTC: nothing reaches the chip
  // until "Valider". Like the ECO page, they all depend on the clock and are
  // greyed out together when no RTC answered at boot.
  g_clock_items[0] = MakeInfo("Horloge", ClockText);
  g_clock_items[1] = MakeValue("Jour", MenuValueType::kUint8, &g_clock_edit.day,
                               1.0f, 31.0f, 1.0f, "", RtcPresent);
  g_clock_items[2] = MakeValue("Mois", MenuValueType::kUint8, &g_clock_edit.month,
                               1.0f, 12.0f, 1.0f, "", RtcPresent);
  g_clock_items[3] = MakeValue("Annee", MenuValueType::kUint16, &g_clock_edit.year,
                               2020.0f, 2099.0f, 1.0f, "", RtcPresent);
  g_clock_items[4] = MakeValue("Heure", MenuValueType::kUint8, &g_clock_edit.hour,
                               0.0f, 23.0f, 1.0f, " h", RtcPresent);
  g_clock_items[5] = MakeValue("Minute", MenuValueType::kUint8, &g_clock_edit.minute,
                               0.0f, 59.0f, 1.0f, " min", RtcPresent);
  g_clock_items[6] = MakeAction("Valider", ApplyClockToRtc, RtcPresent);
  g_clock_items[7] = MakeBack();

  // One end-stop pair per register: extraction and recycling are asymmetric, so
  // calibrating one says nothing about the other. Each is captured by driving
  // that register to the stop and reading the raw value off this page.
  //
  // These bind to uint16_t fields: the width has to match the declaration
  // exactly, or WriteBinding would scribble past the end of the field.
  g_damper_items[0] = MakeValue("Extrac. ferme", MenuValueType::kUint16,
                                &s.extraction_raw_closed, 0.0f, 4095.0f, 10.0f, "");
  g_damper_items[1] = MakeValue("Extrac. ouvert", MenuValueType::kUint16,
                                &s.extraction_raw_open, 0.0f, 4095.0f, 10.0f, "");
  g_damper_items[2] = MakeValue("Recycl. ferme", MenuValueType::kUint16,
                                &s.recycling_raw_closed, 0.0f, 4095.0f, 10.0f, "");
  g_damper_items[3] = MakeValue("Recycl. ouvert", MenuValueType::kUint16,
                                &s.recycling_raw_open, 0.0f, 4095.0f, 10.0f, "");
  g_damper_items[4] = MakeBack();

  g_system_items[0] = MakeSubmenu("Registres", &g_damper_page);
  g_system_items[1] = MakeSubmenu("Date / Heure", &g_clock_page, RtcPresent,
                                  LoadClockFromRtc);
  g_system_items[2] = MakeAction("Reinit. usine", ResetToFactoryDefaults);
  g_system_items[3] = MakeBack();

  g_root_items[0] = MakeSubmenu("Consignes", &g_setpoint_page);
  g_root_items[1] = MakeSubmenu("Sources", &g_source_page);
  g_root_items[2] = MakeSubmenu("Mode ECO", &g_eco_page);
  g_root_items[3] = MakeSubmenu("Phases", &g_phase_page);
  g_root_items[4] = MakeSubmenu("Regulation", &g_control_page);
  g_root_items[5] = MakeSubmenu("Systeme", &g_system_page);
  g_root_items[6] = MakeBack();

  stack_[0] = &g_root_page;
  cursor_stack_[0] = 0;
  depth_ = 0;

  Logger::Info("MenuSystem: ready");
}

// --- Navigation -------------------------------------------------------------

void MenuSystem::Open()
{
  open_ = true;
  editing_ = false;
  depth_ = 0;
  cursor_stack_[0] = 0;
  scroll_ = 0;
  dirty_ = true;
}

void MenuSystem::Close()
{
  open_ = false;
  editing_ = false;
}

bool MenuSystem::ConsumeDirty()
{
  bool was_dirty = dirty_;
  dirty_ = false;
  return was_dirty;
}

bool MenuSystem::IsItemSelectable(const MenuItem &item) const
{
  if (item.kind == MenuItemKind::kInfo)
  {
    return false;
  }
  return item.is_available == nullptr || item.is_available();
}

uint8_t MenuSystem::FirstSelectableIndex(const MenuPage *page) const
{
  if (page != nullptr)
  {
    for (uint8_t i = 0; i < page->count; i++)
    {
      if (IsItemSelectable(page->items[i]))
      {
        return i;
      }
    }
  }
  return 0;
}

void MenuSystem::MoveCursor(int32_t detents)
{
  const MenuPage *page = CurrentPage();
  if (page == nullptr || page->count == 0)
  {
    return;
  }

  int32_t direction = (detents > 0) ? 1 : -1;
  int32_t steps = detents > 0 ? detents : -detents;

  int32_t cursor = cursor_stack_[depth_];
  for (int32_t i = 0; i < steps; i++)
  {
    // Walk past unavailable entries so the cursor never lands on one, and stop
    // at the ends rather than wrapping: wrapping in a long list makes it easy
    // to overshoot "Retour" without noticing.
    int32_t next = cursor;
    do
    {
      next += direction;
      if (next < 0 || next >= page->count)
      {
        next = -1;
        break;
      }
    } while (!IsItemSelectable(page->items[next]));

    if (next < 0)
    {
      break;
    }
    cursor = next;
  }

  cursor_stack_[depth_] = static_cast<uint8_t>(cursor);
  EnsureCursorVisible();
  dirty_ = true;
}

void MenuSystem::EnsureCursorVisible()
{
  uint8_t cursor = Cursor();
  if (cursor < scroll_)
  {
    scroll_ = cursor;
  }
  else if (cursor >= scroll_ + kVisibleRows)
  {
    scroll_ = static_cast<uint8_t>(cursor - kVisibleRows + 1);
  }
}

void MenuSystem::GoBack()
{
  if (depth_ == 0)
  {
    Close();
    return;
  }
  depth_--;
  scroll_ = 0;
  EnsureCursorVisible();
  dirty_ = true;
}

void MenuSystem::Activate()
{
  const MenuPage *page = CurrentPage();
  if (page == nullptr || Cursor() >= page->count)
  {
    return;
  }

  const MenuItem &item = page->items[Cursor()];
  if (!IsItemSelectable(item))
  {
    return;
  }

  switch (item.kind)
  {
  case MenuItemKind::kSubmenu:
    if (item.submenu != nullptr && depth_ + 1 < kMaxDepth)
    {
      depth_++;
      stack_[depth_] = item.submenu;
      // Not necessarily row 0: a page can open on a read-only row, and the
      // cursor must never rest on something a click cannot act upon.
      cursor_stack_[depth_] = FirstSelectableIndex(item.submenu);
      scroll_ = 0;
      dirty_ = true;

      // After the page is on the stack, so the hook can fill the values the
      // page is about to show.
      if (item.on_enter != nullptr)
      {
        item.on_enter();
      }
    }
    break;

  case MenuItemKind::kBack:
    GoBack();
    break;

  case MenuItemKind::kValue:
    if (item.value_type == MenuValueType::kBool)
    {
      // A boolean has nothing to scroll through, so a click just flips it.
      WriteBinding(item, ReadBinding(item) != 0.0f ? 0.0f : 1.0f);
      if (on_change_ != nullptr)
      {
        on_change_();
      }
      dirty_ = true;
    }
    else
    {
      editing_ = !editing_;
      if (!editing_ && on_change_ != nullptr)
      {
        on_change_(); // committed
      }
      dirty_ = true;
    }
    break;

  case MenuItemKind::kAction:
    if (item.action != nullptr)
    {
      item.action();
      if (on_change_ != nullptr)
      {
        on_change_();
      }
      dirty_ = true;
    }
    break;

  default:
    break;
  }
}

bool MenuSystem::HandleRotation(int32_t detents)
{
  if (!open_ || detents == 0)
  {
    return false;
  }

  if (editing_)
  {
    AdjustValue(detents);
  }
  else
  {
    MoveCursor(detents);
  }
  return true;
}

bool MenuSystem::HandleClick()
{
  if (!open_)
  {
    return false;
  }
  Activate();
  return true;
}

// --- Value bindings ---------------------------------------------------------

float MenuSystem::ReadBinding(const MenuItem &item) const
{
  if (item.binding == nullptr)
  {
    return 0.0f;
  }
  switch (item.value_type)
  {
  case MenuValueType::kFloat:  return *static_cast<float *>(item.binding);
  case MenuValueType::kUint32: return static_cast<float>(*static_cast<uint32_t *>(item.binding));
  case MenuValueType::kUint16: return static_cast<float>(*static_cast<uint16_t *>(item.binding));
  case MenuValueType::kUint8:  return static_cast<float>(*static_cast<uint8_t *>(item.binding));
  case MenuValueType::kBool:   return *static_cast<bool *>(item.binding) ? 1.0f : 0.0f;
  default:                     return 0.0f;
  }
}

void MenuSystem::WriteBinding(const MenuItem &item, float value)
{
  if (item.binding == nullptr)
  {
    return;
  }
  switch (item.value_type)
  {
  case MenuValueType::kFloat:
    *static_cast<float *>(item.binding) = value;
    break;
  case MenuValueType::kUint32:
    *static_cast<uint32_t *>(item.binding) = static_cast<uint32_t>(lroundf(value));
    break;
  case MenuValueType::kUint16:
    *static_cast<uint16_t *>(item.binding) = static_cast<uint16_t>(lroundf(value));
    break;
  case MenuValueType::kUint8:
    *static_cast<uint8_t *>(item.binding) = static_cast<uint8_t>(lroundf(value));
    break;
  case MenuValueType::kBool:
    *static_cast<bool *>(item.binding) = (value != 0.0f);
    break;
  default:
    break;
  }
}

void MenuSystem::AdjustValue(int32_t detents)
{
  const MenuPage *page = CurrentPage();
  if (page == nullptr || Cursor() >= page->count)
  {
    return;
  }

  const MenuItem &item = page->items[Cursor()];
  if (item.kind != MenuItemKind::kValue || item.value_type == MenuValueType::kBool)
  {
    return;
  }

  float value = ReadBinding(item) + detents * item.step;
  value = constrain(value, item.min_value, item.max_value);
  WriteBinding(item, value);
  dirty_ = true;
}

void MenuSystem::FormatItemValue(const MenuItem &item, char *out, size_t length) const
{
  out[0] = '\0';

  switch (item.kind)
  {
  case MenuItemKind::kSubmenu:
    snprintf(out, length, ">");
    break;

  case MenuItemKind::kInfo:
    if (item.text != nullptr)
    {
      snprintf(out, length, "%s", item.text());
    }
    break;

  case MenuItemKind::kValue:
    if (item.value_type == MenuValueType::kBool)
    {
      bool on = ReadBinding(item) != 0.0f;
      snprintf(out, length, "%s", on ? item.true_label : item.false_label);
    }
    else if (item.value_type == MenuValueType::kFloat)
    {
      // Integer steps do not need a decimal; 0.5 C steps do.
      if (item.step >= 1.0f)
      {
        snprintf(out, length, "%.0f%s", ReadBinding(item), item.unit);
      }
      else
      {
        snprintf(out, length, "%.1f%s", ReadBinding(item), item.unit);
      }
    }
    else
    {
      snprintf(out, length, "%.0f%s", ReadBinding(item), item.unit);
    }
    break;

  default:
    break;
  }
}
