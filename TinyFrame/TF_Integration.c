#include "TinyFrame.h"

/**
 * 这是将 TinyFrame 集成到应用程序的示例。
 *
 * TF_WriteImpl() 是必须实现的函数，互斥锁函数是弱引用的，
 * 如果不使用可以删除。它们会在所有 TF_Send/Respond 函数中被调用。
 *
 * 另外，如果你想使用监听器超时功能，请记得定期调用 TF_Tick()。
 */

void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len)
{
    // 发送到 UART
}

// --------- 互斥锁回调函数 ----------
// 仅当配置文件中 TF_USE_MUTEX 为 1 时才需要。
// 如果不使用互斥锁请删除这些函数

/** 在组装和发送帧之前获取 TX 接口锁 */
bool TF_ClaimTx(TinyFrame *tf)
{
    // 获取互斥锁
    return true;  // 返回 true 表示成功
}

/** 在组装和发送帧之后释放 TX 接口锁 */
void TF_ReleaseTx(TinyFrame *tf)
{
    // 释放互斥锁
}

// --------- 自定义校验和 ---------
// 仅当使用自定义校验和类型时才需要在此定义。
// 如果使用内置校验和类型请删除这些函数

/** 初始化校验和 */
TF_CKSUM TF_CksumStart(void)
{
    return 0;
}

/** 使用一个字节更新校验和 */
TF_CKSUM TF_CksumAdd(TF_CKSUM cksum, uint8_t byte)
{
    return cksum ^ byte;
}

/** 完成校验和计算 */
TF_CKSUM TF_CksumEnd(TF_CKSUM cksum)
{
    return cksum;
}
