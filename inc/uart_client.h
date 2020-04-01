#ifndef __UART_GENERIC_DEVICE_H__
#define __UART_GENERIC_DEVICE_H__

#ifdef PKG_USING_UART_CLIENT

struct uart_response
{
	rt_uint8_t *buf;
	rt_size_t buf_size;
	rt_uint32_t timeout;
};

struct uart_client
{
	rt_device_t device;
	rt_uint8_t *recv_buf;
	rt_size_t recv_buf_size;
	rt_uint32_t frame_timeout_ms;
	rt_sem_t rx_notice;
    rt_sem_t lock;

	struct uart_response resp;
	rt_sem_t resp_notice;
	rt_sem_t resp_end_notice;
	rt_bool_t resp_consume;
	
	rt_thread_t parser;	
	void (*frame_handler)(rt_uint8_t *frame_data, rt_size_t size);
    rt_timer_t send_interval_timer;
};
typedef struct uart_client *uart_client_t;

uart_client_t uart_client_get_by_name(const char *dev_name);
uart_client_t uart_client_create(const char *dev_name, rt_size_t recv_buf_size, rt_uint32_t send_interval_ms, rt_uint32_t frame_timeout_ms, void (*frame_handler)(rt_uint8_t *frame_data, rt_size_t size));
rt_err_t uart_client_request_start(uart_client_t client, rt_uint32_t timeout_ms, rt_uint8_t *req_buf, rt_size_t req_size);
rt_err_t uart_client_request_start_with_rs485(uart_client_t client, rt_uint32_t timeout_ms, rt_uint8_t *req_buf, rt_size_t req_size, void (*set_tx)(void), void (*set_rx)(void));
void uart_client_request_end(uart_client_t client, rt_bool_t consume);
rt_err_t uart_client_request_no_response(uart_client_t client, rt_uint8_t *req_buf, rt_size_t req_size);
rt_err_t uart_client_request_no_response_with_rs485(uart_client_t client, rt_uint8_t *req_buf, rt_size_t req_size, void (*set_tx)(void), void (*set_rx)(void));
void uart_client_set_frame_handler(uart_client_t client, rt_uint32_t frame_timeout_ms, void (*frame_handler)(rt_uint8_t *frame_data, rt_size_t size));

#endif
#endif
