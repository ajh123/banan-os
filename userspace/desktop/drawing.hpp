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

void put_pixel(int x, int y, uint32_t color)
{
	static_cast<uint32_t*>(fb_mmap)[y * fb_info.width + x] = color;
}

void draw_circle(int cx, int cy, int r, uint32_t color)
{
	int min_x = BAN::Math::max<int>(cx - r, 0);
	int max_x = BAN::Math::min<int>(cx + r + 1, fb_info.width);

	int min_y = BAN::Math::max<int>(cy - r, 0);
	int max_y = BAN::Math::min<int>(cy + r + 1, fb_info.height);

	for (int y = min_y; y < max_y; y++)
	{
		for (int x = min_x; x < max_x; x++)
		{
			int dx = x - cx;
			int dy = y - cy;
			if (dx * dx + dy * dy > r * r)
				continue;
			put_pixel(x, y, color);
		}
	}
}

void draw_rectangle(int x, int y, int width, int height, uint32_t color)
{
    for (int i = x; i < x+width; i++)
    {
        for (int j = y; j < y+height; j++)
        {
            put_pixel(j, i, color);
        }
    }
}

void draw_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = BAN::Math::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -BAN::Math::abs(y1 - y0), sy = y0 < y1 ? 1 : -1; 
    int err = dx + dy, e2;

    while (true)
    {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
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