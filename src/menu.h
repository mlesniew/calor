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
    virtual std::string get_text() const = 0;
};

struct MenuItemNoAction : public MenuItem {
    MenuItemNoAction(const std::string & text): text(text) {}
    std::string get_text() const override { return text; }
    const std::string text;
};

typedef std::unique_ptr<MenuItem> MenuItemPtr;

class Menu : public Screen {
public:
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
    }

    void refresh_row(LiquidCrystal_I2C & lcd, unsigned int row) {
        const unsigned int item_index = screen_idx + row;
        const bool is_selected = (item_index == selection_idx);
        lcd.setCursor(0, row);
        lcd.print(is_selected ? "\176" : " ");

        std::string text;

        if (item_index <= items.size()) {
            text = items[item_index]->get_text();
        }

        // TODO: trim to display width
        lcd.print(text.c_str());

        for (unsigned int col = 1 + text.size(); col < LCD_COLS; ++col)
            lcd.print(" ");
    }

    bool tick(LiquidCrystal_I2C & lcd, Buttons & buttons) override {
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

    void add_item(const std::string & text) {
        auto item = MenuItemPtr(new MenuItemNoAction(text));
        add_item(std::move(item));
    }

protected:
    bool refresh_required;
    std::vector<MenuItemPtr> items;
    unsigned int screen_idx;
    unsigned int selection_idx;
};
