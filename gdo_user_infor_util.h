#ifndef _GDO_USER_INFOR_UTIL_H_
#define _GDO_USER_INFOR_UTIL_H_

#include <stdint.h>
#include "gdo_config.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
// #include"gdo_lock_config.h"
enum user_infor_err_t {
  USER_UTIL_NO_SLOT_EMPTY  = 2,
  USER_UTIL_USER_NOT_EXIST = 3,
  USER_UTIL_USER_INDEX_INVALID,
  USER_UTIL_ACCESS_FILE_ERR,
};
enum user_role {
  USER_OWNER = 0x01,
  USER_ADMIN = 0x02,
  USER_GUEST = 0x03,
  USER_OTHER,
};

enum user_status {
  USER_NOT_EXIST = 0x00,
  USER_ACTIVE    = 0x01,
  USER_INACTIVE  = 0x02, /*pending*/
  USER_SHARE,
};

typedef struct {
  uint8_t user_name[GDO_MAX_USER_NAME_LEN]; // sha256
  uint8_t pub_key[GDO_ECDH_PUBLIC_KEY_LEN];
  uint8_t user_role;
  uint8_t user_status;
  uint8_t user_info[GDO_USER_INFOR_LEN];
  uint8_t temp_key[GDO_USER_NUM_TEMP_KEY][GDO_USER_TEMP_KEY_LEN];
} gdo_user_infor;

/*
   * @brief add specific user to the user list then store to the protect memory.
   *
   * @return Returns number user linked  on success, or a negative error code indicating failure.
   *
   */
int gdo_user_add_user(gdo_user_infor *user, uint32_t *user_list, size_t *number_user);

/*
   * @brief remove specific user to the user list then store to the protect memory.
   *
   * @return Returns number user linked  on success, or a negative error code indicating failure.
   *
   */
int gdo_user_remove_user(gdo_user_infor *user, uint32_t *user_list, size_t *number_user);

/*
   * @brief clear the user list then update to the protect memory.
   *
   * @return Returns 0  on success, or a negative error code indicating failure.
   *
   */
int remove_all_user(uint32_t *user_list, size_t *number_user);

/**
   * @brief Load the user list that linked to GDO from the protect memory.
   *
   * @return Returns number user linked on success, or a negative error code indicating failure.
   *
   */
int gdo_user_infor_load_user_list(uint32_t *user_list);

/**
   * @brief Check user with user name is appear in list of user.
   *
   *
   * @param user Pointer to the user information structure to be checked.
   * @return Returns 0 if the user information is valid and exists in the database, or a negative error code indicating failure.
   *
   */
int gdo_user_check_user(gdo_user_infor *user, uint32_t *user_list);

/*
   *@brief Update information of user exist in user list.
   *
   *
   * @param user Pointer to the user information structure to be checked.
   * @return Returns number of user list if  success, or a negative error code indicating failure.
   *        -1:
   *        -2: user not exist in user list
   */
int gdo_user_update(gdo_user_infor *user, uint32_t *user_list, size_t *number_user);

#if (GDO_USER_LIST_TEST)
int gdo_user_infor_unit_test();
#endif
#ifdef __cplusplus
}
#endif

#endif