
#include <ext4.h>
#include <ext4_fs.h>
#include <ext4_mbr.h>
#include <ext4_mkfs.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

#define EXT4_FILEDEV_BSIZE 512

#define SECTOR_COUNT 0x80000

extern "C" int mem_file_dev_open(struct ext4_blockdev* bdev);

extern "C" int mem_file_dev_bread(struct ext4_blockdev* bdev, void* buf,
                                  uint64_t blk_id, uint32_t blk_cnt);

extern "C" int mem_file_dev_bwrite(struct ext4_blockdev* bdev, const void* buf,
                                   uint64_t blk_id, uint32_t blk_cnt);

extern "C" int mem_file_dev_close(struct ext4_blockdev* bdev);

extern "C" {

uint8_t p_buffer[EXT4_FILEDEV_BSIZE];

struct ext4_blockdev_iface s_i_face {
  .open = mem_file_dev_open, .bread = mem_file_dev_bread,
  .bwrite = mem_file_dev_bwrite, .close = mem_file_dev_close, .lock = nullptr,
  .unlock = nullptr, .ph_bsize = EXT4_FILEDEV_BSIZE, .ph_bbuf = p_buffer,
};
}

struct RamBlockDev {
  struct ext4_blockdev super;
  const char* path;
  std::fstream file;
  bool handled = true;

  RamBlockDev(const char* path) : path(path), file(), handled(true) {
    super.bdif = &s_i_face;
  }

  RamBlockDev(struct ext4_blockdev const& dev, const char* path)
      : super(dev), path(path), handled(false) {}

  ~RamBlockDev() {
    if (file.is_open()) {
      file.close();
    }
  }
};

extern "C" {

int mem_file_dev_open(struct ext4_blockdev* bdev) {
  auto dev = reinterpret_cast<RamBlockDev*>(bdev);

  if (dev->handled) {
    dev->super.part_offset = 0;
    dev->super.part_size = SECTOR_COUNT * EXT4_FILEDEV_BSIZE;
    dev->super.bdif->ph_bcnt = SECTOR_COUNT;
  }

  dev->file.open(dev->path,
                 std::fstream::in | std::fstream::out | std::fstream::binary);

  if (dev->handled) {
    if (!dev->file.is_open()) {
      // dev->file.open(dev->path, std::fstream::out | std::fstream::binary);

      std::ofstream out_file(dev->path, std::ios::binary);

      out_file.seekp(SECTOR_COUNT * EXT4_FILEDEV_BSIZE - 1);
      out_file.write("1", 1);
      out_file.close();
    }
  }

  if (!dev->file.is_open()) {
    dev->file.open(dev->path,
                   std::fstream::in | std::fstream::out | std::fstream::binary);
  }

  auto size = dev->file.tellg();

  dev->file.seekg(0, std::ios::end);

  auto file_size = dev->file.tellg() - size;

  if (file_size < SECTOR_COUNT * EXT4_FILEDEV_BSIZE) {
    return ENODATA;
  }

  dev->file.seekp(0, std::fstream::beg);

  return EOK;
}

int mem_file_dev_bread(struct ext4_blockdev* bdev, void* buf, uint64_t blk_id,
                       uint32_t blk_cnt) {
  auto dev = reinterpret_cast<RamBlockDev*>(bdev);

  auto offset = blk_id * EXT4_FILEDEV_BSIZE;

  dev->file.seekg(offset, std::fstream::beg);

  dev->file.read((char*)buf, blk_cnt * EXT4_FILEDEV_BSIZE);

  return EOK;
}

int mem_file_dev_bwrite(struct ext4_blockdev* bdev, const void* buf,
                        uint64_t blk_id, uint32_t blk_cnt) {
  auto dev = reinterpret_cast<RamBlockDev*>(bdev);

  auto offset = blk_id * EXT4_FILEDEV_BSIZE;

  dev->file.seekp(offset, std::fstream::beg);

  dev->file.write((char*)buf, blk_cnt * EXT4_FILEDEV_BSIZE);

  return EOK;
}

int mem_file_dev_close(struct ext4_blockdev* bdev) {
  auto dev = reinterpret_cast<RamBlockDev*>(bdev);

  if (!dev->file.is_open()) {
    return ENFILE;
  }

  dev->file.close();

  return EOK;
}
}

int main(int argc, const char** argv) {
  RamBlockDev mem_dev("demo_ext2.img");
  mem_dev.super.part_offset = 0;
  mem_dev.super.part_size = SECTOR_COUNT * EXT4_FILEDEV_BSIZE;

  ext4_dmask_set(DEBUG_ALL);

  // ext4_device_register(reinterpret_cast<struct ext4_blockdev*>(&mem_dev),
  //                      "ext2_root");

  struct ext4_mbr_parts parts {};
  parts.division[0] = 100;
  parts.division[1] = 0;
  parts.division[2] = 0;
  parts.division[3] = 0;

  // auto r = ext4_mbr_write(reinterpret_cast<struct ext4_blockdev*>(&mem_dev),
  //                         &parts, 0);

  // if (r != EOK) {
  //   std::cerr << "ext4_mbr_write : rc = " << r << std::endl;
  //   return -1;
  // }

  struct ext4_mbr_bdevs devs {};

  auto r =
      ext4_mbr_scan(reinterpret_cast<struct ext4_blockdev*>(&mem_dev), &devs);

  if (r != EOK) {
    std::cerr << "ext4_mbr_scan : rc = " << r << std::endl;
    return -1;
  }

  RamBlockDev sub_dev{devs.partitions[0], mem_dev.path};

  ext4_fs fs{};

  ext4_mkfs_info info{
      .block_size = 1024,
      .journal = false,
  };

  r = ext4_mkfs(&fs, reinterpret_cast<struct ext4_blockdev*>(&sub_dev), &info,
                F_SET_EXT2);

  if (r != EOK) {
    std::cerr << "ext4 mkfs : rc = " << r << std::endl;
    return -1;
  }

  ext4_device_register(reinterpret_cast<struct ext4_blockdev*>(&sub_dev),
                       "ext2_fs");

  r = ext4_mount("ext2_fs", "/", false);

  if (r != EOK) {
    std::cerr << "ext4_mount : rc = " << r << std::endl;

    return -1;
  }

  ext4_file file{};

  r = ext4_fopen(&file, "/h.md", "w+");

  if (r != EOK) {
    std::cerr << "ext4_fopen : rc = " << r << std::endl;

    return -1;
  }

  r = ext4_fwrite(&file, "a", 1, nullptr);

  if (r != EOK) {
    std::cerr << "ext4_fwrite : rc = " << r << std::endl;

    return -1;
  }

  ext4_fclose(&file);

  return 0;
}