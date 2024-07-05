/********************************************************************
 * COPYRIGHT (C) 2024 Assa Abloy. All rights reserved.
 *
 * This file is part of Havasu and is proprietary to Assa Abloy.
 *
 * No part of this file may be copied, reproduced, modified, published,
 * uploaded, posted, transmitted, or distributed in any way, without
 * the prior written permission of Assa Abloy.
 *
 * Created by: FPT Software. Date: Feb 01, 2024
 ********************************************************************/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include "gdo_config.h"
#include "gdo_file_system_util.h"
#include <zephyr/devicetree.h>
#include <stdio.h>
#include <string.h>
#include "gdo_user_infor_util.h"
#include "gdo_schedule.h"
K_MUTEX_DEFINE(fileaccess);

// static FATFS fat_fs;
// /* mounting info */
// static struct fs_mount_t mp = {
//     .type    = FS_FATFS,
//     .fs_data = &fat_fs,
// };

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);

/* Matches LFS_NAME_MAX */
#define MAX_PATH_LEN   255
#define TEST_FILE_SIZE 547

static uint8_t file_test_pattern[TEST_FILE_SIZE];

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
    .type        = FS_LITTLEFS,
    .fs_data     = &storage,
    .storage_dev = (void *) FIXED_PARTITION_ID(littlefs_storage),
    // .storage_dev = (void *)FIXED_PARTITION_ID(storage),
    // .storage_dev = (void *)FLASH_AREA_ID(storage),
    // .storage_dev = (void *)FLASH_AREA_ID(external_flash),
    .mnt_point = "/lfs1",
};

struct fs_mount_t *mountpoint = &lfs_storage_mnt;
static int littlefs_flash_erase(unsigned int id);
static int littlefs_mount(struct fs_mount_t *mp)
{
  int rc;

  rc = littlefs_flash_erase((uintptr_t)mp->storage_dev);
  if (rc < 0)
  {
  	return rc;
  }

  rc = fs_mount(mp);
  if (rc < 0) {
    LOG_PRINTK("FAIL: mount id %" PRIuPTR " at %s: %d\n", (uintptr_t) mp->storage_dev, mp->mnt_point, rc);
    return rc;
  }
  LOG_PRINTK("%s mount: %d\n", mp->mnt_point, rc);

  return 0;
}

static int lsdir(const char *path)
{
  int res;
  struct fs_dir_t dirp;
  static struct fs_dirent entry;

  fs_dir_t_init(&dirp);

  /* Verify fs_opendir() */
  res = fs_opendir(&dirp, path);
  if (res) {
    LOG_ERR("Error opening dir %s [%d]\n", path, res);
    return res;
  }

  LOG_PRINTK("\nListing dir %s ...\n", path);
  for (;;) {
    /* Verify fs_readdir() */
    res = fs_readdir(&dirp, &entry);

    /* entry.name[0] == 0 means end-of-dir */
    if (res || entry.name[0] == 0) {
      if (res < 0) {
        LOG_ERR("Error reading dir [%d]\n", res);
      }
      break;
    }

    if (entry.type == FS_DIR_ENTRY_DIR) {
      LOG_PRINTK("[DIR ] %s\n", entry.name);
    } else {
      LOG_PRINTK("[FILE] %s (size = %zu)\n", entry.name, entry.size);
    }
  }

  /* Verify fs_closedir() */
  fs_closedir(&dirp);

  return res;
}

