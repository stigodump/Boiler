#include "boiler_menu.hpp"
#include "app_settings.hpp"
#include "boiler_board.hpp"
#include "common/alarm_timer.hpp"
#include "common/logger.hpp"

// Access the global logger from main.cpp
extern common::Logger<boiler_board::Console> log_core0;

namespace boiler {

// Global tracking of menu system for internal actions
static MenuSystem *g_menu_system = nullptr;

// Adjustable values buffer for menu interaction
static int menu_boiler_temp = 21;
static int menu_pump_delay_sec = 180;

// --- Callbacks ---

static void on_save_settings() {
  log_core0.info("[MENU] Save Settings Action Triggered!");
  if (g_menu_system) {
    g_menu_system->close();
  }
}
} // namespace boiler

extern common::AlarmTimer alarm_timer;   // From main.cpp
extern boiler::AppSettings app_settings; // From main.cpp

namespace boiler {

// Forward declarations for menu linking
using common::AlarmTimer;
using common::TimeSlot;

extern const MenuNode root_menu;

static void on_temp_changed() {
  // Optional callback if live-updating is desired when editing value.
  // For now we will rely on pressing "Save" to commit the value.
}

// --- Day Slots Submenu (Dynamic) ---

extern const MenuNode timers_menu;

static MenuItem day_slots_items[AlarmTimer::kMaxSlotsPerDay +
                                3]; // Cancel, Slots, Add, Save
static char day_slot_strings[AlarmTimer::kMaxSlotsPerDay][16] = {0};

static common::TimeSlot editing_slots[AlarmTimer::kMaxSlotsPerDay] = {};
static uint8_t editing_day = 0;

// Forward declaration for the parent ptr
extern const MenuNode timers_menu;

static MenuNode day_slots_menu = {
    "Day Slots",
    0, // Updated dynamically
    day_slots_items,
    &timers_menu // Parent is the timers menu
};

static MenuNode delete_slot_nodes[AlarmTimer::kMaxSlotsPerDay];
static MenuItem delete_slot_items[AlarmTimer::kMaxSlotsPerDay][2];

static void rebuild_day_slots_menu() {
  day_slots_items[0] = {"Cancel", MenuItemType::Back, {}};
  size_t item_count = 1;

  for (int i = 0; i < AlarmTimer::kMaxSlotsPerDay; ++i) {
    if (editing_slots[i].active) {
      // Prepare deletion menu for this specific slot index
      delete_slot_items[i][0] = {"Cancel", MenuItemType::Back, {}};
      delete_slot_items[i][1] = {
          "Confirm Delete", MenuItemType::Action, {.action = nullptr}};
      // Unfortunately lambdas in C++ without captures decay to func ptrs, but
      // here we need `i`. Workaround: We define 8 distinct non-capturing
      // lambdas manually or just use a static trick. But since kMaxSlotsPerDay
      // is small (8), we can just hardcode them in a switch or array.
      static const MenuActionCallback delete_cbs[8] = {
          []() {
            editing_slots[0].active = false;
            rebuild_day_slots_menu();
            if (g_menu_system)
              g_menu_system->on_left();
          },
          []() {
            editing_slots[1].active = false;
            rebuild_day_slots_menu();
            if (g_menu_system)
              g_menu_system->on_left();
          },
          []() {
            editing_slots[2].active = false;
            rebuild_day_slots_menu();
            if (g_menu_system)
              g_menu_system->on_left();
          },
          []() {
            editing_slots[3].active = false;
            rebuild_day_slots_menu();
            if (g_menu_system)
              g_menu_system->on_left();
          },
          []() {
            editing_slots[4].active = false;
            rebuild_day_slots_menu();
            if (g_menu_system)
              g_menu_system->on_left();
          },
          []() {
            editing_slots[5].active = false;
            rebuild_day_slots_menu();
            if (g_menu_system)
              g_menu_system->on_left();
          },
          []() {
            editing_slots[6].active = false;
            rebuild_day_slots_menu();
            if (g_menu_system)
              g_menu_system->on_left();
          },
          []() {
            editing_slots[7].active = false;
            rebuild_day_slots_menu();
            if (g_menu_system)
              g_menu_system->on_left();
          }};

      delete_slot_items[i][1].data.action = delete_cbs[i];

      delete_slot_nodes[i] = {"Delete Slot?", 2, delete_slot_items[i],
                              &day_slots_menu};

      day_slots_items[item_count] = {
          "", // No prefix label, MenuSystem renders the format
          MenuItemType::ValueTimeSlot,
          {.time_slot = {&editing_slots[i],
                         []() {
                           if (g_menu_system)
                             g_menu_system->request_redraw();
                         },
                         &delete_slot_nodes[i]}}};
      item_count++;
    }
  }

  // Add "Add Slot" if not full
  bool is_full = true;
  for (int i = 0; i < AlarmTimer::kMaxSlotsPerDay; ++i) {
    if (!editing_slots[i].active) {
      is_full = false;
      break;
    }
  }

  if (!is_full) {
    day_slots_items[item_count] = {
        "Add Slot", MenuItemType::Action, {.action = []() {
          for (int i = 0; i < AlarmTimer::kMaxSlotsPerDay; ++i) {
            if (!editing_slots[i].active) {
              editing_slots[i] = {12, 0, 13, 0, true}; // Default new slot
              rebuild_day_slots_menu();
              if (g_menu_system)
                g_menu_system->request_redraw();
              break;
            }
          }
        }}};
    item_count++;
  }

  // Add Save
  day_slots_items[item_count] = {
      "< Save", MenuItemType::Action, {.action = []() {
        alarm_timer.clear_schedule(editing_day);
        for (int i = 0; i < AlarmTimer::kMaxSlotsPerDay; ++i) {
          if (editing_slots[i].active) {
            alarm_timer.add_time_slot(editing_day, editing_slots[i].start_hour,
                                      editing_slots[i].start_minute,
                                      editing_slots[i].end_hour,
                                      editing_slots[i].end_minute);
          }
        }
        if (g_menu_system)
          g_menu_system->on_left(); // Navigate back simulates Cancel/Back logic
      }}};
  item_count++;

  day_slots_menu.num_items = item_count;
}

static void prepare_day_menu(uint8_t day_index) {
  if (day_index > 6)
    return;
  editing_day = day_index;

  const TimeSlot *slots = alarm_timer.get_day_slots(day_index);
  if (slots) {
    for (int i = 0; i < AlarmTimer::kMaxSlotsPerDay; ++i) {
      editing_slots[i] = slots[i];
    }
  } else {
    for (int i = 0; i < AlarmTimer::kMaxSlotsPerDay; ++i) {
      editing_slots[i].active = false;
    }
  }

  rebuild_day_slots_menu();
}

// --- Timers Submenu ---

static const MenuItem timers_items[] = {
    {"Back", MenuItemType::Back, {}},
    {"Day 1 (Mon)",
     MenuItemType::DynamicSubMenu,
     {.dynamic_submenu = {&day_slots_menu,
                          []() {
                            prepare_day_menu(1);
                            day_slots_menu.title = "Monday";
                          }}}},
    {"Day 2 (Tue)",
     MenuItemType::DynamicSubMenu,
     {.dynamic_submenu = {&day_slots_menu,
                          []() {
                            prepare_day_menu(2);
                            day_slots_menu.title = "Tuesday";
                          }}}},
    {"Day 3 (Wed)",
     MenuItemType::DynamicSubMenu,
     {.dynamic_submenu = {&day_slots_menu,
                          []() {
                            prepare_day_menu(3);
                            day_slots_menu.title = "Wednesday";
                          }}}},
    {"Day 4 (Thu)",
     MenuItemType::DynamicSubMenu,
     {.dynamic_submenu = {&day_slots_menu,
                          []() {
                            prepare_day_menu(4);
                            day_slots_menu.title = "Thursday";
                          }}}},
    {"Day 5 (Fri)",
     MenuItemType::DynamicSubMenu,
     {.dynamic_submenu = {&day_slots_menu,
                          []() {
                            prepare_day_menu(5);
                            day_slots_menu.title = "Friday";
                          }}}},
    {"Day 6 (Sat)",
     MenuItemType::DynamicSubMenu,
     {.dynamic_submenu = {&day_slots_menu,
                          []() {
                            prepare_day_menu(6);
                            day_slots_menu.title = "Saturday";
                          }}}},
    {"Day 7 (Sun)",
     MenuItemType::DynamicSubMenu,
     {.dynamic_submenu = {&day_slots_menu,
                          []() {
                            prepare_day_menu(0);
                            day_slots_menu.title = "Sunday";
                          }}}},
};

const MenuNode timers_menu = {
    "Timers Menu", sizeof(timers_items) / sizeof(timers_items[0]), timers_items,
    &root_menu // Parent is the root menu
};

// --- Set Temp Submenu ---

static MenuItem set_temp_items[] = {
    {"Cancel", MenuItemType::Back, {}},
    {"Temperature",
     MenuItemType::ValueInt,
     {}}, // Pointers initialized in setup_menu
    {"< Save", MenuItemType::Action, {.action = []() {
       app_settings.set_boiler_temp(menu_boiler_temp);
       log_core0.info("[MENU] Boiler Temp changed and saved: %d",
                      app_settings.get_boiler_temp());
       if (g_menu_system)
         g_menu_system->on_left(); // Go back
     }}}};

const MenuNode set_temp_menu = {
    "Boiler", sizeof(set_temp_items) / sizeof(set_temp_items[0]),
    set_temp_items, &root_menu};

// --- Set Pump Delay Submenu ---

static MenuItem set_pump_delay_items[] = {
    {"Cancel", MenuItemType::Back, {}},
    {"Delay", MenuItemType::ValueInt, {}}, // Pointers initialized in setup_menu
    {"< Save", MenuItemType::Action, {.action = []() {
       app_settings.set_pump_run_time_sec(menu_pump_delay_sec);
       log_core0.info("[MENU] Pump Delay changed and saved: %d",
                      app_settings.get_pump_run_time_sec());
       if (g_menu_system)
         g_menu_system->on_left(); // Go back
     }}}};

const MenuNode set_pump_delay_menu = {"Pump Delay",
                                      sizeof(set_pump_delay_items) /
                                          sizeof(set_pump_delay_items[0]),
                                      set_pump_delay_items, &root_menu};

// --- Run Time Submenu ---

static MenuItem run_time_items[2];
static char run_time_str[32];
static MenuNode run_time_menu = {"Run Time", 2, run_time_items, &root_menu};

static void prepare_run_time_menu() {
  run_time_items[0] = {"Back", MenuItemType::Back, {}};

  uint32_t total_sec = app_settings.get_burner_runtime();
  uint32_t hours = total_sec / 3600;
  uint32_t mins = (total_sec % 3600) / 60;
  uint32_t secs = total_sec % 60;

  snprintf(run_time_str, sizeof(run_time_str), "%lu HRS %02lu MIN", hours,
           mins);
  run_time_items[1] = {run_time_str, MenuItemType::Action, {.action = []() {}}};
}

// --- Main Menu Structure ---

static const MenuItem root_items[] = {
    {"Timers", MenuItemType::SubMenu, {.submenu = &timers_menu}},
    {"Temperature",
     MenuItemType::DynamicSubMenu,
     {.dynamic_submenu = {&set_temp_menu,
                          []() {
                            menu_boiler_temp = app_settings.get_boiler_temp();
                          }}}},
    {"Pump Delay",
     MenuItemType::DynamicSubMenu,
     {.dynamic_submenu = {&set_pump_delay_menu,
                          []() {
                            menu_pump_delay_sec =
                                app_settings.get_pump_run_time_sec();
                          }}}},
    {"Burner Runtime",
     MenuItemType::DynamicSubMenu,
     {.dynamic_submenu = {&run_time_menu, prepare_run_time_menu}}},
    {"Exit", MenuItemType::Action, {.action = on_save_settings}}};

const MenuNode root_menu = {
    "Settings", sizeof(root_items) / sizeof(root_items[0]), root_items,
    nullptr // No parent for root
};

void setup_menu(MenuSystem &menu_system) {
  g_menu_system = &menu_system;

  // Initialize buffer from settings
  menu_boiler_temp = app_settings.get_boiler_temp();

  // Setup the ValueInt handler for "Target Temp" in set_temp_items
  set_temp_items[1].data.int_val.value = &menu_boiler_temp;
  set_temp_items[1].data.int_val.min_val = AppSettings::kMinBoilerTemp;
  set_temp_items[1].data.int_val.max_val = AppSettings::kMaxBoilerTemp;
  set_temp_items[1].data.int_val.step = 1;
  set_temp_items[1].data.int_val.on_change =
      nullptr; // Confirmed on Save action instead
  set_temp_items[1].data.int_val.format_suffix = "\x7F"
                                                 "C";

  // Setup the ValueInt handler for "Delay" in set_pump_delay_items
  set_pump_delay_items[1].data.int_val.value = &menu_pump_delay_sec;
  set_pump_delay_items[1].data.int_val.min_val = 0;
  set_pump_delay_items[1].data.int_val.max_val = AppSettings::kMaxPumpRunTime;
  set_pump_delay_items[1].data.int_val.step = 30; // 30 sec increments
  set_pump_delay_items[1].data.int_val.on_change = nullptr;
  set_pump_delay_items[1].data.int_val.format_suffix = " Sec";

  menu_system.set_root_menu(&root_menu);
}

} // namespace boiler
