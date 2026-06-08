#ifndef FLASH_DEF_H
#define FLASH_DEF_H

typedef enum {
    SR_FLASH_OK = 0,
    SR_FLASH_UNLOCK_ERR,
    SR_FLASH_WRITE_ERR,
    SR_FLASH_ERASE_ERR,
    SR_FLASH_READBACK_ERR,
    SR_FLASH_LOCK_ERR
} sr_flash_status_t;

#endif