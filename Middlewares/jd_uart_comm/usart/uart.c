#include "uart.h"
#include "utils.h"
#include "stm32f0xx_hal.h"
#include "stm32f0xx_it.h"
#include "jd_os_middleware.h"
#include "cmsis_os.h"
#include <stdarg.h>

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/* External variables --------------------------------------------------------*/
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart6;

/*******************************************************************************
 * Code
 ******************************************************************************/
//const char *to_send = "Hello,FreeRTOS USART driver !\r\n";
//const char *send_buffer_overrun = "\r\nRing buffer overrun!\r\n";

uint8_t aRxBuffer3;				//接收中断缓冲
uint8_t Uart_RxBuff3[512];		//接收缓冲
uint8_t Uart_Rx_Cnt3 = 0;		//接收缓冲计数

uint8_t aRxBuffer4;				//接收中断缓冲
uint8_t Uart_RxBuff4[512];		//接收缓冲
uint8_t Uart_Rx_Cnt4 = 0;		//接收缓冲计数

uint8_t	cAlmStr1[] = "数据溢出(大于512)\r\n";
uint8_t	cAlmStr2[] = "未知串口\r\n";
uint8_t	cAlmStr3[] = "设备未初始化\r\n";

SemaphoreHandle_t UartRxSem4;

extern bool is_transparent_mode;

static bool Rx3Started = pdFALSE;
static bool Rx4Started = pdFALSE;	//single-wire

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	static unsigned char loopcnt3 = 0;
	static unsigned char loopcnt4 = 0;
	
	if(htim->Instance == TIM6){
		//100ms timeout.
		if(Rx3Started){
			if(loopcnt3++>30){	//3s timeout
				loopcnt3 = 0;
				Uart_Rx_Cnt3 = 0;
				memset(Uart_RxBuff3,0x00,sizeof(Uart_RxBuff3));
				Rx3Started = pdFALSE;
				dberr("%s:uart3 rx timeout !!!\r\n",__func__);
			}
		}
		else
			loopcnt3 = 0;
		
		if(Rx4Started){
			if(loopcnt4++>30){ //3s timeout
				loopcnt4 = 0;
				Uart_Rx_Cnt4 = 0;
				memset(Uart_RxBuff4,0x00,sizeof(Uart_RxBuff4));
				Rx4Started = pdFALSE;
				dberr("%s:uart4(single-wire) rx timeout !!!\r\n",__func__);
			}
		}
		else
			loopcnt4 = 0;			
	}
}

