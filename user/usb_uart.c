#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "byte_queue.h"
#include <stdbool.h>

#ifndef USB_DEBUG_ENABLE
#define USB_DEBUG_ENABLE 1
#endif

/* USB CDC UART Configuration */
#define CONFIG_UART_TX_SIZE 512
#define CONFIG_UART_RX_SIZE 512
#define TX_CACHE_SIZE CONFIG_UART_TX_SIZE
#define RX_CACHE_SIZE CONFIG_UART_RX_SIZE

uint8_t tx_cache[TX_CACHE_SIZE];
uint8_t rx_cache[RX_CACHE_SIZE];
byte_queue_t uart_tx_queue;
byte_queue_t uart_rx_queue;
static bool _b_inited = false;
void uart_init() {
	if (_b_inited) {
		return;
	}
	_b_inited = true;
    byte_queue_init(&uart_tx_queue, tx_cache, TX_CACHE_SIZE);
	byte_queue_init(&uart_rx_queue, rx_cache, RX_CACHE_SIZE);
#if USB_DEBUG_ENABLE
	MX_USB_DEVICE_Init();
#endif
}

void uart_deinit(void) {
	if (!_b_inited) {
		return;
	}
	_b_inited = false;
	// MX_USB_DEVICE_DeInit();  // 暂不支持
}

void uart_tx_poll(void) {
#if USB_DEBUG_ENABLE
	uint8_t buffer[32];
	while(true) {
		if (CDC_Transmit_isBusy() == USBD_BUSY) {
			break;
		}
		uint16_t len = byte_queue_read(&uart_tx_queue, buffer, sizeof(buffer));
		if (len <= 0) {
			break;
		}
		CDC_Transmit_FS(buffer, len);
	}
#endif
}
extern void serial_on_data_received(uint8_t *buffer, uint16_t len);

void uart_rx_poll(void) {
#if USB_DEBUG_ENABLE
	uint8_t buffer[64];
	while(true) {
		uint16_t len = byte_queue_read(&uart_rx_queue, buffer, sizeof(buffer));
		if (len <= 0) {
			break;
		}
		serial_on_data_received(buffer, len);
	}
#endif
}

void usb_uart_rx_handler(uint8_t *data, uint32_t len) {
	byte_queue_write(&uart_rx_queue, data, len);
}
