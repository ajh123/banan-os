#pragma once

#include <BAN/String.h>
#include <BAN/Vector.h>

#include "./drawing.hpp"
#include "./font.hpp"

const uint8_t mouse_icon[16][11] =  {
	{1,0,0,0,0,0,0,0,0,0,0},
	{1,1,0,0,0,0,0,0,0,0,0},
	{1,2,1,0,0,0,0,0,0,0,0},
	{1,2,2,1,0,0,0,0,0,0,0},
	{1,2,2,2,1,0,0,0,0,0,0},
	{1,2,2,2,2,1,0,0,0,0,0},
	{1,2,2,2,2,2,1,0,0,0,0},
	{1,2,2,2,2,2,2,1,0,0,0},
	{1,2,2,2,2,2,2,2,1,0,0},
	{1,2,2,2,2,2,2,2,2,1,0},
	{1,2,2,2,2,1,1,1,1,1,1},
	{1,2,2,2,1,0,0,0,0,0,0},
	{1,2,2,1,0,0,0,0,0,0,0},
	{1,2,1,0,0,0,0,0,0,0,0},
	{1,1,0,0,0,0,0,0,0,0,0},
	{1,0,0,0,0,0,0,0,0,0,0},
};

uint32_t mouse_color_mapping[] = {0, 0xFFFFFFFF, 0};

void draw_window_border(uint32_t abs_x, uint32_t abs_y, uint32_t width, uint32_t height, uint32_t border_width, uint32_t edge_color, uint32_t mid_color)
{
    // Ensure the border width does not exceed the dimensions of the window
    border_width = BAN::Math::min<uint32_t>(border_width, width / 2);
    border_width = BAN::Math::min<uint32_t>(border_width, height / 2);

    uint32_t outer_x = abs_x - border_width;
    uint32_t outer_y = abs_y - border_width;
    uint32_t outer_width = width + 2 * border_width;
    uint32_t outer_height = height + 2 * border_width;

    // Draw the edge border (first and last lines)
    for (uint32_t i = 0; i < border_width; ++i)
    {
        if (i == 0 || i == border_width - 1)
        {
            // Draw top edge border
            draw_horizontal_line(outer_x + i, outer_y + i, outer_width - 2 * i, edge_color);
            // Draw bottom edge border
            draw_horizontal_line(outer_x + i, outer_y + outer_height - i - 1, outer_width - 2 * i, edge_color);
            // Draw left edge border
            draw_vertical_line(outer_x + i, outer_y + i, outer_height - 2 * i, edge_color);
            // Draw right edge border
            draw_vertical_line(outer_x + outer_width - i - 1, outer_y + i, outer_height - 2 * i, edge_color);
        }
    }

    // Draw the middle border
    for (uint32_t i = 1; i < border_width - 1; ++i)
    {
        // Draw top middle border
        draw_horizontal_line(outer_x + i, outer_y + i, outer_width - 2 * i, mid_color);
        // Draw bottom middle border
        draw_horizontal_line(outer_x + i, outer_y + outer_height - i - 1, outer_width - 2 * i, mid_color);
        // Draw left middle border
        draw_vertical_line(outer_x + i, outer_y + i, outer_height - 2 * i, mid_color);
        // Draw right middle border
        draw_vertical_line(outer_x + outer_width - i - 1, outer_y + i, outer_height - 2 * i, mid_color);
    }
}

void draw_mouse(uint32_t abs_x, uint32_t abs_y)
{
    for (int row = 0; row < 16; ++row) {
        for (int col = 0; col < 11; ++col) {
            uint8_t data = mouse_icon[row][col];
            if (data > 0) {
                uint32_t color = mouse_color_mapping[data];
                put_pixel(row+abs_x, col+abs_y, color);
            }
        }
    }
}

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
        // Background
        draw_rectangle(abs_x, abs_y, width, height, 0xFFFFFF);
        // Border
        draw_window_border(abs_x, abs_y, width, height, 4, 0x000000, 0xC0C0C0);

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