static HAL_StatusTypeDef HAL_UART_Transmit_Retry(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
	unsigned char retry_cnt = 3;
	HAL_StatusTypeDef status;
	
	while(((status=HAL_UART_Transmit(huart,pData,Size,Timeout)) != HAL_OK) && (retry_cnt-- > 0)){
		;
	}
	return status;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	
	  /* Prevent unused argument(s) compilation warning */
	  UNUSED(huart);

	  if(huart->Instance == USART3){ //communication with PC
		  if(!is_transparent_mode)
		  {
			  Uart_Rx_Cnt3 = 0;
			  memset(Uart_RxBuff3,0x00,sizeof(Uart_RxBuff3));
			  //HAL_UART_Transmit(huart, (uint8_t *)&cAlmStr3, sizeof(cAlmStr3),UartTimeOut(3000));
			  debug("%s:uart single wire is not ready!!!\r\n",__func__);
		  }
		  else if(Uart_Rx_Cnt3 >= sizeof(Uart_RxBuff3))  //溢出判断
		  {
			  Uart_Rx_Cnt3 = 0;
			  memset(Uart_RxBuff3,0x00,sizeof(Uart_RxBuff3));
			  //HAL_UART_Transmit(huart, (uint8_t *)&cAlmStr1, sizeof(cAlmStr1),UartTimeOut(3000));
			  debug("%s:uart3 buffer is overflow '%d' !!!\r\n",__func__,Uart_Rx_Cnt3);
		  }
		  else
		  {
			  Uart_RxBuff3[Uart_Rx_Cnt3++] = aRxBuffer3;  //接收数据转存
			  //check LLC Head
			  if((Uart_Rx_Cnt3 == 1)&&(Uart_RxBuff3[0] != 0xAA)){
			  	Uart_Rx_Cnt3 = 0;
			  	memset(Uart_RxBuff3,0x00,sizeof(Uart_RxBuff3));
				dberr("%s:recv the bad 1 byte '%02x' of LLC Head from uart3",__func__,Uart_RxBuff3[0]);
			  }
			  else if((Uart_Rx_Cnt3 == 2)&&(Uart_RxBuff3[1] != 0x05)){
			  	Uart_Rx_Cnt3 = 0;
			  	memset(Uart_RxBuff3,0x00,sizeof(Uart_RxBuff3));
				dberr("%s:recv the bad 2 byte '%02x' of LLC Head from uart3",__func__,Uart_RxBuff3[1]);
			  }
			  else{
			  	  if(Uart_Rx_Cnt3 == 2)	//recv the LLC head completed !
				  	Rx3Started = pdTRUE;
				  //check LLC Tail
				  if(Uart_RxBuff3[Uart_Rx_Cnt3-1] == 0xA5) //判断结束位
				  {
				  	  Rx3Started = pdFALSE;
					  jz_uart_write_ex(&huart4, (uint8_t *)&Uart_RxBuff3, Uart_Rx_Cnt3,UartTimeOut(3000));//将收到的信息发送出去
					  Uart_Rx_Cnt3 = 0;
					  memset(Uart_RxBuff3,0x00,sizeof(Uart_RxBuff3)); //清空数组
				  }
			  }
		  }
		  
		  HAL_UART_Receive_IT(huart, (uint8_t *)&aRxBuffer3, 1);   //再开启接收中断
	  }
	  else if(huart->Instance == USART4){ //single wire
		  if(Uart_Rx_Cnt4 >= sizeof(Uart_RxBuff4))  //溢出判断
		  {
			  Uart_Rx_Cnt4 = 0;
			  memset(Uart_RxBuff4,0x00,sizeof(Uart_RxBuff4));
			  HAL_UART_Transmit(huart, (uint8_t *)&cAlmStr1, sizeof(cAlmStr1),UartTimeOut(3000));
			  debug("%s:uart4 buffer is overflow '%d' !!!\r\n",__func__,Uart_Rx_Cnt4);
		  }
		  else
		  {
			  Uart_RxBuff4[Uart_Rx_Cnt4++] = aRxBuffer4;  //接收数据转存
			  //check LLC Head
			  if((Uart_Rx_Cnt4 == 1)&&(Uart_RxBuff4[0] != 0xAA)){
			  	Uart_Rx_Cnt4 = 0;
			  	memset(Uart_RxBuff4,0x00,sizeof(Uart_RxBuff4));
				dberr("%s:recv the bad 1 byte '%02x' of LLC Head from uart3",__func__,Uart_RxBuff4[0]);
			  }
			  else if((Uart_Rx_Cnt4 == 2)&&(Uart_RxBuff4[1] != 0x05)){
			  	Uart_Rx_Cnt4 = 0;
			  	memset(Uart_RxBuff4,0x00,sizeof(Uart_RxBuff4));
				dberr("%s:recv the bad 2 byte '%02x' of LLC Head from uart3",__func__,Uart_RxBuff4[1]);
			  }
			  else{
			  	  if(Uart_Rx_Cnt4 == 2)	//recv the LLC head completed !
				  	Rx4Started = pdTRUE;
			  	  //check LLC Tail
				  if(Uart_RxBuff4[Uart_Rx_Cnt4-1] == 0xA5) //判断结束位
				  {
				  	  Rx4Started = pdFALSE;
					  if(!is_transparent_mode)
						  osMutexRelease(UartRxSem4);
					  else{
						  HAL_UART_Transmit_Retry(&huart3, (uint8_t *)&Uart_RxBuff4, Uart_Rx_Cnt4,UartTimeOut(3000)); //将收到的信息发送出去
						  Uart_Rx_Cnt4 = 0;
						  memset(Uart_RxBuff4,0x00,sizeof(Uart_RxBuff4)); //清空数组
					  }
				  }
			  }
		  }
		  
		  HAL_UART_Receive_IT(huart, (uint8_t *)&aRxBuffer4, 1);   //再开启接收中断
	  }
	  else{
		//dberr("%s:unknown uart\r\n",__func__);
		HAL_UART_Transmit(huart, (uint8_t *)&cAlmStr2, sizeof(cAlmStr2),UartTimeOut(3000));
	  }
}

int jz_uart_write_ex(void *fd, u8 * buffer, int lens,uint32_t ulTimeout/*ticks*/)
{
	int ret = -1;
	if (lens <=  0)
		return 0;

	UART_HandleTypeDef* hdl = (UART_HandleTypeDef*)fd;
	USART_TypeDef* ins = hdl->Instance;

	if (USART4 == ins) {	//single wire
		HAL_HalfDuplex_EnableTransmitter(hdl);
	}

	if(HAL_UART_Transmit_Retry(hdl, buffer, lens, ulTimeout) == HAL_OK)
		ret = lens;
	else
		dberr("%s:error\r\n",__func__);
	//SET_BIT(huart2.Instance->CR1, USART_CR1_RE_Pos);

	if (USART4 == ins) {	//single wire
		HAL_HalfDuplex_EnableReceiver(hdl);
		HAL_UART_Receive_IT(hdl, (uint8_t *)&aRxBuffer4, 1);
	}
	//debug("%s lens %d>>>>>\r\n", __func__, lens);
	//dump_buffer(buffer, ret);
	return ret;
}

int jz_uart_read_ex(void *fd, u8 * buffer, int lens,uint32_t ulTimeout/*millisec*/)
{
	size_t ret=0;

	//debug("%s:recv start,buf_len=%d\r\n",__func__,lens);
	osMutexWait(UartRxSem4,0xFFFFFFFF);
	ret = Uart_Rx_Cnt4;
	memcpy(buffer,Uart_RxBuff4,ret);
    Uart_Rx_Cnt4 = 0;
    memset(Uart_RxBuff4,0x00,sizeof(Uart_RxBuff4)); //清空数组		
	if(ret > 0){
		//debug("\r\n%s:[len=%d,buffer=%s]\r\n",__func__,(int)ret,(char *)buffer);
	}

	return (int)ret;
}

void * jz_uart_init_ex(int usart_no)
{
	void *hdl = NULL;
	switch(usart_no){
		case 3:		//communication with PC
			Uart_Rx_Cnt3 = 0;
			memset(Uart_RxBuff3,0x00,sizeof(Uart_RxBuff3));
			hdl = (void *)&huart3;
			break;

		case 4:		//single wire
			if(jd_om_sem_new(&UartRxSem4) != osOK){
				dberr("%s:create usart4 semaphore fail.\r\n",__func__);
				return NULL;
			}
			Uart_Rx_Cnt4 = 0;
			memset(Uart_RxBuff4,0x00,sizeof(Uart_RxBuff4));
			hdl = (void *)&huart4;
			break;

		default:
			dberr("%s:unknown usart num=%d\r\n",__func__,usart_no);
			break;	
	}

	return hdl;
}

void jz_uart_close_ex(void *fd)
{
	debug("%s >>>>>\r\n",__func__);
}

static int inHandlerMode (void)
{
  return __get_IPSR() != 0;
}

void print_usart(char *format, ...)
{
    char buf[128];

    if(inHandlerMode() != 0)	//in interrupt mode
       taskDISABLE_INTERRUPTS();
    else{
	    while(HAL_UART_GetState(&huart6) == HAL_UART_STATE_BUSY_TX)
            osThreadYield();
    }

    va_list ap;
    va_start(ap, format);
    if(vsprintf(buf, format, ap) > 0){
        HAL_UART_Transmit(&huart6, (uint8_t *)buf, strlen(buf), 100);
    }
    va_end(ap);

    if(inHandlerMode() != 0)
        taskENABLE_INTERRUPTS();
}

/*-----------------------------EOF-----------------------------------*/

