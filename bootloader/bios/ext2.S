# FIXME: don't assume 512 byte sectors
.set SECTOR_SHIFT,	9
.set SECTOR_SIZE,	1 << SECTOR_SHIFT

.set EXT2_MAX_BLOCK_SIZE,	4096
.set EXT2_SUPERBLOCK_SIZE,	264
.set EXT2_BGD_SHIFT,		5
.set EXT2_BGD_SIZE,			1 << EXT2_BGD_SHIFT
.set EXT2_INODE_SIZE_MAX,	256
.set EXT2_ROOT_INO,			2
.set EXT2_GOOD_OLD_REV,		0

# inode types
.set EXT2_S_IMASK,			0xF000
.set EXT2_S_IFDIR,			0x4000
.set EXT2_S_IFREG,			0x8000

# superblock offsets
.set s_first_data_block,	20
.set s_log_block_size,		24
.set s_inodes_per_group,	40
.set s_magic,				56
.set s_rev_level,			76
.set s_inode_size,			88

# block group descriptor offsets
.set bg_inode_table,		8

# inode offsets
.set i_mode,				0
.set i_size,				4
.set i_block,				40


.code16
.section .stage2

# checks whether partition contains ext2 filesystem.
# fills ext2_superblock_buffer
#	dl:		drive number
#	ecx:	sector count
#	bx:eax:	first sector
# return:
#	al: 1 if is ext2, 0 otherwise
#	si: error message on error
.global has_ext2_filesystem
has_ext2_filesystem:
	pushl %ecx
	pushw %bx
	pushw %di

	# fill ext2_partition_first_sector
	movw $0,   (ext2_partition_first_sector + 6)
	movw %bx,  (ext2_partition_first_sector + 4)
	movl %eax, (ext2_partition_first_sector + 0)

	# fill ext2_drive_number
	movb %dl, (ext2_drive_number)

	cmpl $3, %ecx
	jb .has_ext2_filesystem_does_not_fit

	# one sector
	movw $1, %cx

	# from byte offset 1024
	addl $(1024 / SECTOR_SIZE), %eax
	adcw $0, %bx

	# into sector buffer
	movw $ext2_block_buffer, %di

	call read_from_disk

	# copy superblock to its buffer
	movw $ext2_block_buffer, %si
	movw $ext2_superblock_buffer, %di
	movw $EXT2_SUPERBLOCK_SIZE, %cx
	rep movsb

	# verify magic
	cmpw $0xEF53, (ext2_superblock_buffer + s_magic)
	jne .has_ext2_filesystem_invalid_magic

	# verify block size
	  # verify shift fits in one byte
	movl (ext2_superblock_buffer + s_log_block_size), %ecx
	testl $0xFFFFFF00, %ecx
	jnz .has_ext2_filesystem_unsupported_block_size
	  # verify 1024 << s_log_block_size <= EXT2_MAX_BLOCK_SIZE
	movl $1024, %eax
	shll %cl, %eax
	cmpl $EXT2_MAX_BLOCK_SIZE, %eax
	ja .has_ext2_filesystem_unsupported_block_size

	# fill block size and shift
	movl %eax, (ext2_block_size)
	addl $10, %ecx
	movl %ecx, (ext2_block_shift)

	# fill inode size
	movl $128, %eax
	cmpl $EXT2_GOOD_OLD_REV, (ext2_superblock_buffer + s_rev_level)
	cmovnel (ext2_superblock_buffer + s_inode_size), %eax
	movl %eax, (ext2_inode_size)

	movb $1, %al
	jmp .has_ext2_filesystem_done

 .has_ext2_filesystem_does_not_fit:
	movw $root_partition_does_not_fit_ext2_filesystem_msg, %si
	movb $0, %al
	jmp .has_ext2_filesystem_done

 .has_ext2_filesystem_invalid_magic:
	movw $root_partition_has_invalid_ext2_magic_msg, %si
	movb $0, %al
	jmp .has_ext2_filesystem_done

 .has_ext2_filesystem_unsupported_block_size:
	movw $root_partition_has_unsupported_ext2_block_size_msg, %si
	movb $0, %al
	jmp .has_ext2_filesystem_done

 .has_ext2_filesystem_done:
	popw %di
	popw %bx
	popl %ecx
	ret


