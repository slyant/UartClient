#ifndef __UART_CLIENT_EXAMPLES_H__
#define __UART_CLIENT_EXAMPLES_H__

//串口设备通讯API
int uart_init(void);
void uart_set_frame_handler(void (*frame_handler)(rt_uint8_t *frame_data, rt_size_t size));
rt_err_t uart_request_no_response(rt_uint8_t *frame_data, rt_uint16_t frame_len);
rt_err_t uart_request_start(rt_uint8_t *frame_data, rt_uint16_t frame_len, rt_uint32_t timeout_ms);
void uart_request_end(rt_bool_t consume);

//与另一个串口设备通讯的例子：设置时间，不需要设备响应
rt_err_t uart_set_datetime(rt_uint16_t year, rt_uint8_t month, rt_uint8_t day, rt_uint8_t hour, rt_uint8_t min, rt_uint8_t sec);
//与另一个串口设备通讯的例子：配置设备，需要设备响应
rt_err_t uart_set_config(char *params);

#endif
