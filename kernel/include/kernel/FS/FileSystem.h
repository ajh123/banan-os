#pragma once

#include <kernel/FS/Inode.h>

namespace Kernel
{

	class FileSystem
	{
	public:
		virtual ~FileSystem() {}
		virtual BAN::RefPtr<Inode> root_inode() = 0;

		virtual dev_t dev() const = 0;
	};

}
