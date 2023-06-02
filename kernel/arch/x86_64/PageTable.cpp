#include <BAN/Errors.h>
#include <kernel/Arch.h>
#include <kernel/LockGuard.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/PageTable.h>

#define CLEANUP_STRUCTURE(s)				\
	do {									\
		for (uint64_t i = 0; i < 512; i++)	\
			if ((s)[i] & Flags::Present)	\
				return;						\
		kfree(s);							\
	} while (false)

extern uint8_t g_kernel_start[];
extern uint8_t g_kernel_end[];

namespace Kernel
{
	
	static PageTable* s_kernel = nullptr;
	static PageTable* s_current = nullptr;

	static constexpr inline bool is_canonical(uintptr_t addr)
	{
		constexpr uintptr_t mask = 0xFFFF800000000000;
		addr &= mask;
		return addr == mask || addr == 0;
	}

	static constexpr inline uintptr_t uncanonicalize(uintptr_t addr)
	{
		if (addr & 0x0000800000000000)
			return addr & ~0xFFFF000000000000;
		return addr;
	}

	static constexpr inline uintptr_t canonicalize(uintptr_t addr)
	{
		if (addr & 0x0000800000000000)
			return addr | 0xFFFF000000000000;
		return addr;
	}

	void PageTable::initialize()
	{
		ASSERT(s_kernel == nullptr);
		s_kernel = new PageTable();
		ASSERT(s_kernel);
		s_kernel->initialize_kernel();
		s_kernel->load();
	}

	PageTable& PageTable::kernel()
	{
		ASSERT(s_kernel);
		return *s_kernel;
	}

	PageTable& PageTable::current()
	{
		ASSERT(s_current);
		return *s_current;
	}

	static uint64_t* allocate_page_aligned_page()
	{
		void* page = kmalloc(PAGE_SIZE, PAGE_SIZE);
		ASSERT(page);
		memset(page, 0, PAGE_SIZE);
		return (uint64_t*)page;
	}

	void PageTable::initialize_kernel()
	{
		// Map (0 -> phys_kernel_end) to (KERNEL_OFFSET -> virt_kernel_end)
		m_highest_paging_struct = V2P(allocate_page_aligned_page());
		map_range_at(0, KERNEL_OFFSET, (uintptr_t)g_kernel_end - KERNEL_OFFSET, Flags::ReadWrite | Flags::Present);
	}

	BAN::ErrorOr<PageTable*> PageTable::create_userspace()
	{
		// Here we copy the s_kernel paging structs since they are
		// global for every process

		LockGuard _(s_kernel->m_lock);

		uint64_t* global_pml4 = (uint64_t*)P2V(s_kernel->m_highest_paging_struct);

		uint64_t* pml4 = allocate_page_aligned_page();
		for (uint32_t pml4e = 0; pml4e < 512; pml4e++)
		{
			if (!(global_pml4[pml4e] & Flags::Present))
				continue;

			uint64_t* global_pdpt = (uint64_t*)P2V(global_pml4[pml4e] & PAGE_ADDR_MASK);

			uint64_t* pdpt = allocate_page_aligned_page();
			pml4[pml4e] = V2P(pdpt) | (global_pml4[pml4e] & PAGE_FLAG_MASK);

			for (uint32_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(global_pdpt[pdpte] & Flags::Present))
					continue;

				uint64_t* global_pd = (uint64_t*)P2V(global_pdpt[pdpte] & PAGE_ADDR_MASK);

				uint64_t* pd = allocate_page_aligned_page();
				pdpt[pdpte] = V2P(pd) | (global_pdpt[pdpte] & PAGE_FLAG_MASK);

				for (uint32_t pde = 0; pde < 512; pde++)
				{
					if (!(global_pd[pde] & Flags::Present))
						continue;

					uint64_t* global_pt = (uint64_t*)P2V(global_pd[pde] & PAGE_ADDR_MASK);

					uint64_t* pt = allocate_page_aligned_page();
					pd[pde] = V2P(pt) | (global_pd[pde] & PAGE_FLAG_MASK);

					memcpy(pt, global_pt, PAGE_SIZE);
				}
			}
		}

		PageTable* result = new PageTable;
		if (result == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		result->m_highest_paging_struct = V2P(pml4);
		return result;
	}

