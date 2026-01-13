#ifndef TinyFrameH
#define TinyFrameH

/**
 * TinyFrame 协议库
 *
 * (c) Ondřej Hruška 2017-2018, MIT License
 * 无责任/担保，可免费用于任何用途，必须保留此声明和许可证
 *
 * 上游地址: https://github.com/MightyPork/TinyFrame
 */

#define TF_VERSION "2.3.0"

//---------------------------------------------------------------------------
#include <stdbool.h>  // 为了 bool
#include <stddef.h>   // 为了 NULL
#include <stdint.h>   // 为了 uint8_t 等
#include <string.h>   // 为了 memset()
//---------------------------------------------------------------------------

// 校验和类型 (0 = 无, 8 = ~XOR, 16 = CRC16 0x8005, 32 = CRC32)
#define TF_CKSUM_NONE 0      // 无校验和
#define TF_CKSUM_XOR 8       // 所有负载字节的反转异或 (~XOR)
#define TF_CKSUM_CRC8 9      // Dallas/Maxim CRC8 (1-wire总线用)
#define TF_CKSUM_CRC16 16    // CRC16 多项式 0x8005 (x^16 + x^15 + x^2 + 1)
#define TF_CKSUM_CRC32 32    // CRC32 多项式 0xedb88320
#define TF_CKSUM_CUSTOM8 1   // 自定义 8位 校验和
#define TF_CKSUM_CUSTOM16 2  // 自定义 16位 校验和
#define TF_CKSUM_CUSTOM32 3  // 自定义 32位 校验和

#include "TF_Config.h"

// region 解析数据类型 (Resolve data types)

#if TF_LEN_BYTES == 1
typedef uint8_t TF_LEN;
#elif TF_LEN_BYTES == 2
typedef uint16_t TF_LEN;
#elif TF_LEN_BYTES == 4
typedef uint32_t TF_LEN;
#else
#error Bad value of TF_LEN_BYTES, must be 1, 2 or 4
#endif

#if TF_TYPE_BYTES == 1
typedef uint8_t TF_TYPE;
#elif TF_TYPE_BYTES == 2
typedef uint16_t TF_TYPE;
#elif TF_TYPE_BYTES == 4
typedef uint32_t TF_TYPE;
#else
#error Bad value of TF_TYPE_BYTES, must be 1, 2 or 4
#endif

#if TF_ID_BYTES == 1
typedef uint8_t TF_ID;
#elif TF_ID_BYTES == 2
typedef uint16_t TF_ID;
#elif TF_ID_BYTES == 4
typedef uint32_t TF_ID;
#else
#error Bad value of TF_ID_BYTES, must be 1, 2 or 4
#endif

#if (TF_CKSUM_TYPE == TF_CKSUM_XOR) || (TF_CKSUM_TYPE == TF_CKSUM_NONE) || (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM8) || (TF_CKSUM_TYPE == TF_CKSUM_CRC8)
// ~XOR (如果是 0, 仍然使用 1 字节占位 - 但不会被使用)
typedef uint8_t TF_CKSUM;
#elif (TF_CKSUM_TYPE == TF_CKSUM_CRC16) || (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM16)
// CRC16
typedef uint16_t TF_CKSUM;
#elif (TF_CKSUM_TYPE == TF_CKSUM_CRC32) || (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM32)
// CRC32
typedef uint32_t TF_CKSUM;
#else
#error Bad value for TF_CKSUM_TYPE
#endif

// endregion

//---------------------------------------------------------------------------

/** 对端位枚举 (用于初始化) */
typedef enum
{
    TF_SLAVE = 0,   // 从机
    TF_MASTER = 1,  // 主机
} TF_Peer;

/** 监听器的返回值 */
typedef enum
{
    TF_NEXT = 0,   //!< 未处理，让其他监听器处理
    TF_STAY = 1,   //!< 已处理，保持监听器（不移除）
    TF_RENEW = 2,  //!< 已处理，保持并更新（重置）超时时间 - 仅在监听器设置了超时时有用
    TF_CLOSE = 3,  //!< 已处理，移除此监听器
} TF_Result;

/** 用于发送/接收消息的数据结构 */
typedef struct TF_Msg_
{
    TF_ID frame_id;    //!< 消息 ID
    bool is_response;  //!< 内部标志，在使用 Respond 函数时设置。frame_id 保持不变。
    TF_TYPE type;      //!< 接收到的或发送的消息类型

    /**
     * 接收数据的缓冲区，或要发送的数据。
     *
     * - 如果在 ID 监听器中 (data == NULL)，这意味着监听器超时了，
     * 用户应该释放任何 userdata 并采取其他适当的措施。
     *
     * - 如果在发送帧时 (data == NULL) 且 length 不为零，这将启动一个多部分帧（Multi-part frame）。
     * 随后的调用必须发送负载并关闭帧。
     */
    const uint8_t *data;
    TF_LEN len;  //!< 负载长度

    /**
     * ID 监听器的自定义用户数据。
     *
     * 该数据将存储在监听器插槽中，并通过接收到的消息中的相同字段传递给 ID 回调函数。
     */
    void *userdata;
    void *userdata2;
} TF_Msg;

