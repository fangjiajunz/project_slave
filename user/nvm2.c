#include <string.h>
#include "crc16.h"
#include "nvm.h"
#include "stm32f1xx_hal.h"

#define LOG_TAG "NVM"
#include "log.h"

#define NVM_MAGIC 0x12349876

/**
 * @brief  写数据到 Flash
 * @param  addr   目标 Flash 地址（必须在 Flash 区域内）
 * @param  data   要写入的数据
 * @param  len    数据长度（字节）
 * @param  erase  是否先擦除该地址所在页
 */
static void flash_write_page(uint32_t addr, const uint8_t *data, uint32_t len, bool erase)
{
    HAL_FLASH_Unlock();

    if (erase)
    {
        FLASH_EraseInitTypeDef erase_init;
        uint32_t page_error = 0;

        erase_init.TypeErase   = FLASH_TYPEERASE_PAGES;
        erase_init.PageAddress = addr & ~(FLASH_PAGE_SIZE - 1);  /* 对齐到页起始 */
        erase_init.NbPages     = 1;

        if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK)
        {
            log_error("flash erase failed at 0x%08x", addr);
            HAL_FLASH_Lock();
            return;
        }
    }

    /* 按半字（16-bit）编程 */
    for (uint32_t i = 0; i < len; i += 2)
    {
        uint16_t half_word;
        if (i + 1 < len)
        {
            half_word = data[i] | ((uint16_t)data[i + 1] << 8);
        }
        else
        {
            /* 奇数长度：最后一字节补 0xFF */
            half_word = data[i] | 0xFF00;
        }

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, half_word) != HAL_OK)
        {
            log_error("flash program failed at 0x%08x", addr + i);
            break;
        }
    }

    HAL_FLASH_Lock();
}

typedef struct
{
    uint32_t crc32;
    uint32_t len;
    uint32_t index;
} block_head_t;

typedef struct
{
    uint32_t flash_address[2];
    uint32_t flash_size;
#ifdef CONFIG_NVM_RAM_CACHE
    uint32_t cache;
#endif
    uint32_t curr_index;
    int read_block;
    bool error;
} nvm_block_t;

/* nvm flash layout
|------------------|
    nvm_head_t
|------------------|
      data
|------------------|
*/
static nvm_block_t _nvm_blocks[CONFIG_NVM_PICES_NUM];
static uint32_t block_crc32(nvm_block_t *blocks, int b)
{
    uint32_t address = blocks->flash_address[b];
    block_head_t *flash_header = (block_head_t *)address;
    return crc32_get((uint8_t *)(&flash_header->len), flash_header->len + 8);
}

static int _check_one_block(nvm_block_t *blocks, int b)
{
    block_head_t *head = (block_head_t *)blocks->flash_address[b];
    if (head->crc32 == 0xFFFFFFFF || head->len == 0xFFFFFFFF || head->index == 0xFFFFFFFF || head->len > blocks->flash_size)
    {
        log_debug("data%d is not inited", b);
        return -1;  // head error, need erase
    }
    if (head->crc32 != block_crc32(blocks, b))
    {
        log_warn("data%d crc error 0x%x", b, head->crc32);
        return -1;
    }
    return head->index;
}

static void nvm_init_(nvm_block_t *blocks)
{
    int index0 = _check_one_block(blocks, 0);
    int index1 = _check_one_block(blocks, 1);
    blocks->error = false;
    log_debug("get index -> %d, %d", index0, index1);
    if (index0 > index1)
    {
        blocks->read_block = 0;
        blocks->curr_index = index0;
    }
    else if (index1 > index0)
    {
        blocks->read_block = 1;
        blocks->curr_index = index1;
    }
    else if (index0 != -1)
    {
        blocks->read_block = 0;
        blocks->curr_index = index0;
    }
    else
    {
        blocks->read_block = 0;
        blocks->curr_index = 0;
        blocks->error = true;
    }
#ifdef CONFIG_NVM_RAM_CACHE
    if (!blocks->error)
    {
        memcpy((void *)blocks->cache, (void *)blocks->flash_address[blocks->read_block], blocks->flash_size);
    }
#endif
    log_debug("select data%d:%d", blocks->read_block, blocks->curr_index);
}

void nvm2_init(int nvm_nr, uint32_t flash_addr[], uint32_t flash_size)
{
    if (nvm_nr >= CONFIG_NVM_PICES_NUM)
    {
        log_error("too many nvm nrs");
        return;
    }
    nvm_block_t *blocks = _nvm_blocks + nvm_nr;
    blocks->flash_address[0] = flash_addr[0];
    blocks->flash_address[1] = flash_addr[1];
    log_debug("block %d, flash addr 0x%x[0x%x]", nvm_nr, blocks->flash_address[0], blocks->flash_address[1]);
    blocks->flash_size = flash_size;
#ifdef CONFIG_NVM_RAM_CACHE
    blocks->cache = (uint32_t)os_alloc(flash_size);
#endif
    nvm_init_(blocks);
}

