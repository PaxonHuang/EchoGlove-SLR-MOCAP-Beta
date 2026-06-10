#pragma once
#include <stdbool.h>
#include "data_structures.h"

bool uart_receiver_init(int uart_port, int tx_pin, int rx_pin, int baud);
bool uart_receiver_poll(GlovePacket* out);
uint32_t uart_receiver_count(void);
