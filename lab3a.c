// NAME: Brian Be
// EMAIL: bebrian458@gmail.com
// UID: 204612203

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "ext2_fs.h"

#define SUPERBLOCK_OFFSET 1024

int disk_fd, group_offset;
struct ext2_super_block superblock;
struct ext2_group_desc group_desc;
__u32 block_size = 0;
__u32 inode_size = 0;
int total_groups = 0;

void printBfree(unsigned int block_bitmap_offset){

	// Read data into bitmap
	char *block_bytes = (char*)malloc(sizeof(block_size));
	pread(disk_fd, block_bytes, block_size, block_bitmap_offset);

	// Iterate over each byte
	int byte_counter;
	for(byte_counter = 0; byte_counter < block_size; byte_counter++){

		// Iterate over each bit
		int bit_counter;
		int bit_selector = 1;
		char curr_byte = block_bytes[byte_counter];
		for(bit_counter = 0; bit_counter < 8; bit_counter++){

			// If the current bit is 0, it is free
			if(!(curr_byte & bit_selector)){
				fprintf(stderr, "byte counter: %d\n", byte_counter);
				fprintf(stderr, "bit counter:%d\n", bit_counter);
				fprintf(stdout, "%s,%d\n", "BFREE", ((byte_counter*8)+bit_counter));
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

	return;
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

	// Get the offset for current group's block bitmap
	unsigned int block_bitmap_number = group_desc.bg_block_bitmap;
	unsigned int block_bitmap_offset = SUPERBLOCK_OFFSET+(block_bitmap_number-1)*block_size;
	printBfree(block_bitmap_offset);

	return;
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
		exit(1);
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