#include <vector>
#include <memory>

#include <Arduino.h>

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

    static MenuItem value(const std::string & title, std::function<String()> getter) {
        auto wrapped_getter = [getter]() {
            return std::string(getter().c_str());
        };
        return value(title, wrapped_getter);
    }

    static MenuItem on_off(const std::string & title, std::function<bool()> value_getter) {
        auto wrapped_getter = [value_getter]() {
            return std::string(value_getter() ? "on": "off");
        };
        return value(title, wrapped_getter);
    }

    static MenuItem value(
            const std::string & title,
            std::function<double()> value_getter,
            const std::string & unit = "") {
        auto wrapped_getter = [value_getter, unit]() {
            const double value = value_getter();
            char text[LCD_COLS];
            snprintf(text, LCD_COLS, "%.1f%s", value, unit.c_str());
            return std::string(text);
        };
        return value(title, wrapped_getter);
    }

    const std::string title;
    const std::function<std::string()> value_getter;
    const std::function<MenuAction()> on_left, on_right, on_enter;
    Screen * const screen;
};

class Menu : public Screen {
public:
    Menu(std::initializer_list<MenuItem> items);

    void reset() override;
    void refresh(LiquidCrystal_I2C & lcd) override;
    bool tick(Buttons & buttons) override;

    void add_item(MenuItem && item);

protected:
    void refresh_row(LiquidCrystal_I2C & lcd, unsigned int row);

    bool refresh_required;
    std::vector<MenuItem> items;
    unsigned int screen_idx;
    unsigned int selection_idx;
    Stopwatch stopwatch;
    Screen * screen;
};
