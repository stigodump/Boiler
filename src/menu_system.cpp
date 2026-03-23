#include "menu_system.hpp"

namespace boiler {

MenuSystem::MenuSystem()
    : active_(false), editing_value_(false), temp_edit_val_(0), time_slot_edit_index_(0), dirty_(false),
      root_menu_(nullptr), current_menu_(nullptr), selected_index_(0) {}

void MenuSystem::set_root_menu(const MenuNode* root) {
    root_menu_ = root;
    current_menu_ = root;
    selected_index_ = 0;
    dirty_ = true;
}

void MenuSystem::open() {
    if (!root_menu_) return;
    active_ = true;
    editing_value_ = false;
    time_slot_edit_index_ = 0;
    current_menu_ = root_menu_;
    selected_index_ = 0;
    dirty_ = true;
}

void MenuSystem::close() {
    active_ = false;
    editing_value_ = false;
    time_slot_edit_index_ = 0;
    dirty_ = true;
}

void MenuSystem::navigate_forward() {
    if (!current_menu_ || selected_index_ >= current_menu_->num_items) return;

    const MenuItem& item = current_menu_->items[selected_index_];

    switch (item.type) {
        case MenuItemType::Back:
            navigate_back();
            break;
            
        case MenuItemType::SubMenu:
            if (item.data.submenu) {
                current_menu_ = item.data.submenu;
                selected_index_ = 0;
                dirty_ = true;
            }
            break;

        case MenuItemType::DynamicSubMenu:
            if (item.data.dynamic_submenu.on_prepare) {
                item.data.dynamic_submenu.on_prepare();
            }
            if (item.data.dynamic_submenu.node) {
                current_menu_ = item.data.dynamic_submenu.node;
                selected_index_ = 0;
                dirty_ = true;
            }
            break;

        case MenuItemType::Action:
            if (item.data.action) {
                item.data.action();
            }
            break;

        case MenuItemType::ValueInt:
            if (!editing_value_) {
                // Enter edit mode
                if (item.data.int_val.value != nullptr) {
                    editing_value_ = true;
                    temp_edit_val_ = *(item.data.int_val.value);
                    dirty_ = true;
                }
            } else {
                // Confirm edit mode
                *(item.data.int_val.value) = temp_edit_val_;
                if (item.data.int_val.on_change) {
                    item.data.int_val.on_change();
                }
                editing_value_ = false;
                dirty_ = true;
            }
            break;

        case MenuItemType::ValueTimeSlot:
            if (!editing_value_) {
                // Enter edit mode starting at start_hour
                if (item.data.time_slot.value != nullptr) {
                    editing_value_ = true;
                    temp_edit_slot_ = *(item.data.time_slot.value);
                    time_slot_edit_index_ = 1;
                    dirty_ = true;
                }
            } else {
                // Progress to the next field
                time_slot_edit_index_++;
                if (time_slot_edit_index_ > 4) {
                    // All fields complete, confirm edit and exit
                    *(item.data.time_slot.value) = temp_edit_slot_;
                    if (item.data.time_slot.on_change) {
                        item.data.time_slot.on_change();
                    }
                    editing_value_ = false;
                    time_slot_edit_index_ = 0;
                    dirty_ = true;
                } else {
                    dirty_line_ = true;
                }
            }
            break;
    }
}

void MenuSystem::navigate_back() {
    if (editing_value_) {
        // Cancel edit
        editing_value_ = false;
        time_slot_edit_index_ = 0;
        dirty_ = true;
        return;
    }

    if (current_menu_ && current_menu_->parent) {
        current_menu_ = current_menu_->parent;
        selected_index_ = 0;
        dirty_ = true;
    } else {
        // We are at root, close the menu completely
        close();
    }
}

void MenuSystem::on_select() {
    if (!active_) {
        open();
        return;
    }
    navigate_forward();
}

void MenuSystem::on_right() {
    if (!active_) return;
    
    // In our paradigm, Right acts like Select for navigating INTO submenus
    // But if editing, maybe it does nothing or jumps cursor (not implemented)
    if (!editing_value_) {
        const MenuItem& item = current_menu_->items[selected_index_];
        if (item.type == MenuItemType::SubMenu || item.type == MenuItemType::DynamicSubMenu) {
            navigate_forward();
        } else if (item.type == MenuItemType::ValueTimeSlot) {
            if (item.data.time_slot.delete_node) {
                current_menu_ = item.data.time_slot.delete_node;
                selected_index_ = 0;
                dirty_ = true;
            }
        }
    }
}

void MenuSystem::on_left() {
    if (!active_) return;
    navigate_back();
}

void MenuSystem::on_up() {
    if (!active_) return;

    if (editing_value_) {
        const MenuItem& item = current_menu_->items[selected_index_];
        if (item.type == MenuItemType::ValueInt) {
            temp_edit_val_ += item.data.int_val.step;
            if (temp_edit_val_ > item.data.int_val.max_val) {
                temp_edit_val_ = item.data.int_val.max_val;
            }
        } else if (item.type == MenuItemType::ValueTimeSlot) {
            if (time_slot_edit_index_ == 1) { // start hour
                temp_edit_slot_.start_hour = (temp_edit_slot_.start_hour + 1) % 24;
            } else if (time_slot_edit_index_ == 2) { // start min
                temp_edit_slot_.start_minute = (temp_edit_slot_.start_minute + 5) % 60;
            } else if (time_slot_edit_index_ == 3) { // end hour
                temp_edit_slot_.end_hour = (temp_edit_slot_.end_hour + 1) % 24;
            } else if (time_slot_edit_index_ == 4) { // end min
                temp_edit_slot_.end_minute = (temp_edit_slot_.end_minute + 5) % 60;
            }
            
            // Enforce start <= end constraint
            int start_mins = temp_edit_slot_.start_hour * 60 + temp_edit_slot_.start_minute;
            int end_mins = temp_edit_slot_.end_hour * 60 + temp_edit_slot_.end_minute;
            if (start_mins > end_mins) {
                if (time_slot_edit_index_ <= 2) { 
                    // Editing start time, cap it to end time
                    temp_edit_slot_.start_hour = temp_edit_slot_.end_hour;
                    temp_edit_slot_.start_minute = temp_edit_slot_.end_minute;
                } else {
                    // Editing end time, cap it to start time
                    temp_edit_slot_.end_hour = temp_edit_slot_.start_hour;
                    temp_edit_slot_.end_minute = temp_edit_slot_.start_minute;
                }
            }
        }
        dirty_line_ = true;
    } else {
        if (selected_index_ > 0) {
            selected_index_--;
        } else {
            // wrap around
            selected_index_ = current_menu_->num_items - 1;
        }
        dirty_ = true;
    }
}

void MenuSystem::on_down() {
    if (!active_) return;

    if (editing_value_) {
        const MenuItem& item = current_menu_->items[selected_index_];
        if (item.type == MenuItemType::ValueInt) {
            temp_edit_val_ -= item.data.int_val.step;
            if (temp_edit_val_ < item.data.int_val.min_val) {
                temp_edit_val_ = item.data.int_val.min_val;
            }
        } else if (item.type == MenuItemType::ValueTimeSlot) {
            if (time_slot_edit_index_ == 1) { // start hour
                temp_edit_slot_.start_hour = (temp_edit_slot_.start_hour == 0) ? 23 : temp_edit_slot_.start_hour - 1;
            } else if (time_slot_edit_index_ == 2) { // start min
                temp_edit_slot_.start_minute = (temp_edit_slot_.start_minute < 5) ? 55 : temp_edit_slot_.start_minute - 5;
            } else if (time_slot_edit_index_ == 3) { // end hour
                temp_edit_slot_.end_hour = (temp_edit_slot_.end_hour == 0) ? 23 : temp_edit_slot_.end_hour - 1;
            } else if (time_slot_edit_index_ == 4) { // end min
                temp_edit_slot_.end_minute = (temp_edit_slot_.end_minute < 5) ? 55 : temp_edit_slot_.end_minute - 5;
            }
            
            // Enforce start <= end constraint
            int start_mins = temp_edit_slot_.start_hour * 60 + temp_edit_slot_.start_minute;
            int end_mins = temp_edit_slot_.end_hour * 60 + temp_edit_slot_.end_minute;
            if (start_mins > end_mins) {
                if (time_slot_edit_index_ <= 2) { 
                    // Editing start time, cap it to end time
                    temp_edit_slot_.start_hour = temp_edit_slot_.end_hour;
                    temp_edit_slot_.start_minute = temp_edit_slot_.end_minute;
                } else {
                    // Editing end time, cap it to start time
                    temp_edit_slot_.end_hour = temp_edit_slot_.start_hour;
                    temp_edit_slot_.end_minute = temp_edit_slot_.start_minute;
                }
            }
        }
        dirty_line_ = true;
    } else {
        if (selected_index_ < current_menu_->num_items - 1) {
            selected_index_++;
        } else {
            // wrap around
            selected_index_ = 0;
        }
        dirty_ = true;
    }
}

} // namespace boiler
