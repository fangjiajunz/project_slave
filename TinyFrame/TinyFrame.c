//---------------------------------------------------------------------------
#include "TinyFrame.h"

#include <stdlib.h>  // - 如果使用动态构造函数则需要 malloc()
//---------------------------------------------------------------------------

// 与 ESP8266 SDK 的兼容性
#ifdef ICACHE_FLASH_ATTR
#define _TF_FN ICACHE_FLASH_ATTR
#else
#define _TF_FN
#endif

// 辅助宏
#define TF_MIN(a, b) ((a) < (b) ? (a) : (b))
#define TF_TRY(func)               \
    do                             \
    {                              \
        if (!(func)) return false; \
    } while (0)

// ID 字段位操作的类型相关掩码
#define TF_ID_MASK (TF_ID)(((TF_ID)1 << (sizeof(TF_ID) * 8 - 1)) - 1)
#define TF_ID_PEERBIT (TF_ID)((TF_ID)1 << ((sizeof(TF_ID) * 8) - 1))

#if !TF_USE_MUTEX
// 非线程安全的锁实现，当用户未提供更好的实现时使用。
// 这不如真正的互斥锁可靠，但会捕获大多数由于不当使用 API 导致的错误。

/** 在组装和发送帧之前获取 TX 接口锁 */
static bool TF_ClaimTx(TinyFrame *tf)
{
    if (tf->soft_lock)
    {
        TF_Error("TF is locked for sending!");
        return false;
    }

    tf->soft_lock = true;
    return true;
}

/** 在组装和发送帧之后释放 TX 接口锁 */
static void TF_ReleaseTx(TinyFrame *tf)
{
    tf->soft_lock = false;
}
#endif

// region 校验和

#if TF_CKSUM_TYPE == TF_CKSUM_NONE

static TF_CKSUM TF_CksumStart(void)
{
    return 0;
}

static TF_CKSUM TF_CksumAdd(TF_CKSUM cksum, uint8_t byte)
{
    return cksum;
}

static TF_CKSUM TF_CksumEnd(TF_CKSUM cksum)
{
    return cksum;
}

#elif TF_CKSUM_TYPE == TF_CKSUM_XOR

static TF_CKSUM TF_CksumStart(void)
{
    return 0;
}

static TF_CKSUM TF_CksumAdd(TF_CKSUM cksum, uint8_t byte)
{
    return cksum ^ byte;
}

static TF_CKSUM TF_CksumEnd(TF_CKSUM cksum)
{
    return (TF_CKSUM)~cksum;
}

#elif TF_CKSUM_TYPE == TF_CKSUM_CRC8

static inline uint8_t crc8_bits(uint8_t data)
{
    uint8_t crc = 0;
    if (data & 1) crc ^= 0x5e;
    if (data & 2) crc ^= 0xbc;
    if (data & 4) crc ^= 0x61;
    if (data & 8) crc ^= 0xc2;
    if (data & 0x10) crc ^= 0x9d;
    if (data & 0x20) crc ^= 0x23;
    if (data & 0x40) crc ^= 0x46;
    if (data & 0x80) crc ^= 0x8c;
    return crc;
}

static TF_CKSUM TF_CksumStart(void)
{
    return 0;
}

static TF_CKSUM TF_CksumAdd(TF_CKSUM cksum, uint8_t byte)
{
    return crc8_bits(byte ^ cksum);
}

static TF_CKSUM TF_CksumEnd(TF_CKSUM cksum)
{
    return cksum;
}

#elif TF_CKSUM_TYPE == TF_CKSUM_CRC16

// TODO 尝试用算法替代
/** CRC-16 的 CRC 表。多项式为 0x8005 (x^16 + x^15 + x^2 + 1) */
static const uint16_t crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040};

static TF_CKSUM TF_CksumStart(void)
{
    return 0;
}

static TF_CKSUM TF_CksumAdd(TF_CKSUM cksum, uint8_t byte)
{
    return (cksum >> 8) ^ crc16_table[(cksum ^ byte) & 0xff];
}

static TF_CKSUM TF_CksumEnd(TF_CKSUM cksum)
{
    return cksum;
}

#elif TF_CKSUM_TYPE == TF_CKSUM_CRC32

// TODO 尝试用算法替代
static const uint32_t crc32_table[] = {/* CRC 多项式 0xedb88320 */
                                       0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
                                       0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
                                       0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
                                       0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
                                       0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
                                       0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
                                       0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
                                       0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
                                       0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
                                       0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
                                       0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
                                       0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
                                       0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
                                       0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
                                       0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
                                       0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
                                       0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
                                       0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
                                       0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
                                       0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
                                       0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
                                       0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
                                       0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
                                       0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
                                       0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
                                       0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
                                       0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
                                       0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
                                       0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
                                       0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
                                       0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
                                       0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
                                       0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
                                       0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
                                       0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
                                       0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
                                       0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
                                       0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
                                       0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
                                       0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
                                       0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
                                       0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
                                       0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

static TF_CKSUM TF_CksumStart(void)
{
    return (TF_CKSUM)0xFFFFFFFF;
}

static TF_CKSUM TF_CksumAdd(TF_CKSUM cksum, uint8_t byte)
{
    return crc32_table[((cksum) ^ ((uint8_t)byte)) & 0xff] ^ ((cksum) >> 8);
}

static TF_CKSUM TF_CksumEnd(TF_CKSUM cksum)
{
    return (TF_CKSUM)~cksum;
}

#endif

#define CKSUM_RESET(cksum)         \
    do                             \
    {                              \
        (cksum) = TF_CksumStart(); \
    } while (0)
#define CKSUM_ADD(cksum, byte)                  \
    do                                          \
    {                                           \
        (cksum) = TF_CksumAdd((cksum), (byte)); \
    } while (0)
#define CKSUM_FINALIZE(cksum)           \
    do                                  \
    {                                   \
        (cksum) = TF_CksumEnd((cksum)); \
    } while (0)

// endregion

// region 初始化

/** 使用用户分配的缓冲区初始化 */
bool _TF_FN TF_InitStatic(TinyFrame *tf, TF_Peer peer_bit)
{
    if (tf == NULL)
    {
        TF_Error("TF_InitStatic() false，tf null ");
        return false;
    }

    // 清零，保留用户配置
    uint32_t usertag = tf->usertag;
    void *userdata = tf->userdata;

    memset(tf, 0, sizeof(struct TinyFrame_));

    tf->usertag = usertag;
    tf->userdata = userdata;

    tf->peer_bit = peer_bit;
    return true;
}

/** 使用 malloc 初始化 */
TinyFrame *_TF_FN TF_Init(TF_Peer peer_bit)
{
    TinyFrame *tf = malloc(sizeof(TinyFrame));
    if (!tf)
    {
        TF_Error("TF_Init() falied，memeny not en ");
        return NULL;
    }

    TF_InitStatic(tf, peer_bit);
    return tf;
}

/** 释放结构体 */
void TF_DeInit(TinyFrame *tf)
{
    if (tf == NULL) return;
    free(tf);
}

// endregion 初始化

// region 监听器

/** 将 ID 监听器的超时重置为原始值 */
static inline void _TF_FN renew_id_listener(struct TF_IdListener_ *lst)
{
    lst->timeout = lst->timeout_max;
}

/** 通知回调关于 ID 监听器的消亡 & 让它释放 userdata 中的任何资源 */
static void _TF_FN cleanup_id_listener(TinyFrame *tf, TF_COUNT i, struct TF_IdListener_ *lst)
{
    TF_Msg msg;
    if (lst->fn == NULL) return;

    // 让用户清理他们的数据 - 仅当不为 NULL 时
    if (lst->userdata != NULL || lst->userdata2 != NULL)
    {
        msg.userdata = lst->userdata;
        msg.userdata2 = lst->userdata2;
        msg.data = NULL;    // 这是一个信号，表示监听器应该清理
        lst->fn(tf, &msg);  // 这里忽略返回值 - 使用 TF_STAY 或 TF_CLOSE
    }

    lst->fn = NULL;  // 丢弃监听器
    lst->fn_timeout = NULL;

    if (i == tf->count_id_lst - 1)
    {
        tf->count_id_lst--;
    }
}

/** 清理类型监听器 */
static inline void _TF_FN cleanup_type_listener(TinyFrame *tf, TF_COUNT i, struct TF_TypeListener_ *lst)
{
    lst->fn = NULL;  // 丢弃监听器
    if (i == tf->count_type_lst - 1)
    {
        tf->count_type_lst--;
    }
}

/** 清理通用监听器 */
static inline void _TF_FN cleanup_generic_listener(TinyFrame *tf, TF_COUNT i, struct TF_GenericListener_ *lst)
{
    lst->fn = NULL;  // 丢弃监听器
    if (i == tf->count_generic_lst - 1)
    {
        tf->count_generic_lst--;
    }
}

/** 添加新的 ID 监听器。成功返回 1。 */
bool _TF_FN TF_AddIdListener(TinyFrame *tf, TF_Msg *msg, TF_Listener cb, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    TF_COUNT i;
    struct TF_IdListener_ *lst;
    for (i = 0; i < TF_MAX_ID_LST; i++)
    {
        lst = &tf->id_listeners[i];
        // 测试空槽位
        if (lst->fn == NULL)
        {
            lst->fn = cb;
            lst->fn_timeout = ftimeout;
            lst->id = msg->frame_id;
            lst->userdata = msg->userdata;
            lst->userdata2 = msg->userdata2;
            lst->timeout_max = lst->timeout = timeout;
            if (i >= tf->count_id_lst)
            {
                tf->count_id_lst = (TF_COUNT)(i + 1);
            }
            return true;
        }
    }

    TF_Error("add ID AddIdListener failed");
    return false;
}

/** 添加新的类型监听器。成功返回 1。 */
bool _TF_FN TF_AddTypeListener(TinyFrame *tf, TF_TYPE frame_type, TF_Listener cb)
{
    TF_COUNT i;
    struct TF_TypeListener_ *lst;
    for (i = 0; i < TF_MAX_TYPE_LST; i++)
    {
        lst = &tf->type_listeners[i];
        // 测试空槽位
        if (lst->fn == NULL)
        {
            lst->fn = cb;
            lst->type = frame_type;
            if (i >= tf->count_type_lst)
            {
                tf->count_type_lst = (TF_COUNT)(i + 1);
            }
            return true;
        }
    }

    TF_Error("AddTypeListener failed");
    return false;
}

/** 添加新的通用监听器。成功返回 1。 */
bool _TF_FN TF_AddGenericListener(TinyFrame *tf, TF_Listener cb)
{
    TF_COUNT i;
    struct TF_GenericListener_ *lst;
    for (i = 0; i < TF_MAX_GEN_LST; i++)
    {
        lst = &tf->generic_listeners[i];
        // 测试空槽位
        if (lst->fn == NULL)
        {
            lst->fn = cb;
            if (i >= tf->count_generic_lst)
            {
                tf->count_generic_lst = (TF_COUNT)(i + 1);
            }
            return true;
        }
    }

    TF_Error("TF_AddGenericListener falied");
    return false;
}

/** 通过帧 ID 移除 ID 监听器。成功返回 1。 */
bool _TF_FN TF_RemoveIdListener(TinyFrame *tf, TF_ID frame_id)
{
    TF_COUNT i;
    struct TF_IdListener_ *lst;
    for (i = 0; i < tf->count_id_lst; i++)
    {
        lst = &tf->id_listeners[i];
        // 测试是否存活且匹配
        if (lst->fn != NULL && lst->id == frame_id)
        {
            cleanup_id_listener(tf, i, lst);
            return true;
        }
    }

    TF_Error("Listener ID %d not found  ", (int)frame_id);

    return false;
}

/** 通过类型移除类型监听器。成功返回 1。 */
bool _TF_FN TF_RemoveTypeListener(TinyFrame *tf, TF_TYPE type)
{
    TF_COUNT i;
    struct TF_TypeListener_ *lst;
    for (i = 0; i < tf->count_type_lst; i++)
    {
        lst = &tf->type_listeners[i];
        // 测试是否存活且匹配
        if (lst->fn != NULL && lst->type == type)
        {
            cleanup_type_listener(tf, i, lst);
            return true;
        }
    }

   TF_Error("Failed to remove type listener: type %d not found", (int)type);

    return false;
}

/** 通过函数指针移除通用监听器。成功返回 1。 */
bool _TF_FN TF_RemoveGenericListener(TinyFrame *tf, TF_Listener cb)
{
    TF_COUNT i;
    struct TF_GenericListener_ *lst;
    for (i = 0; i < tf->count_generic_lst; i++)
    {
        lst = &tf->generic_listeners[i];
        // 测试是否存活且匹配
        if (lst->fn == cb)
        {
            cleanup_generic_listener(tf, i, lst);
            return true;
        }
    }

    TF_Error("Generic listener to be removed was not found");

    return false;
}

/** 处理刚刚被解析器收集和验证的消息 */
static void _TF_FN TF_HandleReceivedMessage(TinyFrame *tf)
{
    TF_COUNT i;
    struct TF_IdListener_ *ilst;
    struct TF_TypeListener_ *tlst;
    struct TF_GenericListener_ *glst;
    TF_Result res;

    // 准备消息对象
    TF_Msg msg;
    TF_ClearMsg(&msg);
    msg.frame_id = tf->id;
    msg.is_response = false;
    msg.type = tf->type;
    msg.data = tf->data;
    msg.len = tf->len;

    // 任何监听器都可以消费消息，或让其他人处理它。

    // 循环上界是当前使用的最高槽位索引
    // （或接近它，取决于监听器移除的顺序）。

    // 首先是 ID 监听器
    for (i = 0; i < tf->count_id_lst; i++)
    {
        ilst = &tf->id_listeners[i];

        if (ilst->fn && ilst->id == msg.frame_id)
        {
            msg.userdata = ilst->userdata;  // 将 userdata 指针传递给回调
            msg.userdata2 = ilst->userdata2;
            res = ilst->fn(tf, &msg);
            ilst->userdata = msg.userdata;    // 放回（可能已更改指针或设为 NULL）
            ilst->userdata2 = msg.userdata2;  // 放回（可能已更改指针或设为 NULL）

            if (res != TF_NEXT)
            {
                // 如果是 TF_CLOSE，我们假设用户已经清理了 userdata
                if (res == TF_RENEW)
                {
                    renew_id_listener(ilst);
                }
                else if (res == TF_CLOSE)
                {
                    // 将 userdata 设为 NULL 以避免调用用户进行清理
                    ilst->userdata = NULL;
                    ilst->userdata2 = NULL;
                    cleanup_id_listener(tf, i, ilst);
                }
                return;
            }
        }
    }
    // 为后续不使用 userdata 的监听器清理（这避免了
    // 返回 TF_NEXT 的 ID 监听器的数据泄漏到类型和通用监听器）
    msg.userdata = NULL;
    msg.userdata2 = NULL;

    // 类型监听器
    for (i = 0; i < tf->count_type_lst; i++)
    {
        tlst = &tf->type_listeners[i];

        if (tlst->fn && tlst->type == msg.type)
        {
            res = tlst->fn(tf, &msg);

            if (res != TF_NEXT)
            {
                // 类型监听器没有 userdata。
                // TF_RENEW 在这里没有意义，因为类型监听器不会过期 = 等同于 TF_STAY

                if (res == TF_CLOSE)
                {
                    cleanup_type_listener(tf, i, tlst);
                }
                return;
            }
        }
    }

    // 通用监听器
    for (i = 0; i < tf->count_generic_lst; i++)
    {
        glst = &tf->generic_listeners[i];

        if (glst->fn)
        {
            res = glst->fn(tf, &msg);

            if (res != TF_NEXT)
            {
                // 通用监听器没有 userdata。
                // TF_RENEW 在这里没有意义，因为通用监听器不会过期 = 等同于 TF_STAY

                // 注意：不期望用户会有多个通用监听器，或者
                // 真的会移除它们。它们最有用的是作为默认回调，
                // 当没有其他监听器处理消息时。

                if (res == TF_CLOSE)
                {
                    cleanup_generic_listener(tf, i, glst);
                }
                return;
            }
        }
    }

    TF_Error("Unhandled message, type %d", (int)msg.type);

}

/** 外部重新续期 ID 监听器 */
bool _TF_FN TF_RenewIdListener(TinyFrame *tf, TF_ID id)
{
    TF_COUNT i;
    struct TF_IdListener_ *lst;
    for (i = 0; i < tf->count_id_lst; i++)
    {
        lst = &tf->id_listeners[i];
        // 测试是否存活且匹配
        if (lst->fn != NULL && lst->id == id)
        {
            renew_id_listener(lst);
            return true;
        }
    }

    TF_Error("Failed to renew listener: ID %d not found", (int)id);

    return false;
}

// endregion 监听器

// region 解析器

/** 处理接收到的字节缓冲区 */
void _TF_FN TF_Accept(TinyFrame *tf, const uint8_t *buffer, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++)
    {
        TF_AcceptChar(tf, buffer[i]);
    }
}

/** 重置解析器的内部状态。 */
void _TF_FN TF_ResetParser(TinyFrame *tf)
{
    tf->state = TFState_SOF;
    // 当接收到第一个字节时，解析器会进行更多初始化
}

/** 收到 SOF - 准备接收帧 */
static void _TF_FN pars_begin_frame(TinyFrame *tf)
{
    // 重置状态变量
    CKSUM_RESET(tf->cksum);
#if TF_USE_SOF_BYTE
    CKSUM_ADD(tf->cksum, TF_SOF_BYTE);
#endif

    tf->discard_data = false;

    // 进入 ID 状态
    tf->state = TFState_ID;
    tf->rxi = 0;
}

/** 处理接收到的字符 - 这是主状态机 */
void _TF_FN TF_AcceptChar(TinyFrame *tf, unsigned char c)
{
    // 解析器超时 - 清除
    if (tf->parser_timeout_ticks >= TF_PARSER_TIMEOUT_TICKS)
    {
        if (tf->state != TFState_SOF)
        {
            TF_ResetParser(tf);
            TF_Error("Parser timeout");
        }
    }
    tf->parser_timeout_ticks = 0;

// DRY 代码片段 - 从输入流逐字节收集多字节数字
// 这有点不优雅，但使代码更易读。它像 if() 一样使用，
// 主体仅在整个数字（数据类型为 'type'）被接收并存储到 'dest' 后运行
#define COLLECT_NUMBER(dest, type)    \
    dest = (type)(((dest) << 8) | c); \
    if (++tf->rxi == sizeof(type))

#if !TF_USE_SOF_BYTE
    if (tf->state == TFState_SOF)
    {
        pars_begin_frame(tf);
    }
#endif

    //@formatter:off
    switch (tf->state)
    {
        case TFState_SOF:
            if (c == TF_SOF_BYTE)
            {
                pars_begin_frame(tf);
            }
            break;

        case TFState_ID:
            CKSUM_ADD(tf->cksum, c);
            COLLECT_NUMBER(tf->id, TF_ID)
            {
                // 进入 LEN 状态
                tf->state = TFState_LEN;
                tf->rxi = 0;
            }
            break;

        case TFState_LEN:
            CKSUM_ADD(tf->cksum, c);
            COLLECT_NUMBER(tf->len, TF_LEN)
            {
                // 进入 TYPE 状态
                tf->state = TFState_TYPE;
                tf->rxi = 0;
            }
            break;

        case TFState_TYPE:
            CKSUM_ADD(tf->cksum, c);
            COLLECT_NUMBER(tf->type, TF_TYPE)
            {
#if TF_CKSUM_TYPE == TF_CKSUM_NONE
                tf->state = TFState_DATA;
                tf->rxi = 0;
#else
                // 进入 HEAD_CKSUM 状态
                tf->state = TFState_HEAD_CKSUM;
                tf->rxi = 0;
                tf->ref_cksum = 0;
#endif
            }
            break;

        case TFState_HEAD_CKSUM:
            COLLECT_NUMBER(tf->ref_cksum, TF_CKSUM)
            {
                // 检查帧头校验和与计算值
                CKSUM_FINALIZE(tf->cksum);

                if (tf->cksum != tf->ref_cksum)
                {
                    TF_Error("Header checksum mismatch");
                    TF_ResetParser(tf);
                    break;
                }

                if (tf->len == 0)
                {
                    // 如果消息没有正文，我们完成了。
                    TF_HandleReceivedMessage(tf);
                    TF_ResetParser(tf);
                    break;
                }

                // 进入 DATA 状态
                tf->state = TFState_DATA;
                tf->rxi = 0;

                CKSUM_RESET(tf->cksum);  // 开始收集负载

                if (tf->len > TF_MAX_PAYLOAD_RX)
                {
                    TF_Error("Payload too long: %d", (int)tf->len);
                    // 错误 - 帧太长。消费但不存储。
                    tf->discard_data = true;
                }
            }
            break;

        case TFState_DATA:
            if (tf->discard_data)
            {
                tf->rxi++;
            }
            else
            {
                CKSUM_ADD(tf->cksum, c);
                tf->data[tf->rxi++] = c;
            }

            if (tf->rxi == tf->len)
            {
#if TF_CKSUM_TYPE == TF_CKSUM_NONE
                // 全部完成
                TF_HandleReceivedMessage(tf);
                TF_ResetParser(tf);
#else
                // 进入 DATA_CKSUM 状态
                tf->state = TFState_DATA_CKSUM;
                tf->rxi = 0;
                tf->ref_cksum = 0;
#endif
            }
            break;

        case TFState_DATA_CKSUM:
            COLLECT_NUMBER(tf->ref_cksum, TF_CKSUM)
            {
                // 检查帧头校验和与计算值
                CKSUM_FINALIZE(tf->cksum);
                if (!tf->discard_data)
                {
                    if (tf->cksum == tf->ref_cksum)
                    {
                        TF_HandleReceivedMessage(tf);
                    }
                    else
                    {
                        TF_Error("Data checksum mismatch");
                    }
                }

                TF_ResetParser(tf);
            }
            break;
    }
    //@formatter:on
}

// endregion 解析器

// region 组装和发送

// 用于 Compose 函数的辅助宏
// 使用变量: si - 有符号整数, b - 字节, outbuff - 目标缓冲区, pos - 缓冲区中的字节计数

/**
 * 将数字写入输出缓冲区。
 *
 * @param type - 数据类型
 * @param num - 要写入的数字
 * @param xtra - 每个字节后运行的额外回调，'b' 现在包含该字节。
 */
#define WRITENUM_BASE(type, num, xtra)           \
    for (si = sizeof(type) - 1; si >= 0; si--)   \
    {                                            \
        b = (uint8_t)((num) >> (si * 8) & 0xFF); \
        outbuff[pos++] = b;                      \
        xtra;                                    \
    }

/**
 * 什么都不做
 */
#define _NOOP()

/**
 * 写入数字但不将其字节添加到校验和
 *
 * @param type - 数据类型
 * @param num - 要写入的数字
 */
#define WRITENUM(type, num) WRITENUM_BASE(type, num, _NOOP())

/**
 * 写入数字并将其字节添加到校验和
 *
 * @param type - 数据类型
 * @param num - 要写入的数字
 */
#define WRITENUM_CKSUM(type, num) WRITENUM_BASE(type, num, CKSUM_ADD(cksum, b))

/**
 * 组装帧（由 TF_Send 和 TF_Respond 内部使用）。
 * 帧可以使用 TF_WriteImpl() 发送，或由 TF_Accept() 接收
 *
 * @param outbuff - 存储结果的缓冲区
 * @param msg - 写入缓冲区的消息
 * @return outbuff 中帧使用的字节数，失败返回 0
 */
static inline uint32_t _TF_FN TF_ComposeHead(TinyFrame *tf, uint8_t *outbuff, TF_Msg *msg)
{
    int8_t si = 0;  // 有符号小整数
    uint8_t b = 0;
    TF_ID id = 0;
    TF_CKSUM cksum = 0;
    uint32_t pos = 0;

    (void)cksum;  // 如果禁用校验和则抑制 "unused" 警告

    CKSUM_RESET(cksum);

    // 生成 ID
    if (msg->is_response)
    {
        id = msg->frame_id;
    }
    else
    {
        id = (TF_ID)(tf->next_id++ & TF_ID_MASK);
        if (tf->peer_bit)
        {
            id |= TF_ID_PEERBIT;
        }
    }

    msg->frame_id = id;  // 将解析后的 ID 放入消息对象以供后续使用

    // --- 开始 ---
    CKSUM_RESET(cksum);

#if TF_USE_SOF_BYTE
    outbuff[pos++] = TF_SOF_BYTE;
    CKSUM_ADD(cksum, TF_SOF_BYTE);
#endif

    WRITENUM_CKSUM(TF_ID, id);
    WRITENUM_CKSUM(TF_LEN, msg->len);
    WRITENUM_CKSUM(TF_TYPE, msg->type);

#if TF_CKSUM_TYPE != TF_CKSUM_NONE
    CKSUM_FINALIZE(cksum);
    WRITENUM(TF_CKSUM, cksum);
#endif

    return pos;
}

/**
 * 组装帧（由 TF_Send 和 TF_Respond 内部使用）。
 * 帧可以使用 TF_WriteImpl() 发送，或由 TF_Accept() 接收
 *
 * @param outbuff - 存储结果的缓冲区
 * @param data - 数据缓冲区
 * @param data_len - 数据缓冲区长度
 * @param cksum - 校验和变量，用于所有对 TF_ComposeBody 的调用。第一次使用前必须重置！(CKSUM_RESET(cksum);)
 * @return outbuff 中使用的字节数
 */
static inline uint32_t _TF_FN TF_ComposeBody(uint8_t *outbuff,
                                             const uint8_t *data, TF_LEN data_len,
                                             TF_CKSUM *cksum)
{
    TF_LEN i = 0;
    uint8_t b = 0;
    uint32_t pos = 0;

    for (i = 0; i < data_len; i++)
    {
        b = data[i];
        outbuff[pos++] = b;
        CKSUM_ADD(*cksum, b);
    }

    return pos;
}

/**
 * 完成帧
 *
 * @param outbuff - 存储结果的缓冲区
 * @param cksum - 用于正文的校验和变量
 * @return outbuff 中使用的字节数
 */
static inline uint32_t _TF_FN TF_ComposeTail(uint8_t *outbuff, TF_CKSUM *cksum)
{
    int8_t si = 0;  // 有符号小整数
    uint8_t b = 0;
    uint32_t pos = 0;

#if TF_CKSUM_TYPE != TF_CKSUM_NONE
    CKSUM_FINALIZE(*cksum);
    WRITENUM(TF_CKSUM, *cksum);
#endif
    return pos;
}

/**
 * 开始构建和发送帧
 *
 * @param tf - 实例
 * @param msg - 要发送的消息
 * @param listener - 响应监听器或 NULL
 * @param ftimeout - 超时回调
 * @param timeout - 监听器超时 ticks，0 = 无限
 * @return 成功（互斥锁已获取且监听器已添加，如果有的话）
 */
static bool _TF_FN TF_SendFrame_Begin(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    TF_TRY(TF_ClaimTx(tf));

    tf->tx_pos = (uint32_t)TF_ComposeHead(tf, tf->sendbuf, msg);  // 如果不是响应，帧 ID 在这里递增
    tf->tx_len = msg->len;

    if (listener)
    {
        if (!TF_AddIdListener(tf, msg, listener, ftimeout, timeout))
        {
            TF_ReleaseTx(tf);
            return false;
        }
    }

    CKSUM_RESET(tf->tx_cksum);
    return true;
}

/**
 * 构建和发送帧正文的一部分（或全部）。
 * 注意：这不检查总长度是否与帧头中指定的长度一致
 *
 * @param tf - 实例
 * @param buff - 要写入的字节
 * @param length - 数量
 */
static void _TF_FN TF_SendFrame_Chunk(TinyFrame *tf, const uint8_t *buff, uint32_t length)
{
    uint32_t remain;
    uint32_t chunk;
    uint32_t sent = 0;

    remain = length;
    while (remain > 0)
    {
        // 写入能放入 tx 缓冲区的部分
        chunk = TF_MIN(TF_SENDBUF_LEN - tf->tx_pos, remain);
        tf->tx_pos += TF_ComposeBody(tf->sendbuf + tf->tx_pos, buff + sent, (TF_LEN)chunk, &tf->tx_cksum);
        remain -= chunk;
        sent += chunk;

        // 如果缓冲区已满则刷新
        if (tf->tx_pos == TF_SENDBUF_LEN)
        {
            TF_WriteImpl(tf, (const uint8_t *)tf->sendbuf, tf->tx_pos);
            tf->tx_pos = 0;
        }
    }
}

/**
 * 结束多部分帧。这发送校验和并释放互斥锁。
 *
 * @param tf - 实例
 */
static void _TF_FN TF_SendFrame_End(TinyFrame *tf)
{
    // 仅当消息有正文时才计算校验和
    if (tf->tx_len > 0)
    {
        // 如果校验和无法放入缓冲区则刷新
        if (TF_SENDBUF_LEN - tf->tx_pos < sizeof(TF_CKSUM))
        {
            TF_WriteImpl(tf, (const uint8_t *)tf->sendbuf, tf->tx_pos);
            tf->tx_pos = 0;
        }

        // 添加校验和，刷新剩余要发送的数据
        tf->tx_pos += TF_ComposeTail(tf->sendbuf + tf->tx_pos, &tf->tx_cksum);
    }

    TF_WriteImpl(tf, (const uint8_t *)tf->sendbuf, tf->tx_pos);
    TF_ReleaseTx(tf);
}

/**
 * 发送消息
 *
 * @param tf - 实例
 * @param msg - 消息对象
 * @param listener - ID 监听器，或 NULL
 * @param ftimeout - 超时回调
 * @param timeout - 监听器超时，0 表示无
 * @return 如果发送成功返回 true
 */
static bool _TF_FN TF_SendFrame(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    TF_TRY(TF_SendFrame_Begin(tf, msg, listener, ftimeout, timeout));
    if (msg->len == 0 || msg->data != NULL)
    {
        // 仅当我们不是开始多部分帧时才发送负载和校验和。
        // 多部分帧通过将 data 字段传递为 NULL 并设置长度来标识。
        // 用户然后需要手动调用这些函数
        TF_SendFrame_Chunk(tf, msg->data, msg->len);
        TF_SendFrame_End(tf);
    }
    return true;
}

// endregion 组装和发送

// region 发送 API 函数

/** 不带监听器发送 */
bool _TF_FN TF_Send(TinyFrame *tf, TF_Msg *msg)
{
    return TF_SendFrame(tf, msg, NULL, NULL, 0);
}

/** 不带监听器和结构体发送 */
bool _TF_FN TF_SendSimple(TinyFrame *tf, TF_TYPE type, const uint8_t *data, TF_LEN len)
{
    TF_Msg msg;
    TF_ClearMsg(&msg);
    msg.type = type;
    msg.data = data;
    msg.len = len;
    return TF_Send(tf, &msg);
}

/** 带监听器等待响应发送，不带结构体 */
bool _TF_FN TF_QuerySimple(TinyFrame *tf, TF_TYPE type, const uint8_t *data, TF_LEN len, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    TF_Msg msg;
    TF_ClearMsg(&msg);
    msg.type = type;
    msg.data = data;
    msg.len = len;
    return TF_SendFrame(tf, &msg, listener, ftimeout, timeout);
}

/** 带监听器等待响应发送 */
bool _TF_FN TF_Query(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    return TF_SendFrame(tf, msg, listener, ftimeout, timeout);
}

/** 类似 TF_Send，但使用显式帧 ID（在 msg 对象中设置），用于响应 */
bool _TF_FN TF_Respond(TinyFrame *tf, TF_Msg *msg)
{
    msg->is_response = true;
    return TF_Send(tf, msg);
}

// endregion 发送 API 函数

// region 发送 API 函数 - 多部分

bool _TF_FN TF_Send_Multipart(TinyFrame *tf, TF_Msg *msg)
{
    msg->data = NULL;
    return TF_Send(tf, msg);
}

bool _TF_FN TF_SendSimple_Multipart(TinyFrame *tf, TF_TYPE type, TF_LEN len)
{
    return TF_SendSimple(tf, type, NULL, len);
}

bool _TF_FN TF_QuerySimple_Multipart(TinyFrame *tf, TF_TYPE type, TF_LEN len, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    return TF_QuerySimple(tf, type, NULL, len, listener, ftimeout, timeout);
}

bool _TF_FN TF_Query_Multipart(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout)
{
    msg->data = NULL;
    return TF_Query(tf, msg, listener, ftimeout, timeout);
}

void _TF_FN TF_Respond_Multipart(TinyFrame *tf, TF_Msg *msg)
{
    msg->data = NULL;
    TF_Respond(tf, msg);
}

void _TF_FN TF_Multipart_Payload(TinyFrame *tf, const uint8_t *buff, uint32_t length)
{
    TF_SendFrame_Chunk(tf, buff, length);
}

void _TF_FN TF_Multipart_Close(TinyFrame *tf)
{
    TF_SendFrame_End(tf);
}

// endregion 发送 API 函数 - 多部分

/** 时基钩子 - 用于超时 */
void _TF_FN TF_Tick(TinyFrame *tf)
{
    TF_COUNT i;
    struct TF_IdListener_ *lst;

    // 增加解析器超时（超时在接收下一个字节时处理）
    if (tf->parser_timeout_ticks < TF_PARSER_TIMEOUT_TICKS)
    {
        tf->parser_timeout_ticks++;
    }

    // 递减并使 ID 监听器过期
    for (i = 0; i < tf->count_id_lst; i++)
    {
        lst = &tf->id_listeners[i];
        if (!lst->fn || lst->timeout == 0) continue;
        // 倒计时...
        if (--lst->timeout == 0)
        {
            TF_Error("Listener ID %d has expired", (int)lst->id);

            if (lst->fn_timeout != NULL)
            {
                lst->fn_timeout(tf);  // 执行超时函数
            }
            // 监听器已过期
            cleanup_id_listener(tf, i, lst);
        }
    }
}
