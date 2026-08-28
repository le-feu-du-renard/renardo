#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>

#include "DryerSettings.h"

// Type of the value an entry edits, so one binding pointer can serve them all.
enum class MenuValueType : uint8_t
{
  kNone,
  kFloat,
  kUint32,
  kUint16,
  kUint8,
  kBool,
};

enum class MenuItemKind : uint8_t
{
  kSubmenu,
  kBack,
  kValue,
  kAction,
  kInfo,
};

struct MenuPage;

// Wall clock snapshot exchanged with whoever owns the RTC.
struct MenuClock
{
  uint16_t year;
  uint8_t  month;
  uint8_t  day;
  uint8_t  hour;
  uint8_t  minute;
};

// A register's live feedback, for the read-only rows on the Registres page.
// Reading a raw value off the screen is how the two calibration marks are
// captured, so the page has to show it as it moves.
struct MenuDamperReadback
{
  uint16_t raw;
  float    percent;     // NAN when there is nothing usable to report
  bool     has_signal;  // the channel is carrying a signal at all
};

// Declared here rather than ad hoc in each .cpp: the menu stays unaware of
// RTClib and of the ADC, which is what keeps it compiling in the host tests.
void MenuSetRtcAvailable(bool available);

// Whether a session is running. The programme entry is greyed out while one is:
// the phase machine is already inside a programme, and there is no sensible
// answer to being asked to change it halfway.
void MenuSetSessionRunning(bool running);
void MenuSetClockHooks(bool (*read)(MenuClock &), void (*write)(const MenuClock &));

// `index` is 0 for the extraction register, 1 for the recycling one. The command
// hook drives the air path from the menu: it is what lets an operator send each
// register to its stops to read the marks off, and the only way out of an
// airflow fault that has both of them shut.
void MenuSetDamperHooks(bool (*read)(uint8_t index, MenuDamperReadback &),
                        void (*command)(bool extraction));

struct MenuItem
{
  const char      *label;
  MenuItemKind     kind;
  const MenuPage  *submenu;

  MenuValueType    value_type;
  void            *binding;
  float            min_value;
  float            max_value;
  float            step;
  const char      *unit;

  // Optional labels for a boolean, e.g. "Actif" / "Inactif".
  const char      *true_label;
  const char      *false_label;

  // Entries that make no sense in the current hardware configuration are shown
  // greyed out and skipped by the cursor, rather than hidden — a menu whose
  // entries move around depending on state is much harder to learn.
  bool (*is_available)();

  void (*action)();

  // Fired when a submenu is entered, for a page whose values come from
  // somewhere other than the settings record and have to be read in first.
  void (*on_enter)();

  // Text of a read-only kInfo row, produced on demand by the page that owns it.
  const char *(*text)();
};

struct MenuPage
{
  const char     *title;
  const MenuItem *items;
  uint8_t         count;
};

// Menu driven by rotation and a single click.
//
// Rotation moves the cursor, a click enters a submenu or starts editing a
// value, and each page ends with an explicit "Retour" entry — there is no long
// press, so going back has to be somewhere the knob can reach.
//
// While editing, rotation changes the value and a click commits it. Values are
// written straight into the DryerSettings record; committing an edit asks the
// owner to apply and persist it.
//
// Deliberately unaware of the display: MenuRenderer draws it from the state
// exposed below, the same split as DisplayModel and the main screen. That keeps
// the navigation and the value bindings testable on the host, which is where
// a mismatch between a binding width and its field would otherwise go
// unnoticed until it corrupted the record next to it.
class MenuSystem
{
public:
  MenuSystem();

  // `settings` must outlive the menu: entries bind directly to its fields.
  void Begin(DryerSettings *settings);

  // Called whenever a value is committed, so the owner can push the record into
  // the live managers and save it.
  void SetOnChange(void (*on_change)()) { on_change_ = on_change; }

  bool IsOpen() const { return open_; }
  void Open();
  void Close();

  // Feed the encoder. Returns true when the menu consumed the input.
  bool HandleRotation(int32_t detents);
  bool HandleClick();

  // --- State, read by MenuRenderer ---
  const MenuPage *GetCurrentPage() const { return stack_[depth_]; }
  uint8_t GetCursor() const { return cursor_stack_[depth_]; }
  uint8_t GetScroll() const { return scroll_; }
  bool    IsEditing() const { return editing_; }
  static constexpr uint8_t VisibleRows() { return kVisibleRows; }

  bool IsItemSelectable(const MenuItem &item) const;
  void FormatItemValue(const MenuItem &item, char *out, size_t length) const;

  // True once after anything moved, so the renderer can skip an idle frame — and
  // on its own clock while the current page carries a live row, which nothing
  // else would ever mark dirty.
  bool ConsumeDirty();

  // Marks the whole page dirty, e.g. after the main screen took over the panel.
  void Invalidate() { dirty_ = true; }

private:
  DryerSettings *settings_;
  void (*on_change_)();

  bool open_;
  bool editing_;
  bool dirty_;

  // Navigation stack: deep enough for the whole tree, which is two levels.
  static constexpr uint8_t kMaxDepth = 4;
  const MenuPage *stack_[kMaxDepth];
  uint8_t         cursor_stack_[kMaxDepth];
  uint8_t         depth_;

  // Seven rows of 28 px fill the band between the header and the hint bar. The
  // root page has exactly seven entries, so the whole of it is on screen at
  // once and the most-used page never scrolls.
  static constexpr uint8_t kVisibleRows = 7;
  uint8_t scroll_;

  // Repaint cadence for a page carrying a live row. Matched to
  // DAMPER_SAMPLE_INTERVAL: repainting faster than the value can change only
  // costs sprites, and slower would hide the settling this page exists to show.
  static constexpr uint32_t kLiveRefreshMs = DAMPER_SAMPLE_INTERVAL;
  uint32_t last_live_refresh_ms_;

  bool HasLiveRow() const;

  const MenuPage *CurrentPage() const { return stack_[depth_]; }
  uint8_t         Cursor() const { return cursor_stack_[depth_]; }

  uint8_t FirstSelectableIndex(const MenuPage *page) const;
  void MoveCursor(int32_t detents);
  void AdjustValue(int32_t detents);
  void Activate();
  void GoBack();

  float ReadBinding(const MenuItem &item) const;
  void  WriteBinding(const MenuItem &item, float value);

  void EnsureCursorVisible();
};

#endif // MENU_SYSTEM_H