static int littlefs_increase_infile_value(char *fname)
{
  uint8_t boot_count = 0;
  struct fs_file_t file;
  int rc, ret;

  fs_file_t_init(&file);
  rc = fs_open(&file, fname, FS_O_CREATE | FS_O_RDWR);
  if (rc < 0) {
    LOG_ERR("FAIL: open %s: %d", fname, rc);
    return rc;
  }

  rc = fs_read(&file, &boot_count, sizeof(boot_count));
  if (rc < 0) {
    LOG_ERR("FAIL: read %s: [rd:%d]", fname, rc);
    goto out;
  }
  LOG_PRINTK("%s read count:%u (bytes: %d)\n", fname, boot_count, rc);

  rc = fs_seek(&file, 0, FS_SEEK_SET);
  if (rc < 0) {
    LOG_ERR("FAIL: seek %s: %d", fname, rc);
    goto out;
  }

  boot_count += 1;
  rc = fs_write(&file, &boot_count, sizeof(boot_count));
  if (rc < 0) {
    LOG_ERR("FAIL: write %s: %d", fname, rc);
    goto out;
  }

  LOG_PRINTK("%s write new boot count %u: [wr:%d]\n", fname, boot_count, rc);

out:
  ret = fs_close(&file);
  if (ret < 0) {
    LOG_ERR("FAIL: close %s: %d", fname, ret);
    return ret;
  }

  return (rc < 0 ? rc : 0);
}

static void incr_pattern(uint8_t *p, uint16_t size, uint8_t inc)
{
  uint8_t fill = 0x55;

  if (p[0] % 2 == 0) {
    fill = 0xAA;
  }

  for (int i = 0; i < (size - 1); i++) {
    if (i % 8 == 0) {
      p[i] += inc;
    } else {
      p[i] = fill;
    }
  }

  p[size - 1] += inc;
}

static void init_pattern(uint8_t *p, uint16_t size)
{
  uint8_t v = 0x1;

  memset(p, 0x55, size);

  for (int i = 0; i < size; i += 8) {
    p[i] = v++;
  }

  p[size - 1] = 0xAA;
}

static void print_pattern(uint8_t *p, uint16_t size)
{
  int i, j = size / 16, k;

  for (k = 0, i = 0; k < j; i += 16, k++) {
    LOG_PRINTK("%02x %02x %02x %02x %02x %02x %02x %02x ",
               p[i],
               p[i + 1],
               p[i + 2],
               p[i + 3],
               p[i + 4],
               p[i + 5],
               p[i + 6],
               p[i + 7]);
    LOG_PRINTK("%02x %02x %02x %02x %02x %02x %02x %02x\n",
               p[i + 8],
               p[i + 9],
               p[i + 10],
               p[i + 11],
               p[i + 12],
               p[i + 13],
               p[i + 14],
               p[i + 15]);

    /* Mark 512B (sector) chunks of the test file */
    if ((k + 1) % 32 == 0) {
      LOG_PRINTK("\n");
    }
  }

  for (; i < size; i++) {
    LOG_PRINTK("%02x ", p[i]);
  }

  LOG_PRINTK("\n");
}

static int littlefs_binary_file_adj(char *fname)
{
  struct fs_dirent dirent;
  struct fs_file_t file;
  int rc, ret;

  /*
	 * Uncomment below line to force re-creation of the test pattern
	 * file on the littlefs FS.
	 */
  /* fs_unlink(fname); */
  fs_file_t_init(&file);

  rc = fs_open(&file, fname, FS_O_CREATE | FS_O_RDWR);
  if (rc < 0) {
    LOG_ERR("FAIL: open %s: %d", fname, rc);
    return rc;
  }

  rc = fs_stat(fname, &dirent);
  if (rc < 0) {
    LOG_ERR("FAIL: stat %s: %d", fname, rc);
    goto out;
  }

  /* Check if the file exists - if not just write the pattern */
  if (rc == 0 && dirent.type == FS_DIR_ENTRY_FILE && dirent.size == 0) {
    LOG_INF("Test file: %s not found, create one!", fname);
    init_pattern(file_test_pattern, sizeof(file_test_pattern));
  } else {
    rc = fs_read(&file, file_test_pattern, sizeof(file_test_pattern));
    if (rc < 0) {
      LOG_ERR("FAIL: read %s: [rd:%d]", fname, rc);
      goto out;
    }
    incr_pattern(file_test_pattern, sizeof(file_test_pattern), 0x1);
  }

  LOG_PRINTK("------ FILE: %s ------\n", fname);
  print_pattern(file_test_pattern, sizeof(file_test_pattern));

  rc = fs_seek(&file, 0, FS_SEEK_SET);
  if (rc < 0) {
    LOG_ERR("FAIL: seek %s: %d", fname, rc);
    goto out;
  }

  rc = fs_write(&file, file_test_pattern, sizeof(file_test_pattern));
  if (rc < 0) {
    LOG_ERR("FAIL: write %s: %d", fname, rc);
  }

out:
  ret = fs_close(&file);
  if (ret < 0) {
    LOG_ERR("FAIL: close %s: %d", fname, ret);
    return ret;
  }

  return (rc < 0 ? rc : 0);
}

