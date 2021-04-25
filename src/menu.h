#include <vector>
#include <memory>

#include "buttons.h"
#include <LiquidCrystal_I2C.h>

constexpr size_t LCD_COLS = 20;
constexpr size_t LCD_ROWS = 4;

class Screen {
public:
    virtual void reset() {}
    virtual bool tick(Buttons & buttons);
    virtual void refresh(LiquidCrystal_I2C & lcd);
};

enum class MenuAction { none, recurse, exit };

struct MenuActionFunction {
    MenuActionFunction(std::function<MenuAction()> function): function(function) {}
    MenuActionFunction(std::function<void()> fn): function([fn]{ fn(); return MenuAction::none; }) {}
    MenuActionFunction(MenuAction action) : function([action]{ return action; }) {}
    MenuActionFunction() : function([]{ return MenuAction::none; }) {}

    MenuAction operator()() { return function(); }

    const std::function<MenuAction()> function;
};

struct MenuItem {
    static std::string no_value() { return ""; }

    MenuItem(
            const std::string & title,
            const std::function<std::string()> value_getter=no_value,
            const MenuActionFunction on_left=MenuAction::none,
            const MenuActionFunction on_right=MenuAction::none,
            const MenuActionFunction on_enter=MenuAction::none,
            Screen * screen = nullptr):
        title(title), value_getter(value_getter),
        on_left(on_left), on_right(on_right), on_enter(on_enter),
        screen(screen) {}

    MenuItem(
            const std::string & title,
            Screen * screen)
        : MenuItem(title, no_value, MenuAction::none, MenuAction::none, MenuAction::recurse, screen) {}

    static MenuItem label(const std::string & title) {
        return MenuItem(title, no_value, MenuAction::none, MenuAction::none, MenuAction::none, nullptr);
    }

    static MenuItem exit(const std::string & title = "Back") {
        return MenuItem(title, no_value, MenuAction::none, MenuAction::none, MenuAction::exit, nullptr);
    }

    static MenuItem submenu(const std::string & title, Screen & screen) {
        return MenuItem(title, no_value, MenuAction::none, MenuAction::none, MenuAction::recurse, &screen);
    }

    static MenuItem value(const std::string & title, std::function<std::string()> getter) {
        return MenuItem(title, getter);
    }

    static MenuItem on_off(const std::string & title, std::function<bool()> value_getter) {
        auto wrapped_getter = [value_getter]() {
            return std::string(value_getter() ? "on": "off");
        };
        return value(title, wrapped_getter);
    }

    static MenuItem value(const std::string & title, std::function<double()> value_getter) {
        auto wrapped_getter = [value_getter]() {
            const double value = value_getter();
            char text[LCD_COLS];
            snprintf(text, LCD_COLS, "%.1f", value);
            return std::string(text);
        };
        return value(title, wrapped_getter);
    }

    const std::string title;
    const std::function<std::string()> value_getter;
    const std::function<MenuAction()> on_left, on_right, on_enter;
    Screen * const screen;
};

typedef std::unique_ptr<MenuItem> MenuItemPtr;

class Menu : public Screen {
public:
    Menu(std::initializer_list<MenuItem> items): items(items) {
        reset();
    }

    Menu() {
        reset();
    }

    void reset() {
        screen_idx = 0;
        selection_idx = 0;
        refresh_required = true;
        screen = nullptr;
    }

    void refresh(LiquidCrystal_I2C & lcd) {
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

    void refresh_row(LiquidCrystal_I2C & lcd, unsigned int row) {
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

    bool tick(Buttons & buttons) override {
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

    void add_item(MenuItem && item) {
        items.push_back(item);
    }

protected:
    bool refresh_required;
    std::vector<MenuItem> items;
    unsigned int screen_idx;
    unsigned int selection_idx;
    Stopwatch stopwatch;
    Screen * screen;
};
