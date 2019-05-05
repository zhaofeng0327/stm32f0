/*
 * 2017-09-18
 * 盛耀微科技
 * wxj@glinkwin.com
 */
#ifndef __JZUART_HH__
#define __JZUART_HH__

	#include "typedef.h"

	#define ReqNameByAT	"AT+NAME?\r"
	#define ResNameByAT	"+NAME:JdWuTuo\r\n\r\nOK\r\n\r\n"
	
	void print_usart(char *format, ...);
	EXPORT_API void * jz_uart_init_ex(int usart_no);
	EXPORT_API void jz_uart_close_ex(void *fd);
	EXPORT_API int jz_uart_write_ex(void *fd, u8 * buffer, int lens,uint32_t ulTimeout/*ticks*/);
	EXPORT_API int jz_uart_read_ex(void *fd, u8 * buffer, int lens,uint32_t ulTimeout/*millisec*/);

#endif
