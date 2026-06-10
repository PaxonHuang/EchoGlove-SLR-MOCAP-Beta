#include "uart_receiver.h"
#include <cstring>
#include "driver/uart.h"
#include "esp_log.h"
#include "uart_frame.h"

static const char* TAG = "uart_rx";
static int s_port = 0;
static uint32_t s_count = 0;

static uint8_t s_buf[UART_FRAME_MAX_SIZE * 4];
static size_t s_buf_len = 0;

bool uart_receiver_init(int uart_port, int tx_pin, int rx_pin, int baud) {
    s_port = uart_port;
    uart_config_t cfg = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(s_port, 2048, 512, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(s_port, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(s_port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART%d init: TX=%d RX=%d baud=%d", s_port, tx_pin, rx_pin, baud);
    return true;
}

bool uart_receiver_poll(GlovePacket* out) {
    int avail = uart_read_bytes(s_port, s_buf + s_buf_len, sizeof(s_buf) - s_buf_len, 0);
    if (avail > 0) s_buf_len += avail;

    size_t consumed = 0;
    if (uart_frame_decode(s_buf, s_buf_len, out, &consumed)) {
        if (consumed < s_buf_len)
            memmove(s_buf, s_buf + consumed, s_buf_len - consumed);
        s_buf_len -= consumed;
        s_count++;
        return true;
    }

    // Discard leading non-magic bytes
    while (s_buf_len > 0 && s_buf[0] != UART_MAGIC_0) {
        memmove(s_buf, s_buf + 1, --s_buf_len);
    }
    return false;
}

uint32_t uart_receiver_count(void) {
    return s_count;
}
