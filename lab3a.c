// NAME: Brian Be
// EMAIL: bebrian458@gmail.com
// UID: 204612203

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "ext2_fs.h"

#define SUPERBLOCK_OFFSET 1024
#define REGFILE 		0x8000
#define DIRECTORY 		0x4000
#define SYMLINK 		0xA000

// Globals
int disk_fd, group_offset;
int total_groups = 0;
int inode_nums;
struct ext2_super_block superblock;
struct ext2_group_desc group_desc;
struct ext2_inode curr_inode;
__u32 block_size = 0;
__u32 inode_size = 0;

void printDirEntry(int parent_inode_num, unsigned int dir_block_offset){

	// Dir entry offset = start of dir entry node
	int dir_entry_offset = 0;
	struct ext2_dir_entry curr_dir_entry;

	while(dir_entry_offset < block_size){
		pread(disk_fd, &curr_dir_entry, sizeof(struct ext2_dir_entry), dir_block_offset+dir_entry_offset);
		if(curr_dir_entry.inode != 0)
			fprintf(stdout, "%s,%d,%d,%d,%d,%d,'%s'\n",
				"DIRENT",
				parent_inode_num,
				dir_entry_offset,
				curr_dir_entry.inode,
				curr_dir_entry.rec_len,
				curr_dir_entry.name_len,
				curr_dir_entry.name);

		// Iterate to the next dir entry node
		dir_entry_offset += curr_dir_entry.rec_len;
	}
}

void printInodeSummary(unsigned int inode_offset, int inode_num){

	pread(disk_fd, &curr_inode, sizeof(struct ext2_inode), inode_offset);

	// Identify file type
	__u16 mode_value = curr_inode.i_mode & 0xFFF;
	char file_type = '?'; 	// default
	if(curr_inode.i_mode & REGFILE)			
		file_type = 'f';
	else if(curr_inode.i_mode & DIRECTORY) 	
		file_type = 'd';
	else if(curr_inode.i_mode & SYMLINK) 	
		file_type = 's';

	// Create ctime string format
	time_t ctime = curr_inode.i_ctime;
	struct tm *create_info;
	create_info = gmtime(&ctime);
	char c_buffer[18];
	strftime(c_buffer, 18, "%m/%d/%g %H:%M:%S", create_info);

	// Crete mtime string format
	time_t mtime = curr_inode.i_mtime;
	struct tm *mod_info;
	mod_info = gmtime(&mtime);
	char m_buffer[18];
	strftime(m_buffer, 18, "%m/%d/%g %H:%M:%S", mod_info);

	// Create atime string format
	time_t atime = curr_inode.i_atime;
	struct tm *access_info;
	access_info = gmtime(&atime);
	char a_buffer[18];
	strftime(a_buffer, 18, "%m/%d/%g %H:%M:%S", access_info);

	// Only print if non-zero mode and non-zero link count
	if(curr_inode.i_mode != 0 && curr_inode.i_links_count != 0)
		fprintf(stdout, "%s,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", 
			"INODE",
			inode_num,
			file_type,
			mode_value,
			curr_inode.i_uid,
			curr_inode.i_gid,
			curr_inode.i_links_count,
			c_buffer,
			m_buffer,
			a_buffer,
			curr_inode.i_size,
			curr_inode.i_blocks,
			curr_inode.i_block[0], curr_inode.i_block[1], curr_inode.i_block[2], curr_inode.i_block[3],
			curr_inode.i_block[4], curr_inode.i_block[5], curr_inode.i_block[6], curr_inode.i_block[7], 
			curr_inode.i_block[8], curr_inode.i_block[9], curr_inode.i_block[10], curr_inode.i_block[11], 
			curr_inode.i_block[12], curr_inode.i_block[13], curr_inode.i_block[14]);

	// Print all 12 direct entries
	if(file_type == 'd'){
		int i;
		for(i = 0; i < 12; i++)
			if(curr_inode.i_block[i] != 0)
				// curr_inode.i_block[i] tells us the block num that holds the linked list of dir entries
				printDirEntry(inode_num, SUPERBLOCK_OFFSET+(curr_inode.i_block[i]-1)*block_size);
	}

	// Print indirect pointer
	unsigned int ind_ptr_offset = SUPERBLOCK_OFFSET+(curr_inode.i_block[12]-1)*block_size;
	unsigned int num_ptrs = block_size/sizeof(__u32);
	unsigned int ind_block_ptrs[num_ptrs];
	pread(disk_fd, ind_block_ptrs, block_size, ind_ptr_offset);

	int curr_ptr;
	for(curr_ptr = 0; curr_ptr < num_ptrs; curr_ptr++){
		if(ind_block_ptrs[curr_ptr] != 0){

			fprintf(stdout, "%s,%d,%d,%d,%d,%d\n",
			"INDIRECT",
			inode_num,
			1,  			// 1 - Singly indirect, 2 - Double, 3 Triple
			12+curr_ptr,	// 12 - curr_inode's i_block
			curr_inode.i_block[12],
			ind_block_ptrs[curr_ptr]);
		}
	}

}

void printBfreeOrIfree(unsigned int bitmap_offset, int isInode, int itable_offset, int max_num_inodes){
	
	// Read data into bitmap
	char block_bytes[block_size];
	pread(disk_fd, block_bytes, block_size, bitmap_offset);
	
	// 0th bit of 0th byte represents block num 1 (superblock) of the first group
	int first_block_num = superblock.s_first_data_block;

	// Iterate over each byte
	int byte_counter;
	for(byte_counter = 0; byte_counter < block_size; byte_counter++){

		// Iterate over each bit
		int bit_counter, curr_block_num;
		int bit_selector = 1;
		char curr_byte = block_bytes[byte_counter];
		for(bit_counter = 0; bit_counter < 8; bit_counter++){
			
			curr_block_num = first_block_num + (byte_counter*8) + bit_counter;

			// If the current bit is 0, it is free
			if(!(curr_byte & bit_selector)){

				if(!isInode)
					fprintf(stdout, "%s,%d\n", "BFREE", curr_block_num);
				else
					fprintf(stdout, "%s,%d\n", "IFREE", curr_block_num);
			}
			// Otherwise, it must be used
			// If it is an inode, print the info
			else if(isInode && curr_block_num < max_num_inodes){

				unsigned int inode_offset = SUPERBLOCK_OFFSET 
				+ (itable_offset*block_size) 
				+ (curr_block_num-1)*sizeof(struct ext2_inode);

				printInodeSummary(inode_offset, curr_block_num);

				// TODO: read into inode_nums
			}
			
			bit_selector <<= 1;
		}
	}
}

void printSuperblock(){

	// Read data into superblock struct
	pread(disk_fd, &superblock, sizeof(superblock), SUPERBLOCK_OFFSET);
	block_size = EXT2_MIN_BLOCK_SIZE << superblock.s_log_block_size;

	fprintf(stdout, "%s,%d,%d,%d,%d,%d,%d,%d\n", 
		"SUPERBLOCK", 
		superblock.s_blocks_count, 
		superblock.s_inodes_count, 
		block_size, 
		superblock.s_inode_size, 
		superblock.s_blocks_per_group, 
		superblock.s_inodes_per_group,
		superblock.s_first_ino);
}

void printGroup(int current_group){

	// Read data into group struct
	pread(disk_fd, &group_desc, sizeof(group_desc), group_offset);

	// Default is a filled group
	int curr_num_blocks = superblock.s_blocks_per_group;
	int curr_num_inodes = superblock.s_inodes_per_group;

	// The last group may not be filled
	if(current_group == total_groups-1){
		curr_num_blocks = superblock.s_blocks_count - (superblock.s_blocks_per_group*(total_groups-1));
		curr_num_inodes = superblock.s_inodes_count - (superblock.s_inodes_per_group*(total_groups-1));
	}

	fprintf(stdout, "%s,%d,%d,%d,%d,%d,%d,%d,%d\n", 
		"GROUP",
		superblock.s_block_group_nr,
		curr_num_blocks,
		curr_num_inodes,
		superblock.s_free_blocks_count,
		superblock.s_free_inodes_count,
		group_desc.bg_block_bitmap,
		group_desc.bg_inode_bitmap,
		group_desc.bg_inode_table);

	// Get the offset for current group's block bitmap and print free block entries
	unsigned int block_bitmap_number = group_desc.bg_block_bitmap;
	unsigned int inode_table_number = group_desc.bg_inode_table;
	unsigned int block_bitmap_offset = SUPERBLOCK_OFFSET+(block_bitmap_number-1)*block_size;
	printBfreeOrIfree(block_bitmap_offset, 0, inode_table_number-1, curr_num_inodes);

	// Get the offset for current group's inode bitmap and print free I-node entries
	unsigned int inode_bitmap_number = group_desc.bg_inode_bitmap;
	unsigned int inode_bitmap_offset = SUPERBLOCK_OFFSET+(inode_bitmap_number-1)*block_size;
	printBfreeOrIfree(inode_bitmap_offset, 1, inode_table_number-1, curr_num_inodes);
}

int main(int argc, char *argv[]){

	if(argc != 2){
		fprintf(stderr, "Invalid arguments. Usage: ./lab3a <filename>\n");
		exit(1);
	}

	// Open file
	char* filename = argv[1];
	disk_fd = open(filename, O_RDONLY);
	if(disk_fd < 0){
		fprintf(stderr, "Could not open file\n");
		exit(2);
	}

	printSuperblock();

	// Calculate total number of groups
	// If the last group is partially filled, it will be truncated => add 1 to account for it
	total_groups = superblock.s_blocks_count / superblock.s_blocks_per_group;
	if(superblock.s_blocks_count % superblock.s_blocks_per_group != 0)
		total_groups += 1;

	// Analyze each group
	int current_group;
	group_offset = SUPERBLOCK_OFFSET + block_size;
	for(current_group = 0; current_group < total_groups; current_group++){
		printGroup(current_group);
		group_offset += block_size;	// change block_size
	}
}