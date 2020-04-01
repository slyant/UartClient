#include <rtthread.h>
#include <uart_client.h>

#define LOG_TAG              "uart.client"
#define LOG_LVL              LOG_LVL_INFO
#include <ulog.h>

#define CLIENT_LOCK_NAME            "uclock"
#define CLIENT_SEM_NAME             "ucsem"
#define CLIENT_SEM_RESP_NAME        "ucres"
#define CLIENT_SEM_RESP_END_NAME    "ucrend"
#define CLIENT_THREAD_NAME          "uc"
#define CLIENT_TIME_NAME            "uctime"

#ifndef PKG_UART_CLIENT_MAX_COUNT
	#define PKG_UART_CLIENT_MAX_COUNT	8
#endif

#ifdef PKG_USING_UART_CLIENT

static uart_client_t uart_client_list[PKG_UART_CLIENT_MAX_COUNT] = { 0 };

/* Get uart client by client device name */
uart_client_t uart_client_get_by_name(const char *dev_name)
{
	for(int i = 0; i < PKG_UART_CLIENT_MAX_COUNT; i++)
	{
		if(uart_client_list[i] && rt_strcmp(uart_client_list[i]->device->parent.name, dev_name) == 0)
		{
			return uart_client_list[i];
		}
	}
    return RT_NULL;	
}

static rt_err_t uart_client_rx_ind(rt_device_t dev, rt_size_t size)
{
	uart_client_t client = uart_client_get_by_name(dev->parent.name);
	if(client)
	{
		rt_sem_release(client->rx_notice);
	}

    return RT_EOK;
}

static rt_err_t uart_client_getbyte(uart_client_t client, rt_uint8_t *ch, rt_int32_t timeout)
{
    rt_err_t result = RT_EOK;

    while (rt_device_read(client->device, 0, ch, 1) == 0)
    {
        rt_sem_control(client->rx_notice, RT_IPC_CMD_RESET, RT_NULL);
        result = rt_sem_take(client->rx_notice, timeout);
        if (result != RT_EOK)
        {
            return result;
        }
    }

    return RT_EOK;
}

static rt_size_t uart_client_get_buf(uart_client_t client, rt_int32_t timeout)
{
    rt_size_t recv_index = 0;
    rt_uint8_t temp_ch;    
    while(1)
    {
		if(uart_client_getbyte(client, &temp_ch, timeout) == RT_EOK)
		{
            if(recv_index < client->recv_buf_size - 1)
            {
                client->recv_buf[recv_index++] = temp_ch;
            }
            else
            {
				return recv_index;
            }
		}
		else
		{
			return recv_index;
		}
	}
}

rt_err_t uart_client_request_start(uart_client_t client, rt_uint32_t timeout_ms, rt_uint8_t *req_buf, rt_size_t req_size)
{
	rt_err_t result = RT_EOK;
	if(client == RT_NULL)
	{
		LOG_E("the uart client is null!");
		return -RT_EEMPTY;
	}
	
	rt_sem_take(client->lock, RT_WAITING_FOREVER);
	
	client->resp.timeout = rt_tick_from_millisecond(timeout_ms);
	client->resp.buf = RT_NULL;
	client->resp.buf_size = 0;
	client->resp_consume = RT_FALSE;
	if(client->resp.timeout == 0)
	{
		rt_device_write(client->device, 0, req_buf, req_size);
	}
	else
	{
		rt_sem_control(client->resp_notice, RT_IPC_CMD_RESET, RT_NULL);
		rt_device_write(client->device, 0, req_buf, req_size);
		if(rt_sem_take(client->resp_notice, client->resp.timeout) != RT_EOK)
        {
            LOG_D("uart client(%s) request timeout (%d ticks)!", client->device->parent.name, client->resp->timeout);
            result = -RT_ETIMEOUT;
        }
	}

    return result;	
}

rt_err_t uart_client_request_start_with_rs485(uart_client_t client, rt_uint32_t timeout_ms, rt_uint8_t *req_buf, rt_size_t req_size, void (*set_tx)(void), void (*set_rx)(void))
{
	rt_err_t result = RT_EOK;
	if(client == RT_NULL)
	{
		LOG_E("the uart client is null!");
		return -RT_EEMPTY;
	}
	
	rt_sem_take(client->lock, RT_WAITING_FOREVER);
	
	client->resp.timeout = rt_tick_from_millisecond(timeout_ms);
	client->resp.buf = RT_NULL;
	client->resp.buf_size = 0;
	client->resp_consume = RT_FALSE;
	if(client->resp.timeout == 0)
	{
		set_tx();
		rt_device_write(client->device, 0, req_buf, req_size);
		set_rx();
	}
	else
	{
		rt_sem_control(client->resp_notice, RT_IPC_CMD_RESET, RT_NULL);
		set_tx();
		rt_device_write(client->device, 0, req_buf, req_size);
		set_rx();
		if(rt_sem_take(client->resp_notice, client->resp.timeout) != RT_EOK)
        {
            LOG_D("uart client(%s) request timeout (%d ticks)!", client->device->parent.name, client->resp->timeout);
            result = -RT_ETIMEOUT;
        }
	}

    return result;	
}

void uart_client_request_end(uart_client_t client, rt_bool_t consume)
{
	if(client == RT_NULL) return;
	
	client->resp.timeout = 0;
	client->resp.buf = RT_NULL;
	client->resp.buf_size = 0;
	client->resp_consume = consume;
	rt_sem_release(client->resp_end_notice);
	rt_timer_start(client->send_interval_timer);
}

rt_err_t uart_client_request_no_response(uart_client_t client, rt_uint8_t *req_buf, rt_size_t req_size)
{
	rt_err_t res;
	res = uart_client_request_start(client, 0, req_buf, req_size);
	uart_client_request_end(client, RT_FALSE);
	return res;
}

rt_err_t uart_client_request_no_response_with_rs485(uart_client_t client, rt_uint8_t *req_buf, rt_size_t req_size, void (*set_tx)(void), void (*set_rx)(void))
{
	rt_err_t res;
	res = uart_client_request_start_with_rs485(client, 0, req_buf, req_size, set_tx, set_rx);
	uart_client_request_end(client, RT_FALSE);
	return res;
}

static void client_parser(uart_client_t client)
{      
	rt_size_t size;
    while(1)
    {
		if((size = uart_client_get_buf(client, rt_tick_from_millisecond(client->frame_timeout_ms))) > 0)
		{
			rt_bool_t consume = RT_FALSE;
			client->recv_buf[client->recv_buf_size - 1] = 0x00;
			if(client->resp.timeout > 0)
			{
				client->resp.buf = client->recv_buf;
				client->resp.buf_size = size;
				
				rt_sem_control(client->resp_end_notice, RT_IPC_CMD_RESET, RT_NULL);	
				
				rt_sem_release(client->resp_notice);
				rt_sem_take(client->resp_end_notice, RT_WAITING_FOREVER);
				consume = client->resp_consume;
			}
			if(consume == RT_FALSE && client->frame_handler != RT_NULL)
			{
				client->frame_handler(client->recv_buf, size);
			}				
			rt_memset(client->recv_buf, 0x00, client->recv_buf_size);
		}
    }
}

static void send_interval_timeout(uart_client_t client)
{
    rt_sem_release(client->lock);
}

void uart_client_set_frame_handler(uart_client_t client, rt_uint32_t frame_timeout_ms, void (*frame_handler)(rt_uint8_t *frame_data, rt_size_t size))
{
	if(client == RT_NULL) return;
	
    client->frame_handler = frame_handler;
    client->frame_timeout_ms = frame_timeout_ms;
}

uart_client_t uart_client_create(const char *dev_name, 
                                rt_size_t recv_buf_size, 
                                rt_uint32_t send_interval_ms, 
                                rt_uint32_t frame_timeout_ms, 
                                void (*frame_handler)(rt_uint8_t *frame_data, rt_size_t size))
{
    int result = RT_EOK;
    static int client_num = 0;
    char name[RT_NAME_MAX];	
    rt_err_t open_result = RT_EOK;
    uart_client_t client = RT_NULL;

    RT_ASSERT(dev_name);
    RT_ASSERT(recv_buf_size > 0);	
	
	if(client_num >= PKG_UART_CLIENT_MAX_COUNT)
	{
		result = -RT_EFULL;
		goto __exit;
	}
	
	client = rt_calloc(1, sizeof(struct uart_client));
	if(client == RT_NULL)
	{
		LOG_E("uart client(%s) failure to create! no memory for uart client.", dev_name);
		result = -RT_ENOMEM;
		goto __exit;
	}
	
	client->resp.buf = RT_NULL;
	client->resp.buf_size = 0;
	client->resp.timeout = 0;
	client->resp_consume = RT_FALSE;
	client->parser = RT_NULL;
	
	client->frame_timeout_ms = frame_timeout_ms;
	client->frame_handler = frame_handler;
	
	client->recv_buf = (rt_uint8_t *)rt_calloc(1, recv_buf_size);
    if (client->recv_buf == RT_NULL)
    {
        LOG_E("uart client(%s) failure to create! no memory for ringbuffer pool.", dev_name);
		result = -RT_ENOMEM;
		goto __exit;
    }
	client->recv_buf_size = recv_buf_size;
    
	rt_snprintf(name, RT_NAME_MAX, "%s%d", CLIENT_TIME_NAME, client_num);
    client->send_interval_timer = rt_timer_create(name, 
                                                (void (*)(void *params))send_interval_timeout, 
                                                client, 
                                                rt_tick_from_millisecond(send_interval_ms), 
                                                RT_TIMER_FLAG_ONE_SHOT|RT_TIMER_FLAG_SOFT_TIMER);
    if (client->send_interval_timer == RT_NULL)
    {
        LOG_E("uart client(%s) failure to create! uart_client_timer create failed!", dev_name);
        result = -RT_ENOMEM;
        goto __exit;
    }
	
	rt_snprintf(name, RT_NAME_MAX, "%s%d", CLIENT_LOCK_NAME, client_num);
    client->lock = rt_sem_create(name, 1, RT_IPC_FLAG_FIFO);
    if (client->lock == RT_NULL)
    {
        LOG_E("uart client(%s) failure to create! uart_client_lock create failed!", dev_name);
        result = -RT_ENOMEM;
        goto __exit;
    }

    rt_snprintf(name, RT_NAME_MAX, "%s%d", CLIENT_SEM_NAME, client_num);
    client->rx_notice = rt_sem_create(name, 0, RT_IPC_FLAG_FIFO);
    if (client->rx_notice == RT_NULL)
    {
        LOG_E("uart client(%s) failure to create! uart_client_notice semaphore create failed!", dev_name);
        result = -RT_ENOMEM;
        goto __exit;
    }

    rt_snprintf(name, RT_NAME_MAX, "%s%d", CLIENT_SEM_RESP_NAME, client_num);
    client->resp_notice = rt_sem_create(name, 0, RT_IPC_FLAG_FIFO);
    if (client->resp_notice == RT_NULL)
    {
        LOG_E("uart client(%s) failure to create! uart_client_resp semaphore create failed!", dev_name);
        result = -RT_ENOMEM;
        goto __exit;
    }
	
    rt_snprintf(name, RT_NAME_MAX, "%s%d", CLIENT_SEM_RESP_END_NAME, client_num);
    client->resp_end_notice = rt_sem_create(name, 0, RT_IPC_FLAG_FIFO);
    if (client->resp_end_notice == RT_NULL)
    {
        LOG_E("uart client(%s) failure to create! uart_client_resp_end semaphore create failed!", dev_name);
        result = -RT_ENOMEM;
        goto __exit;
    } 

	rt_snprintf(name, RT_NAME_MAX, "%s%d", CLIENT_THREAD_NAME, client_num);
    client->parser = rt_thread_create(name,
                                     (void (*)(void *parameter))client_parser,
                                     client,
                                     384,
                                     RT_THREAD_PRIORITY_MAX / 3 + client_num,
                                     20);
    if (client->parser == RT_NULL)
    {
		LOG_E("uart client(%s) failure to create! no memory for parser thread.", dev_name);
        result = -RT_ENOMEM;
        goto __exit;
    }	
	
	client->device = rt_device_find(dev_name);
    if (client->device)
    {
        RT_ASSERT(client->device->type == RT_Device_Class_Char);

        /* using DMA mode first */
        open_result = rt_device_open(client->device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_DMA_RX);
        /* using interrupt mode when DMA mode not supported */
        if (open_result == -RT_EIO)
        {
            open_result = rt_device_open(client->device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
        }
        RT_ASSERT(open_result == RT_EOK);

        rt_device_set_rx_indicate(client->device, uart_client_rx_ind);
    }
    else
    {
        LOG_E("uart client failure to create! not find the device(%s).", dev_name);
        result = -RT_ERROR;
        goto __exit;
    }
	
__exit:
    if (result == RT_EOK)
    {		
		uart_client_list[client_num++] = client;
        rt_thread_startup(client->parser);
        LOG_I("uart client on device %s create success.", dev_name);
    }
    else
    {
		if(client)
		{
		    if(client->lock)
			{
				rt_sem_delete(client->lock);				
			}
			if (client->rx_notice)
			{
				rt_sem_delete(client->rx_notice);
			}
			if (client->resp_notice)
			{
				rt_sem_delete(client->resp_notice);
			}			
			if (client->resp_end_notice)
			{
				rt_sem_delete(client->resp_end_notice);
			}
			if(client->recv_buf)
			{
				rt_free(client->recv_buf);
				client->recv_buf = RT_NULL;
			}
			if (client->device)
			{
				rt_device_close(client->device);
			}			
			if(client->parser)
			{
				rt_thread_delete(client->parser);
			}
            if(client->send_interval_timer)
            {
                rt_timer_delete(client->send_interval_timer);
            }            
		
			rt_free(client);
			client = RT_NULL;
		}
        LOG_E("uart client on device %s initialize failed(%d).", dev_name, result);
    }

    return client;
}

#endif
