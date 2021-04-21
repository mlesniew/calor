#include <vector>
#include <memory>

#include "buttons.h"
#include <LiquidCrystal_I2C.h>

constexpr size_t LCD_COLS = 20;
constexpr size_t LCD_ROWS = 4;

class Screen {
public:
    virtual void reset() {}
    virtual bool tick(LiquidCrystal_I2C & lcd, Buttons & buttons);
    virtual void refresh(LiquidCrystal_I2C & lcd);
};

struct MenuItem {
    virtual std::string get_text_left() const = 0;

    virtual std::string get_text_right() const {
        return "";
    }

    virtual void on_left() { };
    virtual void on_right() { };
};

struct MenuItemNoAction : public MenuItem {
    MenuItemNoAction(const std::string & text): text(text) {}
    std::string get_text_left() const override { return text; }
    const std::string text;
};

struct MenuItemWithValue : public MenuItemNoAction {
    MenuItemWithValue(const std::string & text, const std::function<std::string()> value_getter)
        : MenuItemNoAction(text), value_getter(value_getter) {}

    std::string get_text_right() const override { return value_getter(); }

    const std::function<std::string()> value_getter;
};

typedef std::unique_ptr<MenuItem> MenuItemPtr;

class Menu : public Screen {
public:
    Menu() { reset(); }

    void reset() {
        screen_idx = 0;
        selection_idx = 0;
        refresh_required = true;
    }

    void refresh(LiquidCrystal_I2C & lcd) {
        if (selection_idx < screen_idx) {
            screen_idx = selection_idx;
        } else if (selection_idx >= screen_idx + LCD_ROWS) {
            screen_idx = selection_idx - LCD_ROWS + 1;
        }

        for (unsigned int row = 0; row < LCD_ROWS; ++row) {
            refresh_row(lcd, row);
        }

        stopwatch.reset();
    }

    void refresh_row(LiquidCrystal_I2C & lcd, unsigned int row) {
        const unsigned int item_index = screen_idx + row;
        const bool is_selected = (item_index == selection_idx);
        lcd.setCursor(0, row);
        lcd.print(is_selected ? "\176" : " ");

        std::string text_left;
        std::string text_right;

        if (item_index < items.size()) {
            text_left = items[item_index]->get_text_left().substr(0, LCD_COLS - 1);
            text_right = items[item_index]->get_text_right().substr(0, LCD_COLS - 1);
        }

        lcd.print(text_left.c_str());

        for (unsigned int col = 1 + text_left.size(); col < LCD_COLS - text_right.size(); ++col)
            lcd.print(" ");

        lcd.setCursor(LCD_COLS - text_right.size(), row);
        lcd.print(text_right.c_str());
    }

    bool tick(LiquidCrystal_I2C & lcd, Buttons & buttons) override {
        refresh_required = refresh_required || stopwatch.elapsed() > 1;

        if (items.empty())
            return false;

        const auto button = buttons.get_button();

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
            default:
                /* nop */
                break;
        }

        if (refresh_required)
            refresh(lcd);

        return true;
    }

    void add_item(MenuItemPtr && item) {
        items.push_back(std::move(item));
    }

    void add_item(const std::string & text, std::function<std::string()> value_getter) {
        auto item = MenuItemPtr(new MenuItemWithValue(text, value_getter));
        add_item(std::move(item));
    }

    void add_item(const std::string & text, std::function<double()> value_getter) {
        auto wrapped_getter = [value_getter]() {
            const double value = value_getter();
            char text[LCD_COLS];
            snprintf(text, LCD_COLS, "%.1f", value);
            return std::string(text);
        };
        add_item(text, wrapped_getter);
    }

    void add_item_on_off(const std::string & text, std::function<bool()> value_getter) {
        auto wrapped_getter = [value_getter]() {
            return std::string(value_getter() ? "on": "off");
        };
        add_item(text, wrapped_getter);
    }

    void add_item(const std::string & text) {
        auto item = MenuItemPtr(new MenuItemNoAction(text));
        add_item(std::move(item));
    }

protected:
    bool refresh_required;
    std::vector<MenuItemPtr> items;
    unsigned int screen_idx;
    unsigned int selection_idx;
    Stopwatch stopwatch;
};
