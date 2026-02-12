#ifndef _USB_UART_H__
#define _USB_UART_H__
//#include "os/types.h"
#include "byte_queue.h"

extern byte_queue_t uart_tx_queue;
void uart_init(void);
void uart_deinit(void);
void uart_tx_poll(void);
void uart_rx_poll(void);
extern void serial_on_data_received(uint8_t *buffer, uint16_t len);
#endif /* _USB_UART_H__ */

