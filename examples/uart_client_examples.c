#include <rtthread.h>
#include <board.h>
#include "uart_client.h"

#define LOG_TAG              "uart.rs485"
#define LOG_LVL              LOG_LVL_INFO
#include <ulog.h>

#define UART_NAME           "uart2"
#define UART_BAUD_RATE		BAUD_RATE_115200

#define RECV_BUF_SIZE       (256)
#define FRAME_TIMEOUT_MS	500
#define SEND_INTERVAL_MS    1000

static uart_client_t client = RT_NULL;

void uart_set_frame_handler(void (*frame_handler)(rt_uint8_t *frame_data, rt_size_t size))
{
    uart_client_set_frame_handler(client, FRAME_TIMEOUT_MS, frame_handler);
}

rt_err_t uart_request_no_response(rt_uint8_t *frame_data, rt_uint16_t frame_len)
{
	rt_err_t res;
	if(client != RT_NULL)
	{
		res = uart_client_request_no_response(client, frame_data, frame_len);
	}
	else
	{
		res = -RT_EEMPTY;
	}

	return res;
}

rt_err_t uart_request_start(rt_uint8_t *frame_data, rt_uint16_t frame_len, rt_uint32_t timeout_ms)
{
	rt_err_t res;
	if(client != RT_NULL)
	{
		res = uart_client_request_start(client, timeout_ms, frame_data, frame_len);
	}
	else
	{
		res = -RT_EEMPTY;
	}

	return res;
}

void uart_request_end(rt_bool_t consume)
{
	uart_client_request_end(client, consume);
}

//与另一个串口设备通讯的例子：设置时间，不需要设备响应
rt_err_t uart_set_datetime(rt_uint16_t year, rt_uint8_t month, rt_uint8_t day, rt_uint8_t hour, rt_uint8_t min, rt_uint8_t sec)
{
    rt_err_t res;
	if(client == RT_NULL) return -RT_EEMPTY;

	char *frame_data = "2020-04-01 17:48:00";
	rt_uint16_t frame_len = rt_strlen(frame_data);
	res = uart_request_no_response((rt_uint8_t*)frame_data, frame_len);
    //LOG_HEX("req_buf", 16, frame_data, frame_len);

    return res;
}

//与另一个串口设备通讯的例子：配置设备，需要设备响应
rt_err_t uart_set_config(char *params)
{
    rt_err_t res;
	if(client == RT_NULL) return -RT_EEMPTY;

    rt_bool_t consume = RT_FALSE;
	char *frame_data = params;
	rt_uint16_t frame_len = rt_strlen(frame_data);
	res = uart_2_request_start((rt_uint8_t*)frame_data, frame_len, 2000);
    //LOG_HEX("req_buf", 16, frame_data, frame_len);
	if(res == RT_EOK)
	{
		//LOG_HEX("client->resp.buf", 16, client->resp.buf, client->resp.buf_size);
        if(rt_strstr(client->resp.buf, "OK"))
        {
			consume = RT_TRUE;
        }
        else
        {
            res = -RT_ETIMEOUT;
        }
    }
	else
	{
		LOG_E("set config failed(%d)", res);
	}
	uart_request_end(consume);

    return res;
}

int uart_client_init(void)
{
    struct rt_serial_device *serial_device = (struct rt_serial_device *)rt_device_find(UART_NAME);
    if(serial_device == RT_NULL)
    {
        LOG_E("uart client init failed! not find the serial device:%s!", UART_NAME);
        return -RT_ERROR;
    }
    serial_device->config.baud_rate = UART_BAUD_RATE;

    client = uart_client_create(UART_NAME, RECV_BUF_SIZE, SEND_INTERVAL_MS, FRAME_TIMEOUT_MS, RT_NULL);
    if(client == RT_NULL)
    {
        LOG_E("uart client init failed!");
        return -RT_ERROR;
    }

    LOG_I("uart client init success!");

    return RT_EOK;
}