static int littlefs_flash_erase(unsigned int id)
{
  const struct flash_area *pfa;
  int rc;

  rc = flash_area_open(id, &pfa);
  if (rc < 0) {
    LOG_ERR("FAIL: unable to find flash area %u: %d\n", id, rc);
    return rc;
  }

  LOG_PRINTK("Area %u at 0x%x on %s for %u bytes\n",
             id,
             (unsigned int) pfa->fa_off,
             pfa->fa_dev->name,
             (unsigned int) pfa->fa_size);

  /* Optional wipe flash contents */
  if (IS_ENABLED(CONFIG_APP_WIPE_STORAGE)) {
    rc = flash_area_erase(pfa, 0, pfa->fa_size);
    LOG_ERR("Erasing flash area ... %d", rc);
  }

  flash_area_close(pfa);
  return rc;
}

void gdo_littlefs_test()
{
  char fname1[MAX_PATH_LEN];
  char fname2[MAX_PATH_LEN];
  struct fs_statvfs sbuf;
  int rc;

  LOG_INF("Sample program to r/w files on littlefs\n");

  rc = littlefs_mount(mountpoint);
  if (rc < 0) {
    return 0;
  }

  snprintf(fname1, sizeof(fname1), "%s/boot_count", mountpoint->mnt_point);
  snprintf(fname2, sizeof(fname2), "%s/pattern.bin", mountpoint->mnt_point);

  rc = fs_statvfs(mountpoint->mnt_point, &sbuf);
  if (rc < 0) {
    LOG_INF("FAIL: statvfs: %d\n", rc);
    goto out;
  }

  LOG_INF("%s: bsize = %lu ; frsize = %lu ;"
          " blocks = %lu ; bfree = %lu\n",
          mountpoint->mnt_point,
          sbuf.f_bsize,
          sbuf.f_frsize,
          sbuf.f_blocks,
          sbuf.f_bfree);

  rc = lsdir(mountpoint->mnt_point);
  if (rc < 0) {
    LOG_INF("FAIL: lsdir %s: %d\n", mountpoint->mnt_point, rc);
    goto out;
  }

  rc = littlefs_increase_infile_value(fname1);
  if (rc) {
    goto out;
  }

  rc = littlefs_binary_file_adj(fname2);
  if (rc) {
    goto out;
  }

out:
  rc = fs_unmount(mountpoint);
  LOG_INF("%s unmount: %d\n", mountpoint->mnt_point, rc);
  return 0;
}
/*======================fatfs===================*/
static int gdo_disk_init(const char *disk);

bool gdo_flash_earse_region(off_t region_offset, size_t sector_size)
{
  const struct device *flash_dev = DEVICE_DT_GET(DT_ALIAS(spi_flash0));

  if (!device_is_ready(flash_dev)) {
    LOG_ERR("%s: device not ready.\n", flash_dev->name);
    return false;
  }
  int rc = flash_erase(flash_dev, region_offset, sector_size);
  if (rc != 0) {
    LOG_ERR("Flash erase failed! %d\n", rc);
    return false;
  } else {
    LOG_INF("Flash erase succeeded!\n");
  }
  return true;
}