/**
 * 清除消息结构体
 *
 * @param msg - 要原地清除的消息
 */
static inline void TF_ClearMsg(TF_Msg *msg)
{
    memset(msg, 0, sizeof(TF_Msg));
}

/** TinyFrame 结构体类型定义 */
typedef struct TinyFrame_ TinyFrame;

/**
 * TinyFrame 类型监听器回调函数
 *
 * @param tf - 实例
 * @param msg - 接收到的消息，userdata 在对象内部填充
 * @return 监听器结果 (TF_Result)
 */
typedef TF_Result (*TF_Listener)(TinyFrame *tf, TF_Msg *msg);

/**
 * TinyFrame 类型监听器超时回调函数
 *
 * @param tf - 实例
 * @return 监听器结果
 */
typedef TF_Result (*TF_Listener_Timeout)(TinyFrame *tf);

// ---------------------------------- 初始化 ------------------------------

/**
 * 初始化 TinyFrame 引擎。
 * 这也可以用于完全重置它（移除所有监听器等）。
 *
 * .userdata (或 .usertag) 字段可用于在 TF_WriteImpl() 函数等中识别不同的实例。
 * 请在初始化后设置此字段。
 *
 * 此函数是 TF_InitStatic 的封装，它调用 malloc() 来获取实例。
 *
 * @param tf - 实例指针（如果为 NULL 则分配新实例）
 * @param peer_bit - 用于自身的对端位 (TF_MASTER 或 TF_SLAVE)
 * @return TF 实例 或 NULL
 */
TinyFrame *TF_Init(TF_Peer peer_bit);

/**
 * 使用静态分配的实例结构体初始化 TinyFrame 引擎。
 *
 * 当调用 TF_InitStatic 时，.userdata / .usertag 字段会被保留。
 *
 * @param tf - 实例
 * @param peer_bit - 用于自身的对端位
 * @return 成功返回 true
 */
bool TF_InitStatic(TinyFrame *tf, TF_Peer peer_bit);

/**
 * 反初始化动态分配的 TF 实例
 *
 * @param tf - 实例
 */
void TF_DeInit(TinyFrame *tf);

// ---------------------------------- API 调用 --------------------------------------
// 用户的回调函数
/**
 * 接收传入字节 & 解析帧
 *
 * @param tf - 实例
 * @param buffer - 要处理的字节缓冲区
 * @param count - 缓冲区中的字节数
 */
void TF_Accept(TinyFrame *tf, const uint8_t *buffer, uint32_t count);

/**
 * 接收单个传入字节
 *
 * @param tf - 实例
 * @param c - 接收到的字符
 */
void TF_AcceptChar(TinyFrame *tf, uint8_t c);

/**
 * 此函数应被周期性调用。
 * 时间基准用于解析器中部分帧的超时处理，并自动重置解析器。
 * 它也用于使 ID 监听器过期（如果在注册时设置了超时）。
 *
 * 一个常见的调用位置是 SysTick 中断处理程序。
 *
 * @param tf - 实例
 */
void TF_Tick(TinyFrame *tf);

/**
 * 重置帧解析器状态机。
 * 这不会影响已注册的监听器。
 *
 * @param tf - 实例
 */
void TF_ResetParser(TinyFrame *tf);

// ---------------------------- 消息监听器 -------------------------------

/**
 * 注册一个 ID 监听器（监听特定 Frame ID）。
 *
 * @param tf - 实例
 * @param msg - 消息结构 (包含 frame_id 和 userdata)
 * @param cb - 回调函数
 * @param ftimeout - 超时回调函数
 * @param timeout - 自动移除监听器的超时时间（ticks），0 = 永远保持
 * @return 插槽索引（用于移除），或 TF_ERROR (-1)
 */
bool TF_AddIdListener(TinyFrame *tf, TF_Msg *msg, TF_Listener cb, TF_Listener_Timeout ftimeout, TF_TICKS timeout);

/**
 * 通过消息 ID 移除监听器
 *
 * @param tf - 实例
 * @param frame_id - 我们正在监听的帧 ID
 */
bool TF_RemoveIdListener(TinyFrame *tf, TF_ID frame_id);

/**
 * 注册一个类型监听器（监听特定 Type）。
 *
 * @param tf - 实例
 * @param frame_type - 要监听的帧类型
 * @param cb - 回调函数
 * @return 插槽索引（用于移除），或 TF_ERROR (-1)
 */
