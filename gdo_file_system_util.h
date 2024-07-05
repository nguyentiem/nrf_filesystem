#ifndef _FILE_SYSTEM_H_
#define _FILE_SYSTEM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

enum file_status {
  FILE_ERROR = 0x00,
  FILE_EXIST,
  FILE_NOT_EXIST,
};

/**
 * @brief Creates a new file in the file system.
 *
 * This function creates a new file with the specified name in the given disk at the specified base path.
 *
 * @param[in] disk      The name of the disk where the file will be created. For example, "SD", etc.
 * @param[in] file_name The name of the file to be created.
 * @param[in] base_path The base path where the file will be created. This path should end with a directory separator.
 *
 * @return true if the file was successfully created, false otherwise.
 *
 * @note If the file already exists, this function will return false without creating a new file.
 * @note The `disk` parameter should be a valid disk name accessible by the file system.
 * @note The `file_name` parameter should not contain any directory separators ( "\","_").
 * @note The `base_path` parameter should be a valid base path in the file system where the file will be created.
 * @note The function may return false if there are permission issues or if the disk is full.
 *
 */
bool gdo_fs_create_file(const char *full_path_file, size_t size_file);

/**
 * @brief Writes data to a file in the file system.
 *
 * This function writes the specified data buffer to the file located at the given full path in the specified disk.
 *
 * @param[in] disk            The name of the disk where the file exists. For example, "SD", "USB", etc.
 * @param[in] full_path_file  The full path of the file where the data will be written.
 * @param[in] buff            A pointer to the buffer containing the data to be written.
 * @param[in] len             The length of the data buffer in bytes.
 *
 * @return An integer representing the number of bytes successfully written to the file. 
 *         Returns -1 if an error occurs during the write operation.
 *
 * @note The `disk` parameter should be a valid disk name accessible by the file system.
 * @note The `full_path_file` parameter should be the full path of the file in the file system where the data will be written.
 * @note The `buff` parameter should point to a valid memory location containing the data to be written.
 * @note The `len` parameter should be the length of the data buffer in bytes.
 * @note The function may return -1 if there are permission issues, disk full, or if the file does not exist.
 */
int gdo_fs_write_file(const char *disk, const char *full_path_file, void *buff, size_t len);

/**
 * @brief Reads data from a file in the file system.
 *
 * This function reads data from the file located at the specified full path in the given disk and stores it in the provided buffer.
 *
 * @param[in] disk            The name of the disk where the file exists. For example, "SD", "USB", etc.
 * @param[in] full_path_file  The full path of the file from which data will be read.
 * @param[out] buff           A pointer to the buffer where the read data will be stored.
 * @param[in] len             The maximum number of bytes to read from the file.
 *
 * @return An integer representing the number of bytes successfully read from the file.
 *         Returns -1 if an error occurs during the read operation.
 *
 * @note The `disk` parameter should be a valid disk name accessible by the file system.
 * @note The `full_path_file` parameter should be the full path of the file in the file system from which data will be read.
 * @note The `buff` parameter should point to a valid memory location where the read data will be stored.
 * @note The `len` parameter should be the maximum number of bytes to read from the file.
 * @note The function may return -1 if there are permission issues, if the file does not exist, or if an error occurs during the read operation.
 *
 */
int gdo_fs_read_file(const char *disk, const char *full_path_file, void *buff, size_t len);

/**
 * @brief Writes data to a specific index within a file in the file system.
 *
 * This function writes the specified data buffer to the specified index within the file located at the given full path in the specified disk.
 *
 * @param[in] disk            The name of the disk where the file exists. For example, "SD", "USB", etc.
 * @param[in] full_path_file  The full path of the file where the data will be written.
 * @param[in] buff            A pointer to the buffer containing the data to be written.
 * @param[in] len             The length of the data buffer in bytes.
 * @param[in] index           The index within the file where the data will be written.
 *
 * @return An integer representing the number of bytes successfully written to the file at the specified index. 
 *         Returns -1 if an error occurs during the write operation.
 *
 */
int gdo_fs_write_file_index(const char *disk, const char *full_path_file, void *buff, size_t len, size_t index);

/**
 * @brief Reads data from a specific index within a file in the file system.
 *
 * This function reads data from the specified index within the file located at the given full path in the specified disk and stores it in the provided buffer.
 *
 * @param[in] disk            The name of the disk where the file exists. For example, "SD", "USB", etc.
 * @param[in] full_path_file  The full path of the file from which data will be read.
 * @param[out] buff           A pointer to the buffer where the read data will be stored.
 * @param[in] len             The maximum number of bytes to read from the file.
 * @param[in] index           The index within the file from which data will be read.
 *
 * @return An integer representing the number of bytes successfully read from the file at the specified index.
 *         Returns -1 if an error occurs during the read operation.
 *
 */
int gdo_fs_read_file_index(const char *disk, const char *full_path_file, void *buff, size_t len, size_t index);
/**
 * @brief Deletes all files within a specified directory in the file system.
 *
 * This function deletes all files located within the specified directory path in the given disk.
 *
 * @param[in] disk  The name of the disk where the files exist. For example, "SD", "USB", etc.
 * @param[in] path  The path of the directory from which all files will be deleted.
 *
 * @return An integer representing the number of files successfully deleted.
 *         Returns -1 if an error occurs during the deletion process.
 *
 */
int gdo_fs_delete_all_file(const char *disk, const char *path);

bool gdo_flash_earse_region(off_t region_offset, size_t sector_size);

bool gdo_fs_delete_file(const char *disk, const char *full_path_file);

// int lsdir(const char *disk, const char *path);

bool gdo_flash_write_offset(off_t region_offset, uint8_t *buff_write, size_t len);

bool gdo_flash_read_offset(off_t region_offset, uint8_t *buff_read, size_t len);

bool gdo_file_system_init();

void gdo_littlefs_test(); 
#ifdef __cplusplus
}
#endif
#endif /*_FILE_SYSTEM_H_*/