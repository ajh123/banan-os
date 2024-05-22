#pragma once

#include <BAN/String.h>
#include <BAN/Vector.h>

#include "./drawing.hpp"
#include "./font.hpp"

// Base class for all GUI elements
class GUIElement {
protected:
    uint32_t x, y, width, height;
    GUIElement* parent;

    // Calculate absolute position based on parent's position
    void get_absolute_position(uint32_t& abs_x, uint32_t& abs_y) {
        if (parent) {
            uint32_t parent_abs_x, parent_abs_y;
            parent->get_absolute_position(parent_abs_x, parent_abs_y);
            abs_x = parent_abs_x + x;
            abs_y = parent_abs_y + y;
        } else {
            abs_x = x;
            abs_y = y;
        }
    }

public:
    GUIElement(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
        : x(x), y(y), width(width), height(height), parent(nullptr) {}

    void setParent(GUIElement* parentElement) {
        parent = parentElement;
    }

    virtual void draw() = 0; // Pure virtual function to draw the element
    virtual ~GUIElement() = default; // Virtual destructor
};

// Class for Label elements
class Label : public GUIElement {
private:
    BAN::String text;
    uint32_t color;
public:
    Label(uint32_t x, uint32_t y, uint32_t width, uint32_t height, BAN::String text, uint32_t color)
        : GUIElement(x, y, width, height), text(text), color(color) {}

    void setText(BAN::String newText) {
        text = newText;
    }

    BAN::String getText() const {
        return text;
    }

    void draw() override {
        uint32_t abs_x, abs_y;
        get_absolute_position(abs_x, abs_y);
        draw_text(abs_x, abs_y, text, color);
    }
};

// Class for Button elements
class Button : public GUIElement {
private:
    BAN::String text;
    uint32_t color;
    uint32_t text_color;
public:
    Button(uint32_t x, uint32_t y, uint32_t width, uint32_t height, BAN::String text, uint32_t color, uint32_t text_color)
        : GUIElement(x, y, width, height), text(text), color(color), text_color(text_color) {}

    void setText(BAN::String newText) {
        text = newText;
    }

    BAN::String getText() const {
        return text;
    }

    void draw() override {
        uint32_t abs_x, abs_y;
        get_absolute_position(abs_x, abs_y);
        draw_rectangle(abs_x, abs_y, width, height, color);
        // Center the text within the button (for simplicity, only horizontally centered)
        draw_text_centered(abs_x, abs_y, width, text, text_color);
    }
};

// Class for Window elements
class Window : public GUIElement {
private:
    BAN::String title;
    BAN::Vector<GUIElement*> children;
public:
    Window(uint32_t x, uint32_t y, uint32_t width, uint32_t height, BAN::String title)
        : GUIElement(x, y, width, height), title(title) {}

    void setTitle(BAN::String newTitle) {
        title = newTitle;
    }

    BAN::String getTitle() const {
        return title;
    }

    void addElement(GUIElement* element) {
        element->setParent(this);
        MUST(children.push_back(element));
    }

    void draw() override {
        uint32_t abs_x, abs_y;
        get_absolute_position(abs_x, abs_y);
        draw_rectangle(abs_x, abs_y, width, height, 0x000000);
        draw_rectangle(abs_x + 1, abs_y + 1, width - 1 * 2, height - 1 * 2, 0xC0C0C0);
        draw_rectangle(abs_x + 4, abs_y + 4, width - 4 * 2, height - 4 * 2, 0x000000);
        draw_rectangle(abs_x + 5, abs_y + 5, width - 5 * 2, height - 5 * 2, 0xFFFFFF);
        for (auto& child : children) {
            child->draw();
        }
    }

    ~Window() {
        for (auto& child : children) {
            delete child;
        }
    }
};
