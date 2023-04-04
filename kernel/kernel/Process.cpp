#include <BAN/StringView.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/LockGuard.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>

#include <fcntl.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<Process>> Process::create_kernel(entry_t entry, void* data)
	{
		static pid_t next_pid = 1;
		auto process = TRY(BAN::RefPtr<Process>::create(next_pid++));
		TRY(process->m_working_directory.push_back('/'));
		TRY(process->add_thread(entry, data));
		return process;
	}

	BAN::ErrorOr<void> Process::init_stdio()
	{
		if (!m_open_files.empty())
			return BAN::Error::from_c_string("Could not init stdio, process already has open files");
		TRY(open("/dev/tty1", O_RDONLY)); // stdin
		TRY(open("/dev/tty1", O_WRONLY)); // stdout
		TRY(open("/dev/tty1", O_WRONLY)); // stderr
		return {};
	}

	BAN::ErrorOr<void> Process::add_thread(entry_t entry, void* data)
	{
		Thread* thread = TRY(Thread::create(entry, data, this));

		LockGuard _(m_lock);
		TRY(m_threads.push_back(thread));
		if (auto res = Scheduler::get().add_thread(thread); res.is_error())
		{
			m_threads.pop_back();
			return res;
		}

		return {};
	}

	void Process::on_thread_exit(Thread& thread)
	{
		LockGuard _(m_lock);
		for (size_t i = 0; i < m_threads.size(); i++)
			if (m_threads[i] == &thread)
				m_threads.remove(i);
	}

	BAN::ErrorOr<int> Process::open(BAN::StringView path, int flags)
	{
		if (flags & ~O_RDWR)
			return BAN::Error::from_errno(ENOTSUP);

		BAN::String absolute_path = TRY(absolute_path_of(path));

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(absolute_path));

		LockGuard _(m_lock);
		int fd = TRY(get_free_fd());
		auto& open_file_description = m_open_files[fd];
		open_file_description.inode = file.inode;
		open_file_description.path = BAN::move(file.canonical_path);
		open_file_description.offset = 0;
		open_file_description.flags = flags;

		return fd;
	}

	BAN::ErrorOr<void> Process::close(int fd)
	{
		LockGuard _(m_lock);
		TRY(validate_fd(fd));
		auto& open_file_description = this->open_file_description(fd);
		open_file_description.inode = nullptr;
		return {};
	}

	BAN::ErrorOr<size_t> Process::read(int fd, void* buffer, size_t count)
	{
		m_lock.lock();
		TRY(validate_fd(fd));
		auto open_fd_copy = open_file_description(fd);
		m_lock.unlock();

		if (!(open_fd_copy.flags & O_RDONLY))
			return BAN::Error::from_errno(EBADF);
		size_t n_read = TRY(open_fd_copy.inode->read(open_fd_copy.offset, buffer, count));
		open_fd_copy.offset += n_read;

		m_lock.lock();
		MUST(validate_fd(fd));
		open_file_description(fd) = open_fd_copy;
		m_lock.unlock();

		return n_read;
	}

	BAN::ErrorOr<size_t> Process::write(int fd, const void* buffer, size_t count)
	{
		m_lock.lock();
		TRY(validate_fd(fd));
		auto open_fd_copy = open_file_description(fd);
		m_lock.unlock();

		if (!(open_fd_copy.flags & O_WRONLY))
			return BAN::Error::from_errno(EBADF);
		size_t n_written = TRY(open_fd_copy.inode->write(open_fd_copy.offset, buffer, count));
		open_fd_copy.offset += n_written;

		m_lock.lock();
		MUST(validate_fd(fd));
		open_file_description(fd) = open_fd_copy;
		m_lock.unlock();

		return n_written;
	}

	BAN::ErrorOr<void> Process::creat(BAN::StringView path, mode_t mode)
	{
		auto absolute_path = TRY(absolute_path_of(path));
		while (absolute_path.back() != '/')
			absolute_path.pop_back();
		
		auto parent_file = TRY(VirtualFileSystem::get().file_from_absolute_path(absolute_path));
		if (path.count('/') > 0)
			return BAN::Error::from_c_string("You can only create files to current working directory");
		TRY(parent_file.inode->create_file(path, mode));

		return {};
	}

	BAN::ErrorOr<void> Process::mount(BAN::StringView partition, BAN::StringView path)
	{
		auto absolute_partition = TRY(absolute_path_of(partition));
		auto absolute_path = TRY(absolute_path_of(path));
		TRY(VirtualFileSystem::get().mount(absolute_partition, absolute_path));
		return {};
	}

	BAN::ErrorOr<void> Process::fstat(int fd, struct stat* out)
	{
		m_lock.lock();
		TRY(validate_fd(fd));
		auto open_fd_copy = open_file_description(fd);
		m_lock.unlock();

		out->st_dev		= open_fd_copy.inode->dev();
		out->st_ino		= open_fd_copy.inode->ino();
		out->st_mode	= open_fd_copy.inode->mode().mode;
		out->st_nlink	= open_fd_copy.inode->nlink();
		out->st_uid		= open_fd_copy.inode->uid();
		out->st_gid		= open_fd_copy.inode->gid();
		out->st_rdev	= open_fd_copy.inode->rdev();
		out->st_size	= open_fd_copy.inode->size();
		out->st_atim	= open_fd_copy.inode->atime();
		out->st_mtim	= open_fd_copy.inode->mtime();
		out->st_ctim	= open_fd_copy.inode->ctime();
		out->st_blksize	= open_fd_copy.inode->blksize();
		out->st_blocks	= open_fd_copy.inode->blocks();

		return {};
	}

	BAN::ErrorOr<void> Process::stat(BAN::StringView path, struct stat* out)
	{
		int fd = TRY(open(path, O_RDONLY));
		auto ret = fstat(fd, out);
		MUST(close(fd));
		return ret;
	}

	BAN::ErrorOr<BAN::Vector<BAN::String>> Process::read_directory_entries(int fd)
	{
		m_lock.lock();
		TRY(validate_fd(fd));
		auto open_fd_copy = open_file_description(fd);
		m_lock.unlock();

		auto result = TRY(open_fd_copy.inode->read_directory_entries(open_fd_copy.offset));
		open_fd_copy.offset++;

		m_lock.lock();
		MUST(validate_fd(fd));
		open_file_description(fd) = open_fd_copy;
		m_lock.unlock();

		return result;
	}

	BAN::ErrorOr<BAN::String> Process::working_directory() const
	{
		BAN::String result;

		LockGuard _(m_lock);
		TRY(result.append(m_working_directory));
		
		return result;
	}

	BAN::ErrorOr<void> Process::set_working_directory(BAN::StringView path)
	{
		BAN::String absolute_path = TRY(absolute_path_of(path));

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(absolute_path));
		if (!file.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		LockGuard _(m_lock);
		m_working_directory = BAN::move(file.canonical_path);

		return {};
	}

	BAN::ErrorOr<BAN::String> Process::absolute_path_of(BAN::StringView path) const
	{
		if (path.empty())
			return working_directory();
		BAN::String absolute_path;
		if (path.front() != '/')
		{
			LockGuard _(m_lock);
			TRY(absolute_path.append(m_working_directory));
		}
		if (!absolute_path.empty() && absolute_path.back() != '/')
			TRY(absolute_path.push_back('/'));
		TRY(absolute_path.append(path));
		return absolute_path;
	}

	BAN::ErrorOr<void> Process::validate_fd(int fd)
	{
		ASSERT(m_lock.is_locked());
		if (fd < 0 || m_open_files.size() <= (size_t)fd || !m_open_files[fd].inode)
			return BAN::Error::from_errno(EBADF);
		return {};
	}

	Process::OpenFileDescription& Process::open_file_description(int fd)
	{
		ASSERT(m_lock.is_locked());
		MUST(validate_fd(fd));
		return m_open_files[fd];
	}

	BAN::ErrorOr<int> Process::get_free_fd()
	{
		ASSERT(m_lock.is_locked());
		for (size_t fd = 0; fd < m_open_files.size(); fd++)
			if (!m_open_files[fd].inode)
				return fd;
		TRY(m_open_files.push_back({}));
		return m_open_files.size() - 1;
	}

}