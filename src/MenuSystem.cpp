#include "MenuSystem.h"

// ========== MenuItem Implementations ==========

void SubmenuItem::OnEnter(MenuSystem* menu) {
  menu->EnterSubmenu(items_, item_count_, text_);
}

void NumberMenuItem::OnEnter(MenuSystem* menu) {
  editing_ = !editing_;
  menu->editing_ = editing_;

  if (!editing_) {
    // Save settings when exiting edit mode
    menu->GetDryer()->SaveSettings();
  }
}

void NumberMenuItem::OnIncrement(MenuSystem* menu) {
  if (editing_) {
    float current_value = get_func_(menu->GetDryer());
    float new_value = current_value + step_;
    if (new_value <= max_value_) {
      set_func_(menu->GetDryer(), new_value);
    }
  } else {
    menu->Up();
  }
}

void NumberMenuItem::OnDecrement(MenuSystem* menu) {
  if (editing_) {
    float current_value = get_func_(menu->GetDryer());
    float new_value = current_value - step_;
    if (new_value >= min_value_) {
      set_func_(menu->GetDryer(), new_value);
    }
  } else {
    menu->Down();
  }
}

String NumberMenuItem::GetValueString() const {
  // This will be implemented with actual value in Render()
  return "";
}

void CommandMenuItem::OnEnter(MenuSystem* menu) {
  command_(menu);
}

void BackMenuItem::OnEnter(MenuSystem* menu) {
  menu->ExitSubmenu();
}

// ========== MenuSystem Implementation ==========

MenuSystem::MenuSystem(Dryer* dryer, Display* display)
  : dryer_(dryer),
    display_(display),
    active_(false),
    editing_(false),
    menu_depth_(0),
    root_menu_(nullptr) {
}

void MenuSystem::Begin(MenuItem* root_menu) {
  root_menu_ = root_menu;
  Serial.println("MenuSystem initialized");
}

void MenuSystem::Show() {
  if (!active_) {
    active_ = true;
    menu_depth_ = 0;

    // Initialize root menu
    if (root_menu_ && root_menu_->GetType() == MenuItemType::kSubmenu) {
      SubmenuItem* root = static_cast<SubmenuItem*>(root_menu_);
      menu_stack_[0].items = root->GetItems();
      menu_stack_[0].item_count = root->GetItemCount();
      menu_stack_[0].selected_index = 0;
      menu_stack_[0].title = root->GetText();
      menu_depth_ = 1;
    }

    Serial.println("Menu shown");
  }
}

void MenuSystem::Hide() {
  if (active_) {
    active_ = false;
    editing_ = false;
    menu_depth_ = 0;
    Serial.println("Menu hidden");
  }
}

void MenuSystem::Up() {
  if (!active_ || menu_depth_ == 0) return;

  MenuLevel& current = menu_stack_[menu_depth_ - 1];
  if (current.selected_index > 0) {
    current.selected_index--;
  }
}

void MenuSystem::Down() {
  if (!active_ || menu_depth_ == 0) return;

  MenuLevel& current = menu_stack_[menu_depth_ - 1];
  if (current.selected_index < current.item_count - 1) {
    current.selected_index++;
  }
}

void MenuSystem::Enter() {
  if (!active_ || menu_depth_ == 0) return;

  MenuItem* item = GetCurrentItem();
  if (item) {
    item->OnEnter(this);
  }
}

void MenuSystem::Back() {
  if (!active_) return;

  if (editing_) {
    // Exit edit mode
    MenuItem* item = GetCurrentItem();
    if (item && item->GetType() == MenuItemType::kNumber) {
      NumberMenuItem* num_item = static_cast<NumberMenuItem*>(item);
      num_item->editing_ = false;
      editing_ = false;
      dryer_->SaveSettings();
    }
  } else {
    // Go back in menu
    ExitSubmenu();
  }
}

void MenuSystem::EnterSubmenu(MenuItem** items, uint8_t item_count, const char* title) {
  if (menu_depth_ >= kMaxMenuDepth) return;

  menu_stack_[menu_depth_].items = items;
  menu_stack_[menu_depth_].item_count = item_count;
  menu_stack_[menu_depth_].selected_index = 0;
  menu_stack_[menu_depth_].title = title;
  menu_depth_++;
}

const char* MenuSystem::GetCurrentMenuTitle() const {
  if (menu_depth_ == 0) return nullptr;
  return menu_stack_[menu_depth_ - 1].title;
}

void MenuSystem::ExitSubmenu() {
  if (menu_depth_ > 1) {
    menu_depth_--;
  } else {
    Hide();
  }
}

MenuItem* MenuSystem::GetCurrentItem() {
  if (menu_depth_ == 0) return nullptr;

  MenuLevel& current = menu_stack_[menu_depth_ - 1];
  if (current.selected_index >= current.item_count) return nullptr;

  return current.items[current.selected_index];
}

void MenuSystem::Render() {
  if (!active_ || !display_) return;

  display_->ClearMenuArea();

  if (menu_depth_ == 0) return;

  MenuLevel& current = menu_stack_[menu_depth_ - 1];

  // Draw header with menu title
  const char* title = current.title;
  if (title != nullptr) {
    display_->DrawMenuHeader(title);
  }

  // Display up to 4 items at a time (1 less to make room for header)
  const uint8_t kMaxVisibleItems = 4;
  uint8_t start_index = 0;

  // Calculate scroll position
  if (current.selected_index >= kMaxVisibleItems) {
    start_index = current.selected_index - kMaxVisibleItems + 1;
  }

  // Render visible items (offset by 1 for header)
  for (uint8_t i = 0; i < kMaxVisibleItems && (start_index + i) < current.item_count; i++) {
    uint8_t item_index = start_index + i;
    MenuItem* item = current.items[item_index];
    bool selected = (item_index == current.selected_index);

    String line = "";
    if (selected && editing_) {
      line += "> ";
    }

    line += item->GetText();

    // Add value for number items
    if (item->GetType() == MenuItemType::kNumber) {
      NumberMenuItem* num_item = static_cast<NumberMenuItem*>(item);
      float value = num_item->get_func_(dryer_);
      line += ": ";
      line += String(value, 1);
    }
    // Add value for info items
    else if (item->GetType() == MenuItemType::kInfo) {
      InfoMenuItem* info_item = static_cast<InfoMenuItem*>(item);
      line += ": ";
      line += info_item->GetValueString(dryer_);
    }

    // Line index is i+1 to leave room for header
    display_->DrawMenuLine(i + 1, line.c_str(), selected);
  }

  display_->UpdateMenuArea();
}

