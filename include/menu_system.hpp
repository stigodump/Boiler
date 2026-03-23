#pragma once

#include "common/alarm_timer.hpp" // For TimeSlot
#include "common/fonts/font10x16.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace boiler {

struct MenuNode;

/// Callback for simple action items
using MenuActionCallback = void (*)();

/// Called when a value is changed and confirmed
using MenuValueChangeCallback = void (*)();

enum class MenuItemType {
  Back,           // Internal use: return to parent
  SubMenu,        // Navigates to a static child MenuNode
  DynamicSubMenu, // Calls a prepare function, then navigates to a child
                  // MenuNode
  Action,         // Triggers a callback
  ValueInt,       // Edits an integer value
  ValueTimeSlot,  // Edits a TimeSlot (hh:mm - hh:mm)
};

/// Represents a single selectable row in the menu
struct MenuItem {
  const char *label;
  MenuItemType type;
  union {
    const MenuNode *submenu;
    struct {
      const MenuNode *node;
      void (*on_prepare)(); // e.g. populates the node before entering
    } dynamic_submenu;
    MenuActionCallback action;
    struct {
      int *value;
      int min_val;
      int max_val;
      int step;
      MenuValueChangeCallback on_change;
      const char
          *format_suffix; // Optional string like "\x7F" "C" instead of brackets
    } int_val;
    struct {
      common::TimeSlot *value;
      MenuValueChangeCallback on_change;
      const MenuNode *delete_node; // Optional node to jump to on Right-press
    } time_slot;
  } data;
};

/// Represents a menu screen with a list of items
struct MenuNode {
  const char *title;
  std::size_t num_items;
  const MenuItem *items;
  const MenuNode *parent;
};

class MenuSystem {
public:
  MenuSystem();

  /// Set the top-level root menu
  void set_root_menu(const MenuNode *root);

  /// Call when the user presses Joystick Select
  void on_select();

  /// Call when the user presses Joystick Up
  void on_up();

  /// Call when the user presses Joystick Down
  void on_down();

  /// Call when the user presses Joystick Left
  void on_left();

  /// Call when the user presses Joystick Right
  void on_right();

  /// Returns true if the menu is currently visible/active
  bool is_active() const { return active_; }

  /// Forcefully open the menu to the root
  void open();

  /// Forcefully close the menu
  void close();

  /// Request a redraw (e.g., when a custom action changes data that affects
  /// rendering independently)
  void request_redraw() { dirty_ = true; }

  /// Render the menu to the display
  /// Template is used to accept whatever the Display type resolves to (GC9A01)
  template <typename DisplayT> void draw() const;

private:
  bool active_;
  bool editing_value_; // True if we selected a ValueInt or ValueTimeSlot and
                       // are using up/down to scroll values
  int temp_edit_val_;  // Temporary holding value before confirm for ValueInt
  common::TimeSlot temp_edit_slot_; // Temporary holding value for TimeSlot
  int time_slot_edit_index_; // 0 = not editing, 1 = start_hr, 2 = start_min, 3
                             // = end_hr, 4 = end_min
  mutable bool dirty_;       // True if the menu needs redrawing
  mutable bool
      dirty_line_; // True if only the currently selected line needs redrawing

  const MenuNode *root_menu_;
  const MenuNode *current_menu_;
  std::size_t selected_index_; // Which item is highlighted

  void navigate_forward();
  void navigate_back();
};

// --- View Template Implementation ---