	PageTable::~PageTable()
	{
		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		for (uint32_t pml4e = 0; pml4e < 512; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present))
				continue;
			uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
			for (uint32_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(pdpt[pdpte] & Flags::Present))
					continue;
				uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
				for (uint32_t pde = 0; pde < 512; pde++)
				{
					if (!(pd[pde] & Flags::Present))
						continue;
					kfree((void*)P2V(pd[pde] & PAGE_ADDR_MASK));
				}
				kfree(pd);
			}
			kfree(pdpt);
		}
		kfree(pml4);
	}

	void PageTable::load()
	{
		asm volatile("movq %0, %%cr3" :: "r"(m_highest_paging_struct));
		s_current = this;
	}

	void PageTable::invalidate(vaddr_t vaddr)
	{
		ASSERT(this == s_current);
		asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
	}

	void PageTable::identity_map_page(paddr_t address, flags_t flags)
	{
		address &= PAGE_ADDR_MASK;
		map_page_at(address, address, flags);
	}

	void PageTable::identity_map_range(paddr_t address, size_t size, flags_t flags)
	{
		LockGuard _(m_lock);

		paddr_t s_page = address / PAGE_SIZE;
		paddr_t e_page = (address + size - 1) / PAGE_SIZE;
		for (paddr_t page = s_page; page <= e_page; page++)
			identity_map_page(page * PAGE_SIZE, flags);
	}

	void PageTable::unmap_page(vaddr_t vaddr)
	{
		LockGuard _(m_lock);

		ASSERT(is_canonical(vaddr));
		vaddr = uncanonicalize(vaddr);

		vaddr &= PAGE_ADDR_MASK;

		if (is_page_free(vaddr))
		{
			dwarnln("unmapping unmapped page {8H}", vaddr);
			return;
		}

		uint64_t pml4e = (vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		uint64_t pde   = (vaddr >> 21) & 0x1FF;
		uint64_t pte   = (vaddr >> 12) & 0x1FF;
		
		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		uint64_t* pd   = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		uint64_t* pt   = (uint64_t*)P2V(pd[pde]     & PAGE_ADDR_MASK);

		pt[pte] = 0;
		CLEANUP_STRUCTURE(pt);
		pd[pde] = 0;
		CLEANUP_STRUCTURE(pd);
		pdpt[pdpte] = 0;
		CLEANUP_STRUCTURE(pdpt);
		pml4[pml4e] = 0;
	}

	void PageTable::unmap_range(vaddr_t vaddr, size_t size)
	{
		LockGuard _(m_lock);

		vaddr_t s_page = vaddr / PAGE_SIZE;
		vaddr_t e_page = (vaddr + size - 1) / PAGE_SIZE;
		for (vaddr_t page = s_page; page <= e_page; page++)
			unmap_page(page * PAGE_SIZE);
	}

	void PageTable::map_page_at(paddr_t paddr, vaddr_t vaddr, flags_t flags)
	{
		LockGuard _(m_lock);

		ASSERT(is_canonical(vaddr));
		vaddr = uncanonicalize(vaddr);

		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);;

		ASSERT(flags & Flags::Present);

		uint64_t pml4e = (vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		uint64_t pde   = (vaddr >> 21) & 0x1FF;
		uint64_t pte   = (vaddr >> 12) & 0x1FF;

		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		if ((pml4[pml4e] & flags) != flags)
		{
			if (!(pml4[pml4e] & Flags::Present))
				pml4[pml4e] = V2P(allocate_page_aligned_page());
			pml4[pml4e] = (pml4[pml4e] & PAGE_ADDR_MASK) | flags;
		}

		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		if ((pdpt[pdpte] & flags) != flags)
		{
			if (!(pdpt[pdpte] & Flags::Present))
				pdpt[pdpte] = V2P(allocate_page_aligned_page());
			pdpt[pdpte] = (pdpt[pdpte] & PAGE_ADDR_MASK) | flags;
		}

		uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		if ((pd[pde] & flags) != flags)
		{
			if (!(pd[pde] & Flags::Present))
				pd[pde] = V2P(allocate_page_aligned_page());
			pd[pde] = (pd[pde] & PAGE_ADDR_MASK) | flags;
		}

		uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
		pt[pte] = paddr | flags;
	}

	void PageTable::map_range_at(paddr_t paddr, vaddr_t vaddr, size_t size, flags_t flags)
	{
		LockGuard _(m_lock);

		ASSERT(is_canonical(vaddr));

		ASSERT(paddr % PAGE_SIZE == 0);
		ASSERT(vaddr % PAGE_SIZE == 0);

		size_t first_page = vaddr / PAGE_SIZE;
		size_t last_page = (vaddr + size - 1) / PAGE_SIZE;
		size_t page_count = last_page - first_page + 1;
		for (size_t page = 0; page < page_count; page++)
			map_page_at(paddr + page * PAGE_SIZE, vaddr + page * PAGE_SIZE, flags);
	}

	uint64_t PageTable::get_page_data(vaddr_t vaddr) const
	{
		LockGuard _(m_lock);

		ASSERT(is_canonical(vaddr));
		vaddr = uncanonicalize(vaddr);

		ASSERT(vaddr % PAGE_SIZE == 0);

		uint64_t pml4e = (vaddr >> 39) & 0x1FF;
		uint64_t pdpte = (vaddr >> 30) & 0x1FF;
		uint64_t pde   = (vaddr >> 21) & 0x1FF;
		uint64_t pte   = (vaddr >> 12) & 0x1FF;
		
		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		if (!(pml4[pml4e] & Flags::Present))
			return 0;

		uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
		if (!(pdpt[pdpte] & Flags::Present))
			return 0;

		uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
		if (!(pd[pde] & Flags::Present))
			return 0;

		uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
		if (!(pt[pte] & Flags::Present))
			return 0;

		return pt[pte];
	}

	PageTable::flags_t PageTable::get_page_flags(vaddr_t addr) const
	{
		return get_page_data(addr) & PAGE_FLAG_MASK;
	}

	paddr_t PageTable::physical_address_of(vaddr_t addr) const
	{
		return get_page_data(addr) & PAGE_ADDR_MASK;
	}

	vaddr_t PageTable::get_free_page() const
	{
		LockGuard _(m_lock);

		// Try to find free page that can be mapped without
		// allocations (page table with unused entries)
		uint64_t* pml4 = (uint64_t*)P2V(m_highest_paging_struct);
		for (uint64_t pml4e = 0; pml4e < 512; pml4e++)
		{
			if (!(pml4[pml4e] & Flags::Present))
				continue;
			uint64_t* pdpt = (uint64_t*)P2V(pml4[pml4e] & PAGE_ADDR_MASK);
			for (uint64_t pdpte = 0; pdpte < 512; pdpte++)
			{
				if (!(pdpt[pdpte] & Flags::Present))
					continue;
				uint64_t* pd = (uint64_t*)P2V(pdpt[pdpte] & PAGE_ADDR_MASK);
				for (uint64_t pde = 0; pde < 512; pde++)
				{
					if (!(pd[pde] & Flags::Present))
						continue;
					uint64_t* pt = (uint64_t*)P2V(pd[pde] & PAGE_ADDR_MASK);
					for (uint64_t pte = !(pml4e + pdpte + pde); pte < 512; pte++)
					{
						if (!(pt[pte] & Flags::Present))
						{
							vaddr_t vaddr = 0;
							vaddr |= pml4e << 39;
							vaddr |= pdpte << 30;
							vaddr |= pde   << 21;
							vaddr |= pte   << 12;
							return canonicalize(vaddr);
						}
					}
				}
			}
		}

		// Find any free page page (except for page 0)
		vaddr_t vaddr = PAGE_SIZE;
		while ((vaddr >> 48) == 0)
		{
			if (!(get_page_flags(vaddr) & Flags::Present))
				return vaddr;
			vaddr += PAGE_SIZE;
		}

		ASSERT_NOT_REACHED();
	}

	vaddr_t PageTable::get_free_contiguous_pages(size_t page_count) const
	{
		LockGuard _(m_lock);

		for (vaddr_t vaddr = PAGE_SIZE; !(vaddr >> 48); vaddr += PAGE_SIZE)
		{
			bool valid { true };
			for (size_t page = 0; page < page_count; page++)
			{
				if (get_page_flags(vaddr + page * PAGE_SIZE) & Flags::Present)
				{
					vaddr += page * PAGE_SIZE;
					valid = false;
					break;
				}
			}
			if (valid)
				return vaddr;
		}

		ASSERT_NOT_REACHED();
	}

	bool PageTable::is_page_free(vaddr_t page) const
	{
		ASSERT(page % PAGE_SIZE == 0);
		return !(get_page_flags(page) & Flags::Present);
	}

	bool PageTable::is_range_free(vaddr_t start, size_t size) const
	{
		LockGuard _(m_lock);

		vaddr_t first_page = start / PAGE_SIZE;
		vaddr_t last_page = (start + size - 1) / PAGE_SIZE;
		for (vaddr_t page = first_page; page <= last_page; page++)
			if (!is_page_free(page * PAGE_SIZE))
				return false;
		return true;
	}

}