bool gdo_flash_write_offset(off_t region_offset, uint8_t *buff_write, size_t len)
{
  const struct device *flash_dev = DEVICE_DT_GET(DT_ALIAS(spi_flash0));

  if (!device_is_ready(flash_dev)) {
    LOG_ERR("%s: device not ready.\n", flash_dev->name);
    return false;
  }
  int rc = flash_write(flash_dev, region_offset, buff_write, len);
  if (rc != 0) {
    LOG_ERR("Flash write failed! %d\n", rc);
    return false;
  }
  return true;
}

bool gdo_flash_read_offset(off_t region_offset, uint8_t *buff_read, size_t len)
{
  const struct device *flash_dev = DEVICE_DT_GET(DT_ALIAS(spi_flash0));

  if (!device_is_ready(flash_dev)) {
    LOG_ERR("%s: device not ready.\n", flash_dev->name);
    return false;
  }
  int rc = flash_read(flash_dev, region_offset, buff_read, len);
  if (rc != 0) {
    LOG_ERR("Flash write failed! %d", rc);
    return false;
  }
  return true;
}

int gdo_fs_delete_all_file(const char *disk, const char *path)
{
  k_mutex_lock(&fileaccess, K_FOREVER);
  int res = 0;
  struct fs_dir_t dirp;
  static struct fs_dirent entry;
  int count = 0;
  char path_temp[GDO_FS_MAX_PATH_LEN];

  fs_dir_t_init(&dirp);

  /* Verify fs_opendir() */
  res = fs_opendir(&dirp, path);
  if (res) {
    LOG_ERR("Error opening dir %s [%d]\n", path, res);
    k_mutex_unlock(&fileaccess);
    return res;
  }
  for (;;) {
    /* Verify fs_readdir() */
    res = fs_readdir(&dirp, &entry);

    /* entry.name[0] == 0 means end-of-dir */
    if (res || entry.name[0] == 0) {
      break;
    }
    strcpy(path_temp, path);
    strcat(path_temp, "/");
    strcat(path_temp, entry.name);
    fs_unlink(path_temp);
    if (entry.type == FS_DIR_ENTRY_DIR) {
      LOG_ERR("[DIR ] %s\n", entry.name);
    } else {
      LOG_ERR("[FILE] %s (size = %zu)\n", entry.name, entry.size);
    }
    count++;
  }
  res = count;
  k_mutex_unlock(&fileaccess);
  return res;
}

int gdo_disk_init(const char *disk)
{
  static const char *disk_pdrv = GDO_DISK_DRIVE_NAME;
  return 0;
//   if (disk_access_init(disk_pdrv) != 0) {
//     LOG_ERR("Storage init ERROR!");
//     return -1;
//   }
//   mp.type = FS_FATFS, mp.fs_data = &fat_fs, mp.mnt_point = disk;
//   int res = fs_mount(&mp);
// #if defined(CONFIG_FAT_FILESYSTEM_ELM)
//   if (res == FR_OK) {
//     return 0;
// #else
//   if (res == 0) {
//     LOG_ERR("Mount success\n");
//     return 0;
// #endif
//   } else {
//     LOG_ERR("Error mounting disk.\n");
//     return -1;
//   }
//   return -1;
}

static int gdo_fs_deinit()
{
  return 0;
  // return fs_unmount(&mp);
}

bool gdo_fs_create_file(const char *full_path_file, size_t size_file)
{
  if (GDO_FS_MAX_PATH_LEN <= strlen(full_path_file)) {
    LOG_ERR("FS-Create File-ERR: file path too long");
    return false;
  }
  k_mutex_lock(&fileaccess, K_FOREVER);
  struct fs_file_t file;
  fs_file_t_init(&file);
  LOG_INF("Create file %s", full_path_file);

  if (fs_open(&file, full_path_file, FS_O_CREATE | FS_O_RDWR) != 0) {
    LOG_ERR("FS-Create File-ERR: create file %s", full_path_file);
    k_mutex_unlock(&fileaccess);
    return false;
  }

  if (fs_truncate(&file, 0) != 0) {
    LOG_ERR("Failed to shirk file");
    fs_close(&file);
    k_mutex_unlock(&fileaccess);
    return false;
  }

  if (fs_truncate(&file, size_file) != 0) {
    LOG_ERR("Failed to extend file to: %lu bytes", size_file);
    fs_close(&file);
    k_mutex_unlock(&fileaccess);
    return false;
  }
  fs_sync(&file);

  fs_close(&file);
  k_mutex_unlock(&fileaccess);
  return true;
}

int gdo_fs_read_file(const char *disk, const char *full_path_file, void *buff, size_t len)
{
  k_mutex_lock(&fileaccess, K_FOREVER);
  int res = 0;
  struct fs_file_t file;

  fs_file_t_init(&file);
  res = fs_open(&file, full_path_file, FS_O_READ);
  if (res != 0) {
    LOG_ERR("Failed to open file %s Err %d\n", full_path_file, res);
    k_mutex_unlock(&fileaccess);
    return res;
  }
  res = fs_read(&file, buff, len);

  if (res < 0) {
    LOG_ERR("Error read file %s\n", full_path_file);
  }
  if (res != len) {
    LOG_ERR("Error len file %d bytes, you read %d bytes\n", res, len);
    res = 0;
  }
  fs_close(&file);
  k_mutex_unlock(&fileaccess);
  return res;
}

int gdo_fs_write_file(const char *disk, const char *full_path_file, void *buff, size_t len)
{
  k_mutex_lock(&fileaccess, K_FOREVER);
  int res = 0;
  struct fs_file_t file;

  fs_file_t_init(&file);
  res = fs_open(&file, full_path_file, FS_O_APPEND | FS_O_WRITE);
  if (res != 0) {
    LOG_ERR("Failed to open file %s err %d\n", full_path_file, res);
    k_mutex_unlock(&fileaccess);
    return res;
  }

  res = fs_write(&file, buff, len);
  if (res < 0 || res != len) {
    LOG_ERR("Error write file %s , ret: %d\n", full_path_file, res);
    k_mutex_unlock(&fileaccess);
    res = -1;
  }
  fs_close(&file);
  k_mutex_unlock(&fileaccess);
  return res;
}

int gdo_fs_write_file_index(const char *disk, const char *full_path_file, void *buff, size_t len, size_t index)
{
  k_mutex_lock(&fileaccess, K_FOREVER);
  int res = 0;
  struct fs_file_t file;

  fs_file_t_init(&file);
  res = fs_open(&file, full_path_file, FS_O_WRITE);
  if (res != 0) {
    LOG_ERR("Failed to open file %s err %d\n", full_path_file, res);
    k_mutex_unlock(&fileaccess);
    return res;
  }
  res = fs_seek(&file, index, FS_SEEK_SET);
  if (res != 0) {
    LOG_ERR("Failed to seek file %s \n", full_path_file);
    k_mutex_unlock(&fileaccess);
    return res;
  }
  res = fs_write(&file, buff, len);
  if (res < 0 || res != len) {
    LOG_ERR("Error write file %s\n", full_path_file);
    res = -1;
  }
  fs_close(&file);
  k_mutex_unlock(&fileaccess);
  return res;
}

