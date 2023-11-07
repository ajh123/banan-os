#pragma once

#include <BAN/Array.h>
#include <kernel/Memory/Types.h>

#include <sys/types.h>
#include <time.h>

namespace Kernel
{

	struct TmpInodeInfo
	{
		mode_t		mode	{ 0 };
		uid_t		uid		{ 0 };
		gid_t		gid		{ 0 };
		timespec	atime	{ 0 };
		timespec	ctime	{ 0 };
		timespec	mtime	{ 0 };
		nlink_t		nlink	{ 0 };
		size_t		size	{ 0 };
		blkcnt_t	blocks	{ 0 };

		// 2x direct blocks
		// 1x singly indirect
		// 1x doubly indirect
		// 1x triply indirect
		BAN::Array<paddr_t, 5> block;
		static constexpr size_t direct_block_count = 2;
		static constexpr size_t max_size =
			direct_block_count * PAGE_SIZE +
			(PAGE_SIZE / sizeof(paddr_t)) * PAGE_SIZE +
			(PAGE_SIZE / sizeof(paddr_t)) * (PAGE_SIZE / sizeof(paddr_t)) * PAGE_SIZE +
			(PAGE_SIZE / sizeof(paddr_t)) * (PAGE_SIZE / sizeof(paddr_t)) * (PAGE_SIZE / sizeof(paddr_t)) * PAGE_SIZE;
	};
	static_assert(sizeof(TmpInodeInfo) == 128);

	struct TmpDirectoryEntry
	{
		ino_t	ino;
		uint8_t	type;
		size_t	name_len;
		size_t	rec_len;
		char	name[];

		BAN::StringView name_sv() const
		{
			ASSERT(type != DT_UNKNOWN);
			return BAN::StringView(name, name_len);
		}
	};

}