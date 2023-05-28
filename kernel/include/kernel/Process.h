#pragma once

#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/FS/Inode.h>
#include <kernel/Memory/FixedWidthAllocator.h>
#include <kernel/Memory/GeneralAllocator.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MMU.h>
#include <kernel/Memory/VirtualRange.h>
#include <kernel/SpinLock.h>
#include <kernel/Terminal/TTY.h>
#include <kernel/Thread.h>

#include <sys/stat.h>

namespace Kernel
{

	class Process
	{
		BAN_NON_COPYABLE(Process);
		BAN_NON_MOVABLE(Process);

	public:
		using entry_t = Thread::entry_t;

	public:
		static Process* create_kernel(entry_t, void*);
		static BAN::ErrorOr<Process*> create_userspace(BAN::StringView);
		~Process();

		[[noreturn]] void exit();

		void add_thread(Thread*);
		void on_thread_exit(Thread&);

		BAN::ErrorOr<void> init_stdio();
		BAN::ErrorOr<void> set_termios(const termios&);

		pid_t pid() const { return m_pid; }

		BAN::ErrorOr<int> open(BAN::StringView, int);
		BAN::ErrorOr<void> close(int fd);
		BAN::ErrorOr<size_t> read(int fd, void* buffer, size_t count);
		BAN::ErrorOr<size_t> write(int fd, const void* buffer, size_t count);
		BAN::ErrorOr<void> creat(BAN::StringView name, mode_t);

		BAN::ErrorOr<void> seek(int fd, off_t offset, int whence);
		BAN::ErrorOr<off_t> tell(int fd);

		BAN::ErrorOr<void> fstat(int fd, struct stat*);
		BAN::ErrorOr<void> stat(BAN::StringView path, struct stat*);

		BAN::ErrorOr<void> mount(BAN::StringView source, BAN::StringView target);

		BAN::ErrorOr<BAN::Vector<BAN::String>> read_directory_entries(int);

		BAN::ErrorOr<BAN::String> working_directory() const;
		BAN::ErrorOr<void> set_working_directory(BAN::StringView);

		TTY& tty() { ASSERT(m_tty); return *m_tty; }

		BAN::ErrorOr<void*> allocate(size_t);
		void free(void*);

		void termid(char*) const;

		static Process& current() { return Thread::current().process(); }

		MMU& mmu() { return m_mmu ? *m_mmu : MMU::kernel(); }

	private:
		Process(pid_t);
		static Process* create_process();
		static void register_process(Process*);

		BAN::ErrorOr<BAN::String> absolute_path_of(BAN::StringView) const;

	private:
		struct OpenFileDescription
		{
			BAN::RefPtr<Inode> inode;
			BAN::String path;
			off_t offset { 0 };
			uint8_t flags { 0 };
		};

		BAN::ErrorOr<void> validate_fd(int);
		OpenFileDescription& open_file_description(int);
		BAN::ErrorOr<int> get_free_fd();

		BAN::Vector<OpenFileDescription> m_open_files;
		BAN::Vector<VirtualRange*> m_mapped_ranges;

		mutable RecursiveSpinLock m_lock;

		const pid_t m_pid = 0;
		BAN::String m_working_directory;
		BAN::Vector<Thread*> m_threads;

		BAN::Vector<FixedWidthAllocator*> m_fixed_width_allocators;
		GeneralAllocator* m_general_allocator;

		MMU* m_mmu { nullptr };
		TTY* m_tty { nullptr };
	};

}