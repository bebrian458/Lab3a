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

int disk_fd, group_offset;
struct ext2_super_block superblock;
struct ext2_group_desc group_desc;
struct ext2_inode curr_inode;
__u32 block_size = 0;
__u32 inode_size = 0;
int total_groups = 0;
int inode_nums;



void printInodeSummary(unsigned int inode_offset, int inode_num){

	pread(disk_fd, &curr_inode, sizeof(struct ext2_inode), inode_offset);

	// Check mode
	__u16 mode_value = curr_inode.i_mode & 0xFFF;
	char file_type = '?'; 					// default
	if(curr_inode.i_mode & 0x8000)			// regular file
		file_type = 'f';
	else if(curr_inode.i_mode & 0x4000) 	// directory
		file_type = 'd';
	else if(curr_inode.i_mode & 0xA000) 	// symlink
		file_type = 's';

//===========//

	// time_t temp;

	// time_t temp = curr_inode.i_ctime;
	// struct tm *info;
	// info = localtime(&temp);
	// char buf1[18];
	// strftime(buf1, 18, "%m/%d/%g %H:%M:%S", info);


	// time_t temp1 = curr_inode.i_mtime;
	// struct tm *mod;
	// mod = gmtime(&temp1);
	// char buf2[18];
	// strftime(buf2, 18, "%m/%d/%g %H:%M:%S", mod);

	// time_t temp2 = curr_inode.i_atime;
	// struct tm *access;
	// access = gmtime(&temp2);
	// char buf3[18];
	// strftime(buf3, 18, "%m/%d/%g %H:%M:%S", access);


	// check if non-zero mode and non-zero link count
	if(curr_inode.i_mode != 0 && curr_inode.i_links_count != 0){

		// fprintf(stderr, "Before time\n");

		// time_t *temp = malloc(sizeof(temp));

		// temp = (time_t)&curr_inode.i_ctime;

		// fprintf(stderr, "in the middle\n");

		// struct tm *info = gmtime(temp); 

		// fprintf(stderr, "After time\n");

		fprintf(stdout, "%s,%d,%c,%o,%d,%d,%d,%x,%x,%x,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", 
			"INODE",
			inode_num,
			file_type,
			mode_value,
			curr_inode.i_uid,
			curr_inode.i_gid,
			curr_inode.i_links_count,
			// buf1,
			// buf2,
			// buf3,
			curr_inode.i_ctime,
			curr_inode.i_mtime,
			curr_inode.i_atime,
			curr_inode.i_size,
			curr_inode.i_blocks,
			curr_inode.i_block[0], curr_inode.i_block[1], curr_inode.i_block[2], curr_inode.i_block[3],
			curr_inode.i_block[4], curr_inode.i_block[5], curr_inode.i_block[6], curr_inode.i_block[7], 
			curr_inode.i_block[8], curr_inode.i_block[9], curr_inode.i_block[10], curr_inode.i_block[11], 
			curr_inode.i_block[12], curr_inode.i_block[13], curr_inode.i_block[14]);
	}

}

void printBfreeOrIfree(unsigned int bitmap_offset, int isInode, int itable_offset, int max_num_inodes){
	
	// Read data into bitmap
	char *block_bytes = (char*)malloc(sizeof(block_size));
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
				// fprintf(stderr, "%d\n", itable_offset);
				unsigned int inode_offset = SUPERBLOCK_OFFSET 
				+ (itable_offset*block_size) 
				+ (curr_block_num-1)*sizeof(struct ext2_inode);
				printInodeSummary(inode_offset, curr_block_num);

				// TODO: read into inode_nums
			}
			
			bit_selector <<= 1;
		}
	}

	// Free blockbytes so that function an be reused for either BFREE or IFREE
	free(block_bytes);
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

	// Get the offset for current group's block bitmap and print free block entries
	unsigned int block_bitmap_number = group_desc.bg_block_bitmap;
	unsigned int inode_table_number = group_desc.bg_inode_table;
	unsigned int block_bitmap_offset = SUPERBLOCK_OFFSET+(block_bitmap_number-1)*block_size;
	printBfreeOrIfree(block_bitmap_offset, 0, inode_table_number-1, curr_num_inodes);

	// Get the offset for current group's inode bitmap and print free I-node entries
	unsigned int inode_bitmap_number = group_desc.bg_inode_bitmap;
	unsigned int inode_bitmap_offset = SUPERBLOCK_OFFSET+(inode_bitmap_number-1)*block_size;
	printBfreeOrIfree(inode_bitmap_offset, 1, inode_table_number-1, curr_num_inodes);

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