bool TF_AddTypeListener(TinyFrame *tf, TF_TYPE frame_type, TF_Listener cb);

/**
 * 通过类型移除监听器
 *
 * @param tf - 实例
 * @param type - 已注册的类型
 */
bool TF_RemoveTypeListener(TinyFrame *tf, TF_TYPE type);

/**
 * 注册一个通用监听器（监听所有消息）。
 *
 * @param tf - 实例
 * @param cb - 回调函数
 * @return 插槽索引（用于移除），或 TF_ERROR (-1)
 */
bool TF_AddGenericListener(TinyFrame *tf, TF_Listener cb);

/**
 * 通过函数指针移除通用监听器
 *
 * @param tf - 实例
 * @param cb - 要移除的回调函数
 */
bool TF_RemoveGenericListener(TinyFrame *tf, TF_Listener cb);

/**
 * 从外部更新 ID 监听器的超时时间（相对于从 ID 监听器内部返回 TF_RENEW）
 *
 * @param tf - 实例
 * @param id - 要更新的监听器 ID
 * @return 如果找到并更新了监听器，返回 true
 */
bool TF_RenewIdListener(TinyFrame *tf, TF_ID id);

// ---------------------------- 帧发送函数 ------------------------------

/**
 * 发送一个帧，不挂载监听器
 *
 * @param tf - 实例
 * @param msg - 消息结构体。ID 存储在 frame_id 字段中
 * @return 成功返回 true
 */
bool TF_Send(TinyFrame *tf, TF_Msg *msg);

/**
 * 类似于 TF_Send，但不使用结构体，直接传入参数
 */
bool TF_SendSimple(TinyFrame *tf, TF_TYPE type, const uint8_t *data, TF_LEN len);

/**
 * 发送一个帧，并可选地附加一个 ID 监听器。
 *
 * @param tf - 实例
 * @param msg - 消息结构体。ID 存储在 frame_id 字段中
 * @param listener - 等待响应的监听器（可以为 NULL）
 * @param ftimeout - 超时回调函数
 * @param timeout - 监听器过期时间（ticks）
 * @return 成功返回 true
 */
bool TF_Query(TinyFrame *tf, TF_Msg *msg, TF_Listener listener,
              TF_Listener_Timeout ftimeout, TF_TICKS timeout);

/**
 * 类似于 TF_Query()，但不使用结构体，直接传入参数
 */
bool TF_QuerySimple(TinyFrame *tf, TF_TYPE type,
                    const uint8_t *data, TF_LEN len,
                    TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout);

/**
 * 发送对已接收消息的响应。
 *
 * @param tf - 实例
 * @param msg - 消息结构体。ID 从 frame_id 读取。设置 ->renew 以重置监听器超时
 * @return 成功返回 true
 */
bool TF_Respond(TinyFrame *tf, TF_Msg *msg);

// ------------------------ 多部分帧 (MULTIPART) 发送函数 -----------------------------
// 这些例程用于发送长帧，而无需一次性准备好所有数据
// （例如：从外设捕获数据或从大内存缓冲区读取数据）

/**
 * 带有“多部分”负载的 TF_Send()。
 * msg.data 被忽略并设置为 NULL。
 */
bool TF_Send_Multipart(TinyFrame *tf, TF_Msg *msg);

/**
 * 带有“多部分”负载的 TF_SendSimple()。
 */
bool TF_SendSimple_Multipart(TinyFrame *tf, TF_TYPE type, TF_LEN len);

/**
 * 带有“多部分”负载的 TF_QuerySimple()。
 */
bool TF_QuerySimple_Multipart(TinyFrame *tf, TF_TYPE type, TF_LEN len, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout);

/**
 * 带有“多部分”负载的 TF_Query()。
 * msg.data 被忽略并设置为 NULL。
 */
bool TF_Query_Multipart(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_Listener_Timeout ftimeout, TF_TICKS timeout);

/**
 * 带有“多部分”负载的 TF_Respond()。
 * msg.data 被忽略并设置为 NULL。
 */
void TF_Respond_Multipart(TinyFrame *tf, TF_Msg *msg);

/**
 * 发送已启动的多部分帧的负载。如果需要，可以多次调用此函数，
 * 直到传输完完整的长度。
 *
 * @param tf - 实例
 * @param buff - 发送字节的缓冲区
 * @param length - 要发送的字节数
 */
void TF_Multipart_Payload(TinyFrame *tf, const uint8_t *buff, uint32_t length);

/**
 * 关闭多部分消息，生成校验和并释放 Tx 锁。
 *
 * @param tf - 实例
 */
void TF_Multipart_Close(TinyFrame *tf);

// ---------------------------------- 内部结构 ----------------------------------
// 仅为了允许静态初始化而公开可见。

enum TF_State_
{
    TFState_SOF = 0,     //!< 等待 SOF (帧头)
    TFState_LEN,         //!< 等待字节数 (Length)
    TFState_HEAD_CKSUM,  //!< 等待头部校验和
    TFState_ID,          //!< 等待 ID
    TFState_TYPE,        //!< 等待消息类型
    TFState_DATA,        //!< 接收负载
    TFState_DATA_CKSUM   //!< 等待校验和
};

struct TF_IdListener_
{
    TF_ID id;
    TF_Listener fn;
    TF_Listener_Timeout fn_timeout;
    TF_TICKS timeout;      // 剩余多少 ticks 禁用此监听器
    TF_TICKS timeout_max;  // 原始超时存储在这里 (0 = 无超时)
    void *userdata;
    void *userdata2;
};

struct TF_TypeListener_
{
    TF_TYPE type;
    TF_Listener fn;
};

struct TF_GenericListener_
{
    TF_Listener fn;
};

/**
 * 帧解析器内部状态。
 */
struct TinyFrame_
{
    /* 公共用户数据 */
    void *userdata;
    uint32_t usertag;

    // --- 结构体的其余部分是内部的，请勿直接访问 ---

    /* 自身状态 */
    TF_Peer peer_bit;  //!< 自身的对端位 (唯一，以避免消息 ID 冲突)
    TF_ID next_id;     //!< 下一个帧 / 帧链 ID

    /* 解析器状态 */
    enum TF_State_ state;
    TF_TICKS parser_timeout_ticks;
    TF_ID id;                         //!< 传入的数据包 ID
    TF_LEN len;                       //!< 负载长度
    uint8_t data[TF_MAX_PAYLOAD_RX];  //!< 数据字节缓冲区
    TF_LEN rxi;                       //!< 字段大小字节计数器
    TF_CKSUM cksum;                   //!< 计算的数据流校验和
    TF_CKSUM ref_cksum;               //!< 从消息中读取的参考校验和
    TF_TYPE type;                     //!< 收集的消息类型编号
    bool discard_data;                //!< 如果 (len > TF_MAX_PAYLOAD)，则设置此标志以读取帧但忽略数据。

    /* 发送 (Tx) 状态 */
    // 用于构建帧的缓冲区
    uint8_t sendbuf[TF_SENDBUF_LEN];  //!< 发送临时缓冲区

    uint32_t tx_pos;    //!< Tx 缓冲区中的下一个写入位置 (用于多部分)
    uint32_t tx_len;    //!< 总预期 Tx 长度
    TF_CKSUM tx_cksum;  //!< 发送校验和累加器

#if !TF_USE_MUTEX
    bool soft_lock;  //!< 如果未启用互斥锁功能，则使用 Tx 锁标志。
#endif

    /* --- 回调 --- */

    /* 事务回调 */
    struct TF_IdListener_ id_listeners[TF_MAX_ID_LST];
    struct TF_TypeListener_ type_listeners[TF_MAX_TYPE_LST];
    struct TF_GenericListener_ generic_listeners[TF_MAX_GEN_LST];

    // 这些计数器用于优化查找时间。
    // 它们指向最高使用的插槽编号，或接近它，具体取决于移除顺序。
    TF_COUNT count_id_lst;
    TF_COUNT count_type_lst;
    TF_COUNT count_generic_lst;
};

// ------------------------ 需由用户实现 ------------------------

/**
 * 向 UART 发送数据的“写字节”函数
 *
 * ! 请在你的应用程序代码中实现此函数 !
 */
extern void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len);

// 互斥锁函数
#if TF_USE_MUTEX

/** 在组合和发送帧之前请求 TX 接口 */
extern bool TF_ClaimTx(TinyFrame *tf);

/** 在组合和发送帧之后释放 TX 接口 */
extern void TF_ReleaseTx(TinyFrame *tf);

#endif

// 自定义校验和函数
#if (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM8) || (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM16) || (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM32)

/**
 * 初始化校验和
 *
 * @return 初始校验和值
 */
extern TF_CKSUM TF_CksumStart(void);

/**
 * 用一个字节更新校验和
 *
 * @param cksum - 上一个校验和值
 * @param byte - 要添加的字节
 * @return 更新后的校验和值
 */
extern TF_CKSUM TF_CksumAdd(TF_CKSUM cksum, uint8_t byte);

/**
 * 完成校验和计算
 *
 * @param cksum - 上一个校验和值
 * @return 最终校验和值
 */
extern TF_CKSUM TF_CksumEnd(TF_CKSUM cksum);

#endif

#endif




