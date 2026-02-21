#ifndef _NVM_H__
#define _NVM_H__

#include "stdint.h"
#include "stdbool.h"

#define NVM_ERR_NO 0
#define NVM_PARAM_ERR -1
#define NVM_ERR_CRC -2
#define NVM_ERR_LEN -3
#define NVM_ERR_EMPTY -4

#define CONFIG_NVM_PICES_0 0
#define CONFIG_NVM_PICES_1 1
#define CONFIG_NVM_PICES_2 2

#define CONFIG_NVM_PICES_NUM 3

void nvm_init(uint32_t flash_addr[], uint32_t sizes[]);  // for simulink
void nvm2_init(int nvm_nr, uint32_t flash_addr[], uint32_t flash_size);
int32_t nvm_read(uint32_t nvm_nr, uint8_t *buffer, uint32_t len);
int32_t nvm_write(uint32_t nvm_nr, uint8_t *buffer, uint32_t len);
void nvm_test(void);
#endif /*_NVM_H__ */
