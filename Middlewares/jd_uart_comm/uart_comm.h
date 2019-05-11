/************************************************************
Copyright (C), 2017, Tech. Co., Ltd.
FileName: uart_comm.h
Description: 
History:
***********************************************************/

#ifndef __UART_COMM_H__
#define __UART_COMM_H__

#include "jd_os_middleware.h"
#include "protocal.h"

int uart_comm_task_init(jd_om_comm *uart_hdl,int usart_no);
osStatus jd_master_com_send_response(jd_om_comm *hdl,unsigned char type, void *data);
osStatus jd_master_com_send_request(jd_om_comm *hdl,unsigned char type);

#endif
