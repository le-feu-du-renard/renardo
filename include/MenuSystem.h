#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>
#include "Dryer.h"
#include "Display.h"

/**
 * Menu item types
 */
enum class MenuItemType {
  kSubmenu,   // Opens a submenu
  kNumber,    // Numeric value to edit
  kCommand,   // Action to execute
  kBack       // Return to parent menu
};

/**
 * Forward declaration
 */
class MenuSystem;

/**
 * Base class for menu items
 */
class MenuItem {
 public:
  MenuItem(const char* text, MenuItemType type)
    : text_(text), type_(type) {}

  virtual ~MenuItem() {}

  const char* GetText() const { return text_; }
  MenuItemType GetType() const { return type_; }

  virtual void OnEnter(MenuSystem* menu) {}
  virtual void OnIncrement(MenuSystem* menu) {}
  virtual void OnDecrement(MenuSystem* menu) {}
  virtual String GetValueString() const { return ""; }

 protected:
  const char* text_;
  MenuItemType type_;
};

/**
 * Submenu item
 */
class SubmenuItem : public MenuItem {
 public:
  SubmenuItem(const char* text, MenuItem** items, uint8_t item_count)
    : MenuItem(text, MenuItemType::kSubmenu),
      items_(items),
      item_count_(item_count) {}

  void OnEnter(MenuSystem* menu) override;

  MenuItem** GetItems() const { return items_; }
  uint8_t GetItemCount() const { return item_count_; }

 protected:
  MenuItem** items_;
  uint8_t item_count_;
};

/**
 * Numeric menu item
 */
class NumberMenuItem : public MenuItem {
 public:
  typedef float (*GetValueFunc)(Dryer*);
  typedef void (*SetValueFunc)(Dryer*, float);

  NumberMenuItem(const char* text, GetValueFunc get_func, SetValueFunc set_func,
                 float min_value, float max_value, float step)
    : MenuItem(text, MenuItemType::kNumber),
      get_func_(get_func),
      set_func_(set_func),
      min_value_(min_value),
      max_value_(max_value),
      step_(step) {}

  void OnEnter(MenuSystem* menu) override;
  void OnIncrement(MenuSystem* menu) override;
  void OnDecrement(MenuSystem* menu) override;
  String GetValueString() const override;

 private:
  GetValueFunc get_func_;
  SetValueFunc set_func_;
  float min_value_;
  float max_value_;
  float step_;
  bool editing_ = false;

  friend class MenuSystem;
};

/**
 * Command menu item
 */
class CommandMenuItem : public MenuItem {
 public:
  typedef void (*CommandFunc)(MenuSystem*);

  CommandMenuItem(const char* text, CommandFunc command)
    : MenuItem(text, MenuItemType::kCommand),
      command_(command) {}

  void OnEnter(MenuSystem* menu) override;

 private:
  CommandFunc command_;
};

/**
 * Back menu item
 */
class BackMenuItem : public MenuItem {
 public:
  BackMenuItem(const char* text)
    : MenuItem(text, MenuItemType::kBack) {}

  void OnEnter(MenuSystem* menu) override;
};

/**
 * Main Menu System
 */
class MenuSystem {
 public:
  MenuSystem(Dryer* dryer, Display* display);

  void Begin(MenuItem* root_menu);

  // Navigation
  void Show();
  void Hide();
  void Up();
  void Down();
  void Enter();
  void Back();

  // State
  bool IsActive() const { return active_; }
  bool IsEditing() const { return editing_; }

  // Access
  Dryer* GetDryer() { return dryer_; }
  Display* GetDisplay() { return display_; }

  // Rendering
  void Render();

  // Current item access (for input handling)
  MenuItem* GetCurrentItem();

 private:
  static constexpr uint8_t kMaxMenuDepth = 4;

  Dryer* dryer_;
  Display* display_;

  bool active_;
  bool editing_;

  // Menu navigation state
  struct MenuLevel {
    MenuItem** items;
    uint8_t item_count;
    uint8_t selected_index;
  };

  MenuLevel menu_stack_[kMaxMenuDepth];
  uint8_t menu_depth_;
  MenuItem* root_menu_;

  void EnterSubmenu(MenuItem** items, uint8_t item_count);
  void ExitSubmenu();

  friend class NumberMenuItem;
  friend class SubmenuItem;
  friend class BackMenuItem;
  friend class DynamicOperationsSubmenu;
};

#endif  // MENU_SYSTEM_H
