#include <BAN/Math.h>
#include <BAN/String.h>

#include <fcntl.h>
#include <sys/framebuffer.h>
#include <sys/mman.h>
#include <termios.h>

#include <kernel/Input/MouseEvent.h>
#include "./drawing.hpp"
#include "./gui.hpp"

framebuffer_info_t fb_info;
void* fb_mmap = nullptr;

int mouse_fd = -1;

termios original_termios {};



int main(int argc, char** argv)
{
	const char* fb_path = "/dev/fb0";
	const char* mouse_path = "/dev/input1";

	if (argc == 1)
		;
	else if (argc == 3)
	{
		fb_path = argv[1];
		mouse_path = argv[2];
	}
	else
	{
		fprintf(stderr, "usage: %s [FB_PATH MOUSE_PATH]", argv[0]);
		return 1;
	}

	signal(SIGINT, [](int) { exit(0); });
	if (atexit(cleanup) == -1)
	{
		perror("atexit");
		return 1;
	}

	if (BANAN_FB_BPP != 32)
	{
		fprintf(stderr, "unsupported bpp\n");
		return 1;
	}

	int fb_fd = open(fb_path, O_RDWR);
	if (fb_fd == -1)
	{
		fprintf(stderr, "open: ");
		perror(fb_path);
		return 1;
	}

	if (pread(fb_fd, &fb_info, sizeof(fb_info), -1) == -1)
	{
		fprintf(stderr, "read: ");
		perror(fb_path);
		return 1;
	}

	size_t fb_bytes = fb_info.width * fb_info.height * (BANAN_FB_BPP / 8);
	fb_mmap = mmap(nullptr, fb_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	close(fb_fd);
	if (fb_mmap == MAP_FAILED)
	{
		fprintf(stderr, "mmap: ");
		perror(fb_path);
		return 1;
	}

	int mouse_fd = open(mouse_path, O_RDONLY);
	if (mouse_fd == -1)
	{
		fprintf(stderr, "open: ");
		perror(mouse_path);
		return 1;
	}

	if (tcgetattr(STDIN_FILENO, &original_termios) == -1)
	{
		perror("tcgetattr");
		return 1;
	}

	termios termios = original_termios;
	termios.c_lflag &= ~ECHO;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &termios) == -1)
	{
		perror("tcsetattr");
		return 1;
	}



	// Rendering
	memset(fb_mmap, 0x000000, fb_bytes);
	msync(fb_mmap, fb_bytes, MS_SYNC);

	Window* mainWindow = new Window(50, 50, 300, 200, BAN::StringView("Main Window"));

    Label* label1 = new Label(10, 10, 100, 20, BAN::StringView("Label 1"), 0xFF0000); // Red color
    Button* button1 = new Button(10, 40, 60, 20, BAN::StringView("Button 1"), 0x0000FF, 0xFFFFFF); // Blue BG color

    mainWindow->addElement(label1);
    mainWindow->addElement(button1);


	while (true)
	{
		draw_rectangle(0, 0, fb_info.width, fb_info.height, 0x008080);
		mainWindow->draw();
		msync(fb_mmap, fb_bytes, MS_SYNC);
	}


	return 0;
}