# reads block in to ext2_block_buffer
#	eax:	block number
ext2_read_block:
	pushal

	# ecx := sectors_per_block := block_size / sector_size
	movl (ext2_block_size), %ecx
	shrl $SECTOR_SHIFT, %ecx

	# ebx:eax := block * sectors_per_block + (ext2_partition_first_sector)
	xorl %ebx, %ebx
	mull %ecx
	addl (ext2_partition_first_sector + 0), %eax
	adcl (ext2_partition_first_sector + 4), %ebx

	movw $ext2_block_buffer, %di
	movb (ext2_drive_number), %dl
	call read_from_disk

	popal
	ret


# reads block group descrtiptor into ext2_block_group_descriptor
#	eax:	block group
ext2_read_block_group_descriptor:
	pushal

	# ebx := bgd_block_byte_offset := (s_first_data_block + 1) * block_size
	#                              := (s_first_data_block + 1) << ext2_block_shift
	movl (ext2_superblock_buffer + s_first_data_block), %ebx
	incl %ebx
	movb (ext2_block_shift), %cl
	shll %cl, %ebx

	# eax := bgd_byte_offset := bgd_block_byte_offset + EXT2_BGD_SIZE * block_group;
	#                        := bgd_block_byte_offset + (block_group << EXT2_BGD_SHIFT)
	movb $EXT2_BGD_SHIFT, %cl
	shll %cl, %eax
	addl %ebx, %eax

	# eax: bgd_block  := bgd_byte_offset / block_size
	# ebx: bgd_offset := bgd_byte_offset % block_size
	xorl %edx, %edx
	divl (ext2_block_size)
	movl %edx, %ebx

	call ext2_read_block

	# esi := &ext2_block_buffer + bgd_offset := ebx + &ext2_block_buffer
	# edi := &ext2_block_group_descriptor_buffer
	movl %ebx, %esi
	addl $ext2_block_buffer, %esi
	movl $ext2_block_group_descriptor_buffer, %edi
	movw $EXT2_BGD_SIZE, %cx
	rep movsb

	popal
	ret


# reads inode into ext2_inode_buffer
#	eax:	ino
ext2_read_inode:
	pushal

	# eax := block_group = (ino - 1) / s_inodes_per_group
	# ebx := inode_index = (ino - 1) % s_inodes_per_group
	xorl %edx, %edx
	decl %eax
	divl (ext2_superblock_buffer + s_inodes_per_group)
	movl %edx, %ebx

	call ext2_read_block_group_descriptor

	# eax := inode_table_block  := (inode_index * inode_size) / block_size
	# ebx := inode_table_offset := (inode_index * inode_size) % block_size
	movl %ebx, %eax
	mull (ext2_inode_size)
	divl (ext2_block_size)
	movl %edx, %ebx

	# eax := filesystem_block := eax + bg_inode_table
	addl (ext2_block_group_descriptor_buffer + bg_inode_table), %eax

	movb (ext2_drive_number), %dl
	call ext2_read_block

	# copy inode memory
	  # esi := inode_table_offset + ext2_block_buffer := edx + ext2_block_buffer
	movl %ebx, %esi
	addl $ext2_block_buffer, %esi
	  # edi := ext2_inode_buffer
	movl $ext2_inode_buffer, %edi
	  # ecx := inode_size
	movl (ext2_inode_size), %ecx
	rep movsb

	# reset indirect cache to zero
	movl $0, (ext2_inode_indirect_number)

 .ext2_read_inode_done:
	popal
	ret


# gets block index from n'th data block in inode stored in ext2_inode_buffer
#	eax:	data block index
# return:
#	eax:	block index
ext2_data_block_index:
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %esi
	pushl %edi

	# ebx := max_data_blocks := (file_size + block_size - 1) / block_size
	#                        := (i_size + ext2_block_size - 1) >> ext2_block_shift
	# cl  := ext2_block_shift
	movl (ext2_inode_buffer + i_size), %ebx
	addl (ext2_block_size), %ebx
	decl %ebx
	movb (ext2_block_shift), %cl
	shrl %cl, %ebx

	# verify data block is within bounds
	cmpl %ebx, %eax
	jae .ext2_data_block_index_out_of_bounds

	# check if this is direct block access
	cmpl $12, %eax
	jb .ext2_data_block_index_direct
	subl $12, %eax

	# cl  := indices_per_block_shift := ext2_block_shift - 2
	# ebx := comp
	subb $2, %cl
	movl $1, %ebx
	shll %cl, %ebx

	# check if this is singly indirect block access
	cmpl %ebx, %eax
	jb .ext2_data_block_index_singly_indirect
	subl %ebx, %eax
	shll %cl, %ebx

	# check if this is doubly indirect block access
	cmpl %ebx, %eax
	jb .ext2_data_block_index_doubly_indirect
	subl %ebx, %eax
	shll %cl, %ebx

	# check if this is triply indirect block access
	cmpl %ebx, %eax
	jb .ext2_data_block_index_triply_indirect

	# otherwise this is invalid access
	jmp .ext2_data_block_index_invalid

 .ext2_data_block_index_direct:
	movl $(ext2_inode_buffer + i_block), %esi
	movl (%esi, %eax, 4), %eax
	jmp .ext2_data_block_index_done

 .ext2_data_block_index_singly_indirect:
	movl %eax, %ebx
	movl (ext2_inode_buffer + i_block + 12 * 4), %eax
	movw $1, %cx
	jmp .ext2_data_block_index_indirect

 .ext2_data_block_index_doubly_indirect:
	movl %eax, %ebx
	movl (ext2_inode_buffer + i_block + 13 * 4), %eax
	movw $2, %cx
	jmp .ext2_data_block_index_indirect

 .ext2_data_block_index_triply_indirect:
	movl %eax, %ebx
	movl (ext2_inode_buffer + i_block + 14 * 4), %eax
	movw $3, %cx
	jmp .ext2_data_block_index_indirect

	# eax := current block
	# ebx := index
	# cx  := depth
 .ext2_data_block_index_indirect:
	# edx := cache index := (index & ~(block_size / 4 - 1)) | depth
	#                    := (index & -(block_size >> 2)) | depth
	movl (ext2_block_size), %edx
	shrl $2, %edx
	negl %edx
	andl %ebx, %edx
	orw %cx, %dx

	# check whether this block is already cached
	cmpl $0, (ext2_inode_indirect_number)
	je .ext2_data_block_index_indirect_no_cache
	cmpl %edx, (ext2_inode_indirect_number)
	je .ext2_data_block_index_indirect_cached

 .ext2_data_block_index_indirect_no_cache:
	# update cache block number, will be cached when found
	movl %edx, (ext2_inode_indirect_number)

	# eax := current block
	# ebx := index
	# cx  := depth
 .ext2_data_block_index_indirect_loop:
	call ext2_read_block

	# store depth and index
	pushw %cx
	pushl %ebx

	cmpw $1, %cx
	jbe .ext2_data_block_index_no_shift

	# cl := shift
	movb (ext2_block_shift), %al
	subb $2, %al
	decb %cl
	mulb %cl
	movb %al, %cl

	# ebx := ebx >> shift
	shrl %cl, %ebx

 .ext2_data_block_index_no_shift:
	# edx := index of next block (ebx & (block_size / 4 - 1))
	movl (ext2_block_size), %edx
	shrl $2, %edx
	decl %edx
	andl %ebx, %edx

	# eax := next block
	movl $ext2_block_buffer, %esi
	movl (%esi, %edx, 4), %eax

	# restore depth and index
	popl %ebx
	popw %cx

	loop .ext2_data_block_index_indirect_loop

	# cache last read block
	movw $ext2_block_buffer, %si
	movw $ext2_inode_indirect_buffer, %di
	movw (ext2_block_size), %cx
	rep movsb

	jmp .ext2_data_block_index_done

 .ext2_data_block_index_out_of_bounds:
	movw $ext2_data_block_index_out_of_bounds_msg, %si
	call puts; call print_newline
	movl $0, %eax
	jmp .ext2_data_block_index_done

 .ext2_data_block_index_invalid:
	movw $ext2_data_block_index_invalid_msg, %si
	call puts; call print_newline
	movl $0, %eax
	jmp .ext2_data_block_index_done

 .ext2_data_block_index_indirect_cached:
	movl $ext2_inode_indirect_buffer, %esi
	movl (ext2_block_size), %edx
	shrl $2, %edx
	decl %edx
	andl %edx, %ebx
	movl (%esi, %ebx, 4), %eax

 .ext2_data_block_index_done:
	popl %edi
	popl %esi
	popl %edx
	popl %ecx
	popl %ebx
	ret


# read bytes from inode (implements read callback)
#	eax: first byte
#	ecx: byte count
#	edi: buffer
# returns only on success
.global ext2_inode_read_bytes
ext2_inode_read_bytes:
	pushal

	pushl %ebp
	movl %esp, %ebp
	subl $8, %esp

	# save read info
	movl %eax, 0(%esp)
	movl %ecx, 4(%esp)

	# eax := first_byte / block_size
	# edx := first_byte % block_size
	# when edx == 0, no partial read needed
	xorl %edx, %edx
	divl (ext2_block_size)
	testl %edx, %edx
	jz .ext2_inode_read_bytes_no_partial_start

	# get data block index and read block
	call ext2_data_block_index
	call ext2_read_block

	# ecx := byte count (min(block_size - edx, remaining_bytes))
	movl (ext2_block_size), %ecx
	subl %edx, %ecx
	cmpl %ecx, 4(%esp)
	cmovbl 4(%esp), %ecx

	# update remaining read info
	addl %ecx, 0(%esp)
	subl %ecx, 4(%esp)

	# esi := start sector data (block_buffer + index * SECTOR_SIZE)
	movl $ext2_block_buffer, %esi
	addl %edx, %esi

	call memcpy32

	# check if all sectors are read
	cmpl $0, 4(%esp)
	je .ext2_inode_read_bytes_done

 .ext2_inode_read_bytes_no_partial_start:
	# eax := data block index (byte_start / block_size)
	movl 0(%esp), %eax
	movb (ext2_block_shift), %cl
	shrl %cl, %eax

	# get data block index and read block
	call ext2_data_block_index
	call ext2_read_block

	# calculate bytes to copy (min(block_size, remaining_bytes))
	movl (ext2_block_size), %ecx
	cmpl %ecx, 4(%esp)
	cmovbl 4(%esp), %ecx

	# update remaining read info
	addl %ecx, 0(%esp)
	subl %ecx, 4(%esp)

	movl $ext2_block_buffer, %esi
	call memcpy32

	# read next block if more sectors remaining
	cmpl $0, 4(%esp)
	jnz .ext2_inode_read_bytes_no_partial_start

 .ext2_inode_read_bytes_done:
	leavel
	popal
	ret


# find inode in inside directory inode stored in ext2_inode_buffer
# store the found inode in ext2_inode_buffer
#	si:		name string
#	cx:		name length
# return:
#	eax:	ino if inode was found, 0 otherwise
ext2_directory_find_inode:
	pushl %ebx
	pushw %cx
	pushw %dx
	pushw %si
	pushw %di

	pushl %ebp
	movl %esp, %ebp
	subl $8, %esp

	# 0(%esp) := name length
	movw %cx, 0(%esp)

	# 2(%esp) := name string
	movw %si, 2(%esp)

	# verify that the name is <= 0xFF bytes
	cmpw $0xFF, %cx
	ja .ext2_directory_find_inode_not_found

	# ebx := max data blocks: ceil(i_size / block_size)
	movl (ext2_inode_buffer + i_size), %ebx
	addl (ext2_block_size), %ebx
	decl %ebx
	movb (ext2_block_shift), %cl
	shrl %cl, %ebx
	jz .ext2_directory_find_inode_not_found

	# 4(%esp) := current block
	movl $0, 4(%esp)

 .ext2_directory_find_inode_block_read_loop:
	# get next block index
	movl 4(%esp), %eax
	call ext2_data_block_index
	test %eax, %eax
	jz .ext2_directory_find_inode_next_block

	# read current block
	call ext2_read_block

	# dx := current entry pointer
	movw $ext2_block_buffer, %si

 .ext2_directory_find_inode_loop_entries:
	# temporarily store entry pointer in dx
	movw %si, %dx

	# check if name length matches
	# cx := name length
	movw 0(%esp), %cx
	cmpb 6(%si), %cl
	jne .ext2_directory_find_inode_next_entry

	# si := entry name
	addw $8, %si

	# di := asked name
	movw 2(%esp), %di

	# check if name matches
	call memcmp
	test %al, %al
	# NOTE: dx contains entry pointer
	jnz .ext2_directory_find_inode_found

 .ext2_directory_find_inode_next_entry:
	# restore si
	movw %dx, %si

	# go to next entry if this block contains one
	addw 4(%si), %si
	movw $ext2_block_buffer, %di
	addw (ext2_block_size), %di
	cmpw %di, %si
	jb .ext2_directory_find_inode_loop_entries

 .ext2_directory_find_inode_next_block:
	incl 4(%esp)
	cmpl %ebx, 4(%esp)
	jb .ext2_directory_find_inode_block_read_loop

 .ext2_directory_find_inode_not_found:
	xorb %al, %al
	jmp .ext2_directory_find_inode_done

 .ext2_directory_find_inode_found:
	# extract ino and read it to ext2_inode_buffer
	movw %dx, %si
	movl 0(%si), %eax
	call ext2_read_inode

 .ext2_directory_find_inode_done:
	leavel
	popw %di
	popw %si
	popw %dx
	popw %cx
	popl %ebx
	ret


# search for kernel file from filesystem
# returns only on success
.global ext2_find_kernel
ext2_find_kernel:
	pushl %eax
	pushw %cx
	pushw %di
	pushw %si

	movl $EXT2_ROOT_INO, %eax
	call ext2_read_inode

	movw $kernel_path, %di
 .ext2_find_kernel_loop:
	movw (%di), %si

	# check if this list is done
	testw %si, %si
	jz .ext2_find_kernel_loop_done

	# check that current part is directory
	movw (ext2_inode_buffer + i_mode), %ax
	andw $EXT2_S_IMASK, %ax
	cmpw $EXT2_S_IFDIR, %ax
	jne .ext2_find_kernel_part_not_dir

	# prepare registers for directory finding
	movw 0(%si), %cx
	addw $2, %si

	# print search path
	pushw %si
	movw $ext2_looking_for_msg, %si
	call puts
	popw %si
	call puts; call print_newline

	# search current directory for this file
	call ext2_directory_find_inode
	testl %eax, %eax
	jz .ext2_find_kernel_part_not_found

	# loop to next part
	addw $2, %di
	jmp .ext2_find_kernel_loop

 .ext2_find_kernel_loop_done:

	# check that kernel is a regular file
	movw (ext2_inode_buffer + i_mode), %ax
	andw $EXT2_S_IMASK, %ax
	cmpw $EXT2_S_IFREG, %ax
	jne .ext2_find_kernel_not_reg

	movw $ext2_kernel_found_msg, %si
	call puts; call print_newline

	popw %si
	popw %di
	popw %cx
	popl %eax
	ret

 .ext2_find_kernel_part_not_dir:
	movw $ext2_part_not_dir_msg, %si
	jmp print_and_halt

 .ext2_find_kernel_part_not_found:
	movw $ext2_part_not_found_msg, %si
	jmp print_and_halt

 .ext2_find_kernel_not_reg:
	movw $ext2_kernel_not_reg_msg, %si
	jmp print_and_halt

.section .data

kernel_path:
	.short kernel_path1
	.short kernel_path2
	.short 0
kernel_path1:
	.short 4
	.asciz "boot"
kernel_path2:
	.short 15
	.asciz "banan-os.kernel"

root_partition_does_not_fit_ext2_filesystem_msg:
	.asciz "Root partition is too small to contain ext2 filesystem"
root_partition_has_invalid_ext2_magic_msg:
	.asciz "Root partition doesn't contain ext2 magic number"
root_partition_has_unsupported_ext2_block_size_msg:
	.asciz "Root partition has unsupported ext2 block size (1 KiB, 2 KiB and 4 KiB are supported)"

ext2_part_not_dir_msg:
	.asciz "inode in root path is not directory"
ext2_part_not_found_msg:
	.asciz " not found"
ext2_kernel_not_reg_msg:
	.asciz "kernel is not a regular file"
ext2_kernel_found_msg:
	.asciz "kernel found!"

ext2_data_block_index_out_of_bounds_msg:
	.asciz "data block index out of bounds"
ext2_data_block_index_invalid_msg:
	.asciz "data block index is invalid"

ext2_looking_for_msg:
	.asciz "looking for "

.section .bss

.align SECTOR_SIZE
ext2_block_buffer:
	.skip EXT2_MAX_BLOCK_SIZE

ext2_inode_indirect_buffer:
	.skip EXT2_MAX_BLOCK_SIZE
ext2_inode_indirect_number:
	.skip 4

ext2_partition_first_sector:
	.skip 8

ext2_drive_number:
	.skip 1
	.skip 3 # padding

# NOTE: fits in 2 bytes
ext2_inode_size:
	.skip 4
ext2_block_size:
	.skip 4
ext2_block_shift:
	.skip 4

ext2_superblock_buffer:
	.skip EXT2_SUPERBLOCK_SIZE

ext2_block_group_descriptor_buffer:
	.skip EXT2_BGD_SIZE

ext2_inode_buffer:
	.skip EXT2_INODE_SIZE_MAX
