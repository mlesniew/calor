#include "menu.h"

Menu::Menu(std::initializer_list<MenuItem> items): items(items) {
    reset();
}

void Menu::reset() {
    screen_idx = 0;
    selection_idx = 0;
    refresh_required = true;
    screen = nullptr;
}

void Menu::refresh(LiquidCrystal_I2C & lcd) {
    if (screen) {
        screen->refresh(lcd);
    } else if (refresh_required) {
        if (selection_idx < screen_idx) {
            screen_idx = selection_idx;
        } else if (selection_idx >= screen_idx + LCD_ROWS) {
            screen_idx = selection_idx - LCD_ROWS + 1;
        }

        for (unsigned int row = 0; row < LCD_ROWS; ++row) {
            refresh_row(lcd, row);
        }

        stopwatch.reset();
        refresh_required = false;
    }
}

void Menu::refresh_row(LiquidCrystal_I2C & lcd, unsigned int row) {
    const unsigned int item_index = screen_idx + row;
    const bool is_selected = (item_index == selection_idx);
    lcd.setCursor(0, row);
    lcd.print(is_selected ? "\176" : " ");

    std::string text_left;
    std::string text_right;

    if (item_index < items.size()) {
        text_left = items[item_index].title.substr(0, LCD_COLS - 1);
        text_right = items[item_index].value_getter().substr(0, LCD_COLS - 1);
    }

    lcd.print(text_left.c_str());

    for (unsigned int col = 1 + text_left.size(); col < LCD_COLS - text_right.size(); ++col)
        lcd.print(" ");

    lcd.setCursor(LCD_COLS - text_right.size(), row);
    lcd.print(text_right.c_str());
}

bool Menu::tick(Buttons & buttons) {
    if (screen) {
        if (!screen->tick(buttons)) {
            // menu option exited
            screen = nullptr;
            refresh_required = true;
        } else {
            return true;
        }
    }

    if (items.empty()) {
        return false;
    }

    if (selection_idx >= items.size()) {
        selection_idx = items.size() - 1;
        refresh_required = true;
    }

    refresh_required = refresh_required || stopwatch.elapsed() > 1;

    const auto & item = items[selection_idx];
    const auto button = buttons.get_button();
    MenuAction action = MenuAction::none;

    switch (button) {
        case Button::up:
            selection_idx += (items.size() - 1);
            selection_idx %= items.size();
            refresh_required = true;
            break;
        case Button::down:
            selection_idx += (items.size() + 1);
            selection_idx %= items.size();
            refresh_required = true;
            break;
        case Button::left:
            action = item.on_left();
            refresh_required = true;
            break;
        case Button::right:
            action = item.on_right();
            refresh_required = true;
            break;
        case Button::ok:
            action = item.on_enter();
            refresh_required = true;
            break;
        default:
            /* nop */
            break;
    }

    switch (action) {
        case MenuAction::recurse:
            if (item.screen) {
                screen = item.screen;
                screen->reset();
            }
            refresh_required = true;
            return true;
        case MenuAction::exit:
            return false;
        default:
            return true;
    }
}

void Menu::add_item(MenuItem && item) {
    items.push_back(item);
}
