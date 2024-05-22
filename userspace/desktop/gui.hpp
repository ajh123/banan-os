#pragma once

#include <BAN/String.h>
#include <BAN/Vector.h>

#include "./drawing.hpp"
#include "./font.hpp"

// Base class for all GUI elements
class GUIElement {
protected:
    int x, y, width, height;
    GUIElement* parent;

    // Calculate absolute position based on parent's position
    void get_absolute_position(int& abs_x, int& abs_y) {
        if (parent) {
            int parent_abs_x, parent_abs_y;
            parent->get_absolute_position(parent_abs_x, parent_abs_y);
            abs_x = parent_abs_x + x;
            abs_y = parent_abs_y + y;
        } else {
            abs_x = x;
            abs_y = y;
        }
    }

public:
    GUIElement(int x, int y, int width, int height)
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
    Label(int x, int y, int width, int height, BAN::String text, uint32_t color)
        : GUIElement(x, y, width, height), text(text), color(color) {}

    void setText(BAN::String newText) {
        text = newText;
    }

    BAN::String getText() const {
        return text;
    }

    void draw() override {
        int abs_x, abs_y;
        get_absolute_position(abs_x, abs_y);
        draw_text(abs_x, abs_y, text, color);
    }
};

// Class for Button elements
class Button : public GUIElement {
private:
    BAN::String text;
    uint32_t color;
public:
    Button(int x, int y, int width, int height, BAN::String text, uint32_t color)
        : GUIElement(x, y, width, height), text(text), color(color) {}

    void setText(BAN::String newText) {
        text = newText;
    }

    BAN::String getText() const {
        return text;
    }

    void draw() override {
        int abs_x, abs_y;
        get_absolute_position(abs_x, abs_y);
        draw_rectangle(abs_x, abs_y, width, height, color);
        // Center the text within the button (for simplicity, only horizontally centered)
        draw_text_centered(abs_x, abs_y, width, text, color);
    }
};

// Class for Window elements
class Window : public GUIElement {
private:
    BAN::String title;
    BAN::Vector<GUIElement*> children;
    uint32_t color;
public:
    Window(int x, int y, int width, int height, BAN::String title, uint32_t color)
        : GUIElement(x, y, width, height), title(title), color(color) {}

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
        int abs_x, abs_y;
        get_absolute_position(abs_x, abs_y);
        draw_rectangle(abs_x, abs_y, width, height, color);
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
