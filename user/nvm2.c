/**
 * @file   nvm2.c
 * @brief  NVM 非易失性存储实现
 *
 * 存储机制：
 *   每个分区拥有两个 Flash 页（A / B），交替写入（ping-pong）。
 *   每次写入时选择「非当前读取页」进行擦除并写入新数据，
 *   写入完成后切换读取页。即使写入过程中断电，
 *   另一页仍保留上次的有效数据。
 *
 * Flash 页内布局：
 *   |-------- block_head_t --------|
 *   |  crc32 (4B)                  |  对 len + index + data 的 CRC32
 *   |  len   (4B)                  |  用户数据长度
 *   |  index (4B)                  |  写入序号（递增，用于判断哪页更新）
 *   |------------------------------|
 *   |  data  (len B)               |  用户数据
 *   |------------------------------|
 *
 * 初始化时比较两页的 index，选择较大者作为有效数据页。
 * 若两页都无效（全 0xFF 或 CRC 错误），标记 error = true。
 */
#include <string.h>
#include "crc16.h"
#include "nvm.h"
#include "stm32f1xx_hal.h"

#define LOG_TAG "NVM"
#include "log.h"

#define NVM_MAGIC 0x12349876

/**
 * @brief  底层 Flash 写入（支持先擦除）
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

    /* STM32F1 Flash 最小编程单位为半字（16-bit） */
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

/**
 * @brief  Flash 页头部结构（12 字节）
 */
typedef struct
{
    uint32_t crc32;   /* CRC32 校验值，覆盖 len + index + data */
    uint32_t len;     /* 用户数据长度 */
    uint32_t index;   /* 写入序号，每次写入递增，用于判断哪页更新 */
} block_head_t;

/**
 * @brief  NVM 分区运行时控制块
 */
typedef struct
{
    uint32_t flash_address[2];  /* 两个 Flash 页的起始地址 (A / B) */
    uint32_t flash_size;        /* 单个 Flash 页大小 */
#ifdef CONFIG_NVM_RAM_CACHE
    uint32_t cache;             /* RAM 缓存地址（可选） */
#endif
    uint32_t curr_index;        /* 当前有效数据的写入序号 */
    int read_block;             /* 当前有效数据所在页编号 (0 或 1) */
    bool error;                 /* true = 两页均无有效数据 */
} nvm_block_t;

/* 所有分区的控制块数组 */
static nvm_block_t _nvm_blocks[CONFIG_NVM_PICES_NUM];

/**
 * @brief  计算指定页的 CRC32（覆盖 len + index + data）
 * @param  blocks  分区控制块
 * @param  b       页编号 (0 或 1)
 * @return CRC32 值
 */
static uint32_t block_crc32(nvm_block_t *blocks, int b)
{
    uint32_t address = blocks->flash_address[b];
    block_head_t *flash_header = (block_head_t *)address;
    return crc32_get((uint8_t *)(&flash_header->len), flash_header->len + 8);
}

/**
 * @brief  校验单个 Flash 页的有效性
 * @param  blocks  分区控制块
 * @param  b       页编号 (0 或 1)
 * @retval >=0  有效，返回 index 值
 * @retval -1   无效（未初始化或 CRC 错误）
 */
static int _check_one_block(nvm_block_t *blocks, int b)
{
    block_head_t *head = (block_head_t *)blocks->flash_address[b];
    if (head->crc32 == 0xFFFFFFFF || head->len == 0xFFFFFFFF || head->index == 0xFFFFFFFF || head->len > blocks->flash_size)
    {
        log_debug("data%d is not inited", b);
        return -1;
    }
    if (head->crc32 != block_crc32(blocks, b))
    {
        log_warn("data%d crc error 0x%x", b, head->crc32);
        return -1;
    }
    return head->index;
}

/**
 * @brief  内部初始化：比较两页 index，选择有效页
 *
 * 选择逻辑：
 *   - 两页都有效 → 选 index 更大的
 *   - 只有一页有效 → 选该页
 *   - 都无效 → 标记 error = true，等待首次写入
 */
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
        /* 两页 index 相同且有效，默认选页 0 */
        blocks->read_block = 0;
        blocks->curr_index = index0;
    }
    else
    {
        /* 两页都无效，分区为空 */
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

/**
 * @brief  初始化单个 NVM 分区
 */
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

/**
 * @brief  检查分区是否有有效数据
 */
bool nvm_data_avalible(nvm_block_t *blocks)
{
    return !blocks->error;
}

/**
 * @brief  内部读取实现
 * @param  blocks  分区控制块
 * @param  buffer  接收缓冲区
 * @param  len     期望读取长度（必须与写入时一致）
 */
int32_t nvm_read_(nvm_block_t *blocks, uint8_t *buffer, uint32_t len)
{
    if (!nvm_data_avalible(blocks))
    {
        log_warn("read block no data");
        return NVM_ERR_EMPTY;
    }
#ifdef CONFIG_NVM_RAM_CACHE
    uint32_t flash_addr = blocks->cache;  /* 从 RAM 缓存读取 */
#else
    int block_nr = blocks->read_block;
    uint32_t flash_addr = blocks->flash_address[block_nr];  /* 直接读 Flash */
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

/**
 * @brief  从指定分区读取数据（公共接口）
 */
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
/**
 * @brief  获取所有分区中最大的 index（RAM 缓存模式）
 */
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

/**
 * @brief  同步所有分区的 index 并重新计算 CRC（RAM 缓存模式）
 */
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

/**
 * @brief  将所有分区的缓存写入指定 Flash 页（RAM 缓存模式）
 */
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

/**
 * @brief  向指定页写入一个数据块（header + data）
 *
 * 写入步骤：
 *   1. index 在当前基础上 +1
 *   2. 计算 CRC32（覆盖 len + index + data）
 *   3. 擦除目标页，写入 header，紧接着写入 data
 */
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
    /* 先擦除目标页并写入 header，再追加写入 data（同一页内无需再擦） */
    flash_write_page(flash_addr, (uint8_t *)&header, sizeof(header), true);
    flash_write_page(flash_addr + sizeof(header), buffer, len, false);
#endif
    log_debug("write block addr=0x%x b=%d, index=%d", flash_addr, block_nr, header.index);

    /* 回读验证 */
    block_head_t *r = (block_head_t *)flash_addr;
    log_debug("read -> 0x%x,%d,%d, 0x%x", r->crc32, r->index, r->len, block_crc32(blocks, block_nr));
}

/**
 * @brief  内部写入实现：写入到「非当前读取页」，写完后切换读取页
 */
int32_t nvm_write_(nvm_block_t *blocks, uint8_t *buffer, uint32_t len)
{
    int target_block = 1 - blocks->read_block;  /* 交替写入 */
    if (len > blocks->flash_size)
    {
        log_warn("write len error");
        return NVM_ERR_LEN;
    }
    _write_one_block(blocks, target_block, buffer, len);
    blocks->read_block = target_block;
    return NVM_ERR_NO;
}

/**
 * @brief  向指定分区写入数据（公共接口）
 */
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
