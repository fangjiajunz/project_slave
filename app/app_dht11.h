#ifndef APP_DHT11_H
#define APP_DHT11_H

#include <stdbool.h>
#include <stdint.h>

void app_dht11_init(void);
void app_dht11_poll(void);
bool app_dht11_get(int16_t *temp_x10, uint16_t *humi_x10);
bool app_dht11_is_ready(void);

#endif