template <typename DisplayT> void MenuSystem::draw() const {
  if (!active_ || !current_menu_)
    return;
  if (!dirty_ && !dirty_line_)
    return;

  bool full_redraw = dirty_;
  dirty_ = false;
  dirty_line_ = false;

  const common::fonts::FontDef<uint16_t> &font = common::fonts::Font10x16_Def;
  constexpr int scale = 1;
  const int char_width = (font.width + 1) * scale;
  const int char_height = (font.height + 4) * scale;
  constexpr int screen_width = DisplayT::ScreenWidth;
  constexpr int screen_height = DisplayT::ScreenHeight;

  if (full_redraw) {
    // Clear background
    DisplayT::clear_screen();

    // Draw Title (centered at top)
    int title_len = 0;
    while (current_menu_->title[title_len] != '\0')
      title_len++;
    int title_x = (screen_width - (title_len * char_width)) / 2;
    if (title_x < 0)
      title_x = 0;

    DisplayT::draw_string(title_x, 20, current_menu_->title, 0xFFFF, 0x0000,
                          font, scale);
    DisplayT::fill_rect(20, 20 + 16 + 4, 200, 2, 0xFFFF); // underline
  }

  // Calculate vertical scrolled offset
  // We want the selected item (selected_index_) to always be centered
  // vertically Center of screen Y = 120. Item center is item_height / 2 = 12.
  // So target Y for selected item is 108.
  const int target_selected_y = 108;

  // y_offset of the very first item (index 0)
  int start_y = target_selected_y - (selected_index_ * char_height);

  for (std::size_t i = 0; i < current_menu_->num_items; ++i) {
    if (!full_redraw && i != selected_index_)
      continue;

    int y_pos = start_y + (i * char_height);

    // Only draw if it's somewhat visible on the screen
    // Title area ends around y=45. Let's clip to 50 - 230 to avoid over-drawing
    // the edges
    if (y_pos < 45 || y_pos > 220) {
      continue;
    }

    const MenuItem &item = current_menu_->items[i];

    uint16_t text_color = 0xFFFF; // White
    uint16_t bg_color = 0x0000;   // Black

    if (i == selected_index_) {
      text_color = 0x0000; // Black text
      if (editing_value_) {
        bg_color = 0x001F; // Blue for actively editing item
      } else if (item.type == MenuItemType::ValueInt ||
                 item.type == MenuItemType::ValueTimeSlot) {
        bg_color = 0xF800; // Red for editable items
      } else {
        bg_color = 0xFFFF; // White for normal items
      }
    }

    // Initialize text buffer to zeros
    char text_buf[32] = {0};

    switch (item.type) {
    case MenuItemType::Back:
      snprintf(text_buf, sizeof(text_buf), "< %s", item.label);
      break;
    case MenuItemType::SubMenu:
    case MenuItemType::DynamicSubMenu:
      snprintf(text_buf, sizeof(text_buf), "%s >", item.label);
      break;
    case MenuItemType::Action:
      snprintf(text_buf, sizeof(text_buf), "%s", item.label);
      break;
    case MenuItemType::ValueInt: {
      int display_val = 0;
      if (i == selected_index_ && editing_value_) {
        display_val = temp_edit_val_;
      } else if (item.data.int_val.value != nullptr) {
        display_val = *(item.data.int_val.value);
      }
      if (item.data.int_val.format_suffix) {
        snprintf(text_buf, sizeof(text_buf), "%s %d%s", item.label, display_val,
                 item.data.int_val.format_suffix);
      } else {
        snprintf(text_buf, sizeof(text_buf), "%s [%d]", item.label,
                 display_val);
      }
      break;
    }
    case MenuItemType::ValueTimeSlot: {
      common::TimeSlot ts = {0};
      if (i == selected_index_ && editing_value_) {
        ts = temp_edit_slot_;
      } else if (item.data.time_slot.value != nullptr) {
        ts = *(item.data.time_slot.value);
      }
      snprintf(text_buf, sizeof(text_buf), "%02d:%02d - %02d:%02d",
               ts.start_hour, ts.start_minute, ts.end_hour, ts.end_minute);
      break;
    }
    }

    // Calculate centered X position for this specific line
    int line_len = 0;
    while (text_buf[line_len] != '\0')
      line_len++;
    int text_x = (240 - (line_len * char_width)) / 2;
    if (text_x < 0)
      text_x = 0;

    if (i == selected_index_) {
      if (!full_redraw) {
        // Clear the whole line horizontally to prevent artifacts from shrinking
        // text
        DisplayT::fill_rect(0, y_pos - 2, screen_width, char_height + 2,
                            0x0000);
      }
      // Draw a solid background box stretching nicely across the center
      // Padded horizontally to make the block look good
      DisplayT::fill_rect(text_x - 8, y_pos - 2, (line_len * char_width) + 16,
                          char_height, bg_color);
    }

    DisplayT::draw_string(text_x, y_pos, text_buf, text_color, bg_color, font,
                          scale);

    // Overlay specific active field highlight for ValueInt if editing
    if (i == selected_index_ && editing_value_ &&
        item.type == MenuItemType::ValueInt) {
      int hl_start_char = 0;
      // Skip the label
      while (item.label[hl_start_char] != '\0')
        hl_start_char++;
      // Skip space(s)
      while (text_buf[hl_start_char] == ' ')
        hl_start_char++;

      if (text_buf[hl_start_char] != '\0') {
        int hl_len = 0;
        while (text_buf[hl_start_char + hl_len] != '\0' &&
               text_buf[hl_start_char + hl_len] != ']') {
          if (item.data.int_val.format_suffix &&
              (text_buf[hl_start_char + hl_len] ==
               item.data.int_val.format_suffix[0]))
            break;
          hl_len++;
        }
        if (text_buf[hl_start_char + hl_len] == ']')
          hl_len++; // Include ']'

        int hl_x = text_x + (hl_start_char * char_width);
        DisplayT::fill_rect(hl_x - 2, y_pos - 2, (hl_len * char_width) + 4,
                            char_height - 4, 0x001F); // Blue highlight

        char sub_buf[16] = {0};
        for (int m = 0; m < hl_len && m < 15; m++)
          sub_buf[m] = text_buf[hl_start_char + m];
        DisplayT::draw_string(hl_x, y_pos, sub_buf, 0xFFFF, 0x001F, font,
                              scale);
      }
    }

    // Overlay specific active field highlight for ValueTimeSlot if editing
    if (i == selected_index_ && editing_value_ &&
        item.type == MenuItemType::ValueTimeSlot) {
      int hl_start_char = 0;
      if (time_slot_edit_index_ == 1)
        hl_start_char = 0; // start_hour
      else if (time_slot_edit_index_ == 2)
        hl_start_char = 3; // start_minute
      else if (time_slot_edit_index_ == 3)
        hl_start_char = 8; // end_hour
      else if (time_slot_edit_index_ == 4)
        hl_start_char = 11; // end_minute

      if (time_slot_edit_index_ > 0 && time_slot_edit_index_ <= 4) {
        char sub_buf[3] = {text_buf[hl_start_char], text_buf[hl_start_char + 1],
                           '\0'};
        int hl_x = text_x + (hl_start_char * char_width);
        DisplayT::fill_rect(hl_x - 2, y_pos - 2, (2 * char_width) + 4,
                            char_height - 4, 0x001F); // Blue highlight
        DisplayT::draw_string(hl_x, y_pos, sub_buf, 0xFFFF, 0x001F, font,
                              scale);
      }
    }
  }
}

} // namespace boiler