bool nvm_data_avalible(nvm_block_t *blocks)
{
    return !blocks->error;
}

int32_t nvm_read_(nvm_block_t *blocks, uint8_t *buffer, uint32_t len)
{
    if (!nvm_data_avalible(blocks))
    {
        log_warn("read block no data");
        return NVM_ERR_EMPTY;
    }
#ifdef CONFIG_NVM_RAM_CACHE
    uint32_t flash_addr = blocks->cache;  // read from the cache
#else
    int block_nr = blocks->read_block;
    uint32_t flash_addr = blocks->flash_address[block_nr];
#endif
    log_debug("read addr = 0x%x", flash_addr);
    block_head_t *header = (block_head_t *)flash_addr;
    if (len != header->len)
    {
        log_warn("read len error");
        return NVM_ERR_LEN;
    }

    memcpy(buffer, (void *)(flash_addr + sizeof(block_head_t)), len);

    return NVM_ERR_NO;
}

int32_t nvm_read(uint32_t nvm_nr, uint8_t *buffer, uint32_t len)
{
    if (nvm_nr >= CONFIG_NVM_PICES_NUM)
    {
        log_error("read too many nvm nrs");
        return NVM_ERR_EMPTY;
    }
    nvm_block_t *blocks = _nvm_blocks + nvm_nr;
    return nvm_read_(blocks, buffer, len);
}

#ifdef CONFIG_NVM_RAM_CACHE
static uint32_t get_max_index(void)
{
    uint32_t index = 0;
    for (int i = 0; i < CONFIG_NVM_PICES_NUM; i++)
    {
        if (index < _nvm_blocks[i].curr_index)
        {
            index = _nvm_blocks[i].curr_index;
        }
    }
    return index;
}

static void update_all_index(uint32_t skip, uint32_t index)
{
    for (int i = 0; i < CONFIG_NVM_PICES_NUM; i++)
    {
        if (skip == i)
        {
            continue;
        }
        uint32_t cache = _nvm_blocks[i].cache;
        block_head_t *header = (block_head_t *)cache;
        header->index = index;
        header->crc32 = crc32_get((uint8_t *)cache + 4, header->len + 8);
    }
}

static void write_all_blocks(uint32_t block_nr)
{
    for (int i = 0; i < CONFIG_NVM_PICES_NUM; i++)
    {
        nvm_block_t *blocks = _nvm_blocks + i;
        block_head_t *header = (block_head_t *)blocks->cache;
        uint32_t flash_addr = blocks->flash_address[block_nr];
        flash_write_page(flash_addr, (uint8_t *)blocks->cache, header->len + sizeof(header), i == 0 ? true : false);
    }
}
#endif

static void _write_one_block(nvm_block_t *blocks, uint32_t block_nr, uint8_t *buffer, uint32_t len)
{
    uint32_t flash_addr = blocks->flash_address[block_nr];
    block_head_t header;
#ifdef CONFIG_NVM_RAM_CACHE
    header.index = get_max_index() + 1;
#else
    header.index = blocks->curr_index + 1;
#endif
    header.len = len;
    uint32_t crc32 = 0xFFFFFFFF;
    crc32 = crc32_update(crc32, (uint8_t *)&(header.len), 8);
    crc32 = crc32_update(crc32, buffer, len);
    header.crc32 = crc32_finish(crc32);

#ifdef CONFIG_NVM_RAM_CACHE
    memcpy((void *)blocks->cache, &header, sizeof(header));
    memcpy((void *)(blocks->cache + sizeof(header)), buffer, len);
    update_all_index(block_nr, header.index);
    write_all_blocks(block_nr);
#else
    flash_write_page(flash_addr, (uint8_t *)&header, sizeof(header), true);
    flash_write_page(flash_addr + sizeof(header), buffer, len, false);
#endif
    log_debug("write block addr=0x%x b=%d, index=%d", flash_addr, block_nr, header.index);

    block_head_t *r = (block_head_t *)flash_addr;
    log_debug("read -> 0x%x,%d,%d, 0x%x", r->crc32, r->index, r->len, block_crc32(blocks, block_nr));
}

int32_t nvm_write_(nvm_block_t *blocks, uint8_t *buffer, uint32_t len)
{
    int target_block = 1 - blocks->read_block;
    if (len > blocks->flash_size)
    {
        log_warn("write len error");
        return NVM_ERR_LEN;
    }
    _write_one_block(blocks, target_block, buffer, len);
    blocks->read_block = target_block;
    return NVM_ERR_NO;
}

int32_t nvm_write(uint32_t nvm_nr, uint8_t *buffer, uint32_t len)
{
    if (nvm_nr >= CONFIG_NVM_PICES_NUM)
    {
        log_error("write too many nvm nrs");
        return NVM_ERR_EMPTY;
    }
    nvm_block_t *blocks = _nvm_blocks + nvm_nr;
    return nvm_write_(blocks, buffer, len);
}