int gdo_fs_read_file_index(const char *disk, const char *full_path_file, void *buff, size_t len, size_t index)
{
  k_mutex_lock(&fileaccess, K_FOREVER);
  int res = 0;
  struct fs_file_t file;
  fs_file_t_init(&file);
  res = fs_open(&file, full_path_file, FS_O_READ);
  if (res != 0) {
    LOG_ERR("Failed to open file %s error %d\n", full_path_file, res);
    k_mutex_unlock(&fileaccess);
    return res;
  }
  res = fs_seek(&file, index, FS_SEEK_SET);
  if (res != 0) {
    LOG_ERR("Failed to seek file %s \n", full_path_file);
    fs_close(&file);
    k_mutex_unlock(&fileaccess);
    return res;
  }
  res = fs_read(&file, buff, len);

  if (res < 0) {
    LOG_ERR("Error read file %s\n", full_path_file);
    res = -1;
  }

  if (res != len) {
    LOG_ERR("Error len file %d bytes, you read %d bytes, index %d\n", res, len, index);
    res = -1;
  }
  fs_close(&file);
  k_mutex_unlock(&fileaccess);
  return res;
}

bool gdo_fs_delete_file(const char *disk, const char *full_path_file)
{
  bool flag = true;
  k_mutex_lock(&fileaccess, K_FOREVER);
  if (strcmp(full_path_file, GDO_USER_INFOR_FULL_PATH) == 0) {
    flag = gdo_fs_create_file(GDO_USER_INFOR_FULL_PATH, GDO_MAX_USER_SUPORT * sizeof(gdo_user_infor));
  }
  if (strcmp(full_path_file, SCHEDULE_CURRENT_FILE_FULL_PATH) == 0) {
    flag = gdo_fs_create_file(SCHEDULE_CURRENT_FILE_FULL_PATH, SCHEDULE_NUM * sizeof(struct schedule_data));
  }

  if (strcmp(full_path_file, SCHEDULE_BACKUP_FILE_FULL_PATH) == 0) {
    flag = gdo_fs_create_file(SCHEDULE_BACKUP_FILE_FULL_PATH, SCHEDULE_NUM * sizeof(struct schedule_data));
  }

  if (strcmp(full_path_file, HOME_CFG_FILE_FULL_PATH) == 0) {
    flag = gdo_fs_create_file(HOME_CFG_FILE_FULL_PATH, HOME_CFG_FILE_SIZE);
  }
  k_mutex_unlock(&fileaccess);
  return flag;
}

uint8_t gdo_fs_file_exist(const char *full_path_file)
{
  int res = 0;
  struct fs_file_t file;
  uint8_t rs = 0;
  k_mutex_lock(&fileaccess, K_FOREVER);
  fs_file_t_init(&file);
  res = fs_open(&file, full_path_file, FS_O_READ);
  if (res == 0) {
    fs_close(&file);
    rs = FILE_EXIST;
    goto exit;
  }

  if (res == -ENOENT) {
    rs = FILE_NOT_EXIST;
    goto exit;
  }
  rs = FILE_ERROR;
exit:
  k_mutex_unlock(&fileaccess);
  return rs;
}

bool createFileIfNotExist()
{
  bool flag = true;
  if (gdo_fs_file_exist(GDO_USER_INFOR_FULL_PATH) == FILE_NOT_EXIST) {
    flag &= gdo_fs_create_file(GDO_USER_INFOR_FULL_PATH, GDO_MAX_USER_SUPORT * sizeof(gdo_user_infor));
  }

  if (gdo_fs_file_exist(SCHEDULE_CURRENT_FILE_FULL_PATH) == FILE_NOT_EXIST) {
    flag &= gdo_fs_create_file(SCHEDULE_CURRENT_FILE_FULL_PATH, SCHEDULE_NUM * sizeof(struct schedule_data));
  }

  if (gdo_fs_file_exist(SCHEDULE_BACKUP_FILE_FULL_PATH) == FILE_NOT_EXIST) {
    flag &= gdo_fs_create_file(SCHEDULE_BACKUP_FILE_FULL_PATH, SCHEDULE_NUM * sizeof(struct schedule_data));
  }

  if (gdo_fs_file_exist(HOME_CFG_FILE_FULL_PATH) == FILE_NOT_EXIST) {
    flag &= gdo_fs_create_file(HOME_CFG_FILE_FULL_PATH, HOME_CFG_FILE_SIZE);
  }
  return flag;
}

