#ifndef _SYS_BANAN_OS_H
#define _SYS_BANAN_OS_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t 1
#include <stddef.h>

#define TTY_CMD_SET		0x01
#define TTY_CMD_UNSET	0x02

#define TTY_FLAG_ENABLE_OUTPUT	1
#define TTY_FLAG_ENABLE_INPUT	2

#define POWEROFF_SHUTDOWN 0
#define POWEROFF_REBOOT 1

struct proc_meminfo_t
{
	size_t page_size;
	size_t virt_pages;
	size_t phys_pages;
};

/*
fildes:		refers to valid tty device
command:	one of TTY_CMD_* definitions
flags:		bitwise or of TTY_FLAG_* definitions

return value: 0 on success, -1 on failure and errno set to the error
*/
int tty_ctrl(int fildes, int command, int flags);
int poweroff(int command);

int load_keymap(const char* path);

__END_DECLS

#endif
