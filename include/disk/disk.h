#pragma once
#include <cstdint>
#include <stdint.h>
namespace fs {
struct Superblock {
  // --- Standard Fields (Revision 0) ---
  uint32_t s_inodes_count;
  uint32_t s_blocks_count;
  uint32_t s_r_blocks_count;
  uint32_t s_free_blocks_count;
  uint32_t s_free_inodes_count;
  uint32_t s_first_data_block;
  uint32_t s_log_block_size;
  uint32_t s_log_frag_size;
  uint32_t s_blocks_per_group;
  uint32_t s_frags_per_group;
  uint32_t s_inodes_per_group;
  uint32_t s_mtime;
  uint32_t s_wtime;
  uint16_t s_mnt_count;
  uint16_t s_max_mnt_count;
  uint16_t s_magic; // 0xEF53
  uint16_t s_state;
  uint16_t s_errors;
  uint16_t s_minor_rev_level;
  uint32_t s_lastcheck;
  uint32_t s_checkinterval;
  uint32_t s_creator_os;
  uint32_t s_rev_level; // 0 for Revision 0, 1 for Revision 1+
  uint16_t s_def_resuid;
  uint16_t s_def_resgid;

  // --- Extended Fields (Revision 1+) ---
  // These fields are only valid if s_rev_level >= 1
  uint32_t s_first_ino;  // First non-reserved inode
  uint16_t s_inode_size; // Size of on-disk inode structure
  uint16_t s_block_group_nr;
  uint32_t s_feature_compat;
  uint32_t s_feature_incompat;
  uint32_t s_feature_ro_compat;
  uint8_t s_uuid[16];
  char s_volume_name[16];
  char s_last_mounted[64];
  uint32_t s_algo_bitmap;
  // ... further padding/reserved fields if needed
} __attribute__((packed));
struct Ext2Inode {
  uint16_t i_mode;
  uint16_t i_uid;
  uint32_t i_size;
  uint32_t i_atime;
  uint32_t i_ctime;
  uint32_t i_mtime;
  uint32_t i_dtime;
  uint16_t i_gid;
  uint16_t i_links_count;
  uint32_t i_blocks;
  uint32_t i_flags;
  uint32_t i_osd1;
  uint32_t i_block[15];
  uint32_t i_generation;
  uint32_t i_file_acl;
  uint32_t i_dir_acl;
  uint32_t i_faddr;
  uint8_t i_osd2[12];
} __attribute__((packed));
struct Ext2DirEntry {
  uint32_t inode;
  uint16_t rec_len;
  uint8_t name_len;
  uint8_t file_type;
  char name[255];
} __attribute__((packed));
struct Ext2GroupDescriptor {
  uint32_t bg_block_bitmap;      // Block id of block usage bitmap
  uint32_t bg_inode_bitmap;      // Block id of inode usage bitmap
  uint32_t bg_inode_table;       // Starting block id of inode table
  uint16_t bg_free_blocks_count; // Number of free blocks in group
  uint16_t bg_free_inodes_count; // Number of free inodes in group
  uint16_t bg_used_dirs_count;   // Number of directories in group
  uint16_t bg_pad;
  uint32_t bg_reserved[3];
} __attribute__((packed));

class Ext2Disk {
public:
  Ext2Disk(uint8_t drive_id);
  void mount();
  int read_sector(uint32_t lba, uint8_t* buffer);
  int read_sectors(uint32_t lba, uint8_t* buffer, uint32_t count);
  int write_sector(uint32_t lba, const uint8_t* buffer);
  uint8_t m_drive_id;
};
} // namespace fs