bool gdo_file_system_init()
{
  /*read build timer in ex flash */
  /*compare*/
  LOG_INF("Build time %s", BUILD_TIMESTAMP);
  char date[20];
  gdo_flash_read_offset(GDO_BUILD_TIME_OFFSET, (uint8_t *) date, sizeof(BUILD_TIMESTAMP));
  /*build time is same*/
  LOG_INF("build save %s", date);
  memset(date, 0, 20);
  if (strcmp(date, BUILD_TIMESTAMP) == 0) {
    LOG_INF("FILE NOT RESET");
    if (gdo_disk_init(GDO_DISK_MOUNT_PT) != 0) {
      LOG_ERR("FS-INIT: disk");
      return false;
    }
    return createFileIfNotExist();
  }
  gdo_flash_earse_region(GDO_BUILD_TIME_OFFSET, 4096);
  gdo_flash_write_offset(GDO_BUILD_TIME_OFFSET, BUILD_TIMESTAMP, sizeof(BUILD_TIMESTAMP));

  if (GDO_FS_INIT_TYPE == GDO_FS_FORMAT) {
    if (!gdo_flash_earse_region(SPI_FLASH_FS_REGION_OFFSET, SPI_FLASH_FS_SECTOR_SIZE)) {
      LOG_ERR("FS-INIT:Error format flash");
      return false;
    }
    if (gdo_disk_init(GDO_DISK_MOUNT_PT) != 0) {
      LOG_ERR("FS-INIT: disk");
      return false;
    }
    return gdo_fs_create_file(GDO_USER_INFOR_FULL_PATH, GDO_MAX_USER_SUPORT * sizeof(gdo_user_infor)) &&
           gdo_fs_create_file(SCHEDULE_CURRENT_FILE_FULL_PATH, SCHEDULE_NUM * sizeof(struct schedule_data)) &&
           gdo_fs_create_file(SCHEDULE_BACKUP_FILE_FULL_PATH, SCHEDULE_NUM * sizeof(struct schedule_data)) &&
           /*todo log file here*/
           gdo_fs_create_file(HOME_CFG_FILE_FULL_PATH, HOME_CFG_FILE_SIZE);
  }

  if (gdo_disk_init(GDO_DISK_MOUNT_PT) != 0) {
    LOG_ERR("FS-INIT: disk");
    return false;
  }
  if (GDO_FS_INIT_TYPE == GDO_FS_NO_CHANGE) {
    /*do nothing*/
    return createFileIfNotExist();
  }
  bool flag = true;
  if (GDO_FS_INIT_TYPE & GDO_FS_LOG_FILE) {
    // todo log file
  }

  if (GDO_FS_INIT_TYPE & GDO_FS_USER_INFO) {
    /*neu file da ton tai*/
    flag = gdo_fs_create_file(GDO_USER_INFOR_FULL_PATH, GDO_MAX_USER_SUPORT * sizeof(gdo_user_infor));
  }

  if (GDO_FS_INIT_TYPE & GDO_FS_SCHEDULE) {
    flag = gdo_fs_create_file(SCHEDULE_CURRENT_FILE_FULL_PATH, SCHEDULE_NUM * sizeof(struct schedule_data)) &&
           gdo_fs_create_file(SCHEDULE_BACKUP_FILE_FULL_PATH, SCHEDULE_NUM * sizeof(struct schedule_data));
  }

  if (GDO_FS_INIT_TYPE & GDO_FS_HOME_CFG) {
    flag = gdo_fs_create_file(HOME_CFG_FILE_FULL_PATH, HOME_CFG_FILE_SIZE);
  }
  return flag && createFileIfNotExist();
}