#pragma once

#include <BAN/Math.h>

#include <fcntl.h>
#include <sys/framebuffer.h>
#include <sys/mman.h>
#include <termios.h>

#include <kernel/Input/MouseEvent.h>

extern framebuffer_info_t fb_info;
extern void* fb_mmap;
extern int mouse_fd;
extern termios original_termios;

void put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    // Ensure x and y are within bounds
    if (x < fb_info.width && y < fb_info.height)
    {
        // Calculate the offset within the framebuffer
        uint32_t offset = y * fb_info.width + x;
        // Ensure the offset is within the framebuffer size
        if (offset < (fb_info.width * fb_info.height))
        {
            // Set the pixel color
            static_cast<uint32_t*>(fb_mmap)[offset] = color;
        }
    }
}

void draw_circle(uint32_t cx, int cy, uint32_t r, uint32_t color)
{
	uint32_t min_x = BAN::Math::max<uint32_t>(cx - r, 0);
	uint32_t max_x = BAN::Math::min<uint32_t>(cx + r + 1, fb_info.width);

	uint32_t min_y = BAN::Math::max<uint32_t>(cy - r, 0);
	uint32_t max_y = BAN::Math::min<uint32_t>(cy + r + 1, fb_info.height);

	for (uint32_t y = min_y; y < max_y; y++)
	{
		for (uint32_t x = min_x; x < max_x; x++)
		{
			uint32_t dx = x - cx;
			uint32_t dy = y - cy;
			if (dx * dx + dy * dy > r * r)
				continue;
			put_pixel(x, y, color);
		}
	}
}

void draw_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    // Ensure the rectangle is within bounds
    if (x >= fb_info.width || y >= fb_info.height)
        return;

    // Calculate the effective width and height, clipping to the framebuffer bounds
    uint32_t max_width = (x + width > fb_info.width) ? fb_info.width - x : width;
    uint32_t max_height = (y + height > fb_info.height) ? fb_info.height - y : height;

    // Prepare a row of pixels to copy
    uint32_t* row_buffer = new uint32_t[max_width];
    for (uint32_t i = 0; i < max_width; ++i)
    {
        row_buffer[i] = color;
    }

    // Get the starting address in the framebuffer memory
    uint32_t* fb_ptr = static_cast<uint32_t*>(fb_mmap);

    // Draw the rectangle row by row
    for (uint32_t j = 0; j < max_height; ++j)
    {
        // Calculate the offset for the current row
        uint32_t offset = (y + j) * fb_info.width + x;
        // Use memcpy to copy the row buffer to the framebuffer memory
        memcpy(fb_ptr + offset, row_buffer, max_width * sizeof(uint32_t));
    }

    // Clean up the row buffer
    delete[] row_buffer;
}

void draw_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t color)
{
    uint32_t dx = BAN::Math::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    uint32_t dy = -BAN::Math::abs(y1 - y0), sy = y0 < y1 ? 1 : -1; 
    uint32_t err = dx + dy, e2;

    while (true)
    {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_horizontal_line(uint32_t x, uint32_t y, uint32_t length, uint32_t color)
{
    // Ensure the line starts within bounds
    if (y >= fb_info.height || x >= fb_info.width)
        return;

    // Calculate the effective length, clipping to the framebuffer bounds
    uint32_t max_length = (x + length > fb_info.width) ? fb_info.width - x : length;

    // Prepare a row of pixels to copy
    uint32_t* row_buffer = new uint32_t[max_length];
    for (uint32_t i = 0; i < max_length; ++i)
    {
        row_buffer[i] = color;
    }

    // Get the starting address in the framebuffer memory
    uint32_t* fb_ptr = static_cast<uint32_t*>(fb_mmap);

    // Calculate the offset for the starting position
    uint32_t offset = y * fb_info.width + x;
    // Use memcpy to copy the row buffer to the framebuffer memory
    memcpy(fb_ptr + offset, row_buffer, max_length * sizeof(uint32_t));

    // Clean up the row buffer
    delete[] row_buffer;
}

void draw_vertical_line(uint32_t x, uint32_t y, uint32_t length, uint32_t color)
{
    // Ensure the line starts within bounds
    if (x >= fb_info.width || y >= fb_info.height)
        return;

    // Calculate the effective length, clipping to the framebuffer bounds
    uint32_t max_length = (y + length > fb_info.height) ? fb_info.height - y : length;

    // Get the starting address in the framebuffer memory
    uint32_t* fb_ptr = static_cast<uint32_t*>(fb_mmap);

    // Draw the line column by column
    for (uint32_t i = 0; i < max_length; ++i)
    {
        uint32_t offset = (y + i) * fb_info.width + x;
        fb_ptr[offset] = color;
    }
}

void cleanup()
{
	if (fb_mmap)
		munmap(fb_mmap, fb_info.height * fb_info.width * (BANAN_FB_BPP / 8));
	if (mouse_fd != -1)
		close(mouse_fd);
	if (original_termios.c_lflag & ECHO)
		tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}