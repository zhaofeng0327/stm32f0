#include "uart.h"
#include "utils.h"
#include "stm32f0xx_hal.h"
#include "stm32f0xx_it.h"
#include "jd_os_middleware.h"
#include "cmsis_os.h"
#include "main.h"
#include <stdarg.h>

//#define UART_COMM_DBG
#define UART_HALF_DUPLEX_RX_DMA
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/* External variables --------------------------------------------------------*/
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart6;

extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart3_tx;
extern DMA_HandleTypeDef hdma_usart4_rx;

uint8_t uart3_rx_buffer[UART_BUFFER_SIZE] = {0};//接收缓冲
uint8_t uart3_tx_buffer[UART_BUFFER_SIZE] = {0};//发送缓冲
uint8_t uart4_rx_buffer[UART_BUFFER_SIZE] = {0};//接收缓冲
uint16_t uart3_rx_len = 0;             //接收一帧数据的长度
uint16_t uart4_rx_len = 0;             //接收一帧数据的长度
#ifndef UART_HALF_DUPLEX_RX_DMA
uint8_t aRxBuffer4;				//接收中断缓冲
static bool Rx4Started = pdFALSE;	//single-wire
#endif
/*******************************************************************************
 * Code
 ******************************************************************************/
//const char *to_send = "Hello,FreeRTOS USART driver !\r\n";
//const char *send_buffer_overrun = "\r\nRing buffer overrun!\r\n";

SemaphoreHandle_t UartRxSem4;

extern bool is_transparent_mode;

static bool Tx3Started = pdFALSE;
static bool Tx4Started = pdFALSE;	//single-wire

static void uart_recv_data_show(const char *func,UART_HandleTypeDef *huart,uint8_t *buff,uint16_t len)
{
	uint16_t i=0;

	debug("%s[%s]--------------------->uart %s[len=%d]\r\n",__func__,func,
		(USART3 == huart->Instance)?"3":"4-single-wire",len);
	for(i=0;i<len;i++){
		debug("%02x ",buff[i]);
	}
	debug("\r\n",__func__);
}

void uart_reinit(UART_HandleTypeDef *huart)
{
	HAL_UART_DeInit(huart);
	if(USART4 == huart->Instance)
		HAL_HalfDuplex_Init(huart);
	else
		HAL_UART_Init(huart);
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

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	static unsigned char loopcnt3 = 0;
	static unsigned char loopcnt4 = 0;
	static unsigned char loopcntrx4 = 0;
	static unsigned char led_on = 0;
	static unsigned char time_cnt = 0;
	
	if(htim->Instance == TIM6){
		//100ms timeout.
		if(Tx3Started){
			if(loopcnt3++>30){	//3s timeout
				uart_reinit(&huart3);
				uart3_rx_len = 0;
				memset(uart3_rx_buffer,0x00,UART_BUFFER_SIZE);
				HAL_UART_Receive_DMA(&huart3,uart3_rx_buffer,UART_BUFFER_SIZE);//使能接收中断
				loopcnt3 = 0;
				Tx3Started = pdFALSE;
				dberr("%s:uart3 tx timeout !!!\r\n",__func__);
			}
		}
		else
			loopcnt3 = 0;

		if(Tx4Started){
			if(loopcnt4++>30){ //3s timeout
				uart_reinit(&huart4);
				uart4_rx_len = 0;
				memset(uart4_rx_buffer,0x00,UART_BUFFER_SIZE);
				HAL_UART_Receive_DMA(&huart4,uart4_rx_buffer,UART_BUFFER_SIZE);//使能接收中断
				loopcnt4 = 0;
				Tx4Started = pdFALSE;
				dberr("%s:uart4(single-wire) tx timeout !!!\r\n",__func__);
			}
		}
		else
			loopcnt4 = 0;
		
		#ifndef UART_HALF_DUPLEX_RX_DMA
		if(Rx4Started){
			if(loopcntrx4++>30){ //3s timeout
				//uart_reinit(&huart4);
				uart4_rx_len = 0;
				memset(uart4_rx_buffer,0x00,UART_BUFFER_SIZE);
				loopcntrx4 = 0;
				Rx4Started = pdFALSE;
				dberr("%s:uart4(single-wire) rx timeout !!!\r\n",__func__);
			}
		}
		else
			loopcntrx4 = 0;
		#endif

		if(!is_transparent_mode){
			//blink every 500ms.
			if(time_cnt%5 == 0){
				HAL_GPIO_TogglePin(GPIOC,LED1_Pin|LED2_Pin|LED3_Pin);
			}
			if(time_cnt++ == 254)//overflow !
				time_cnt = 0;
		}
		else if(!led_on){
			//led on.
			time_cnt = 0;
			led_on = 1;
			HAL_GPIO_WritePin(GPIOC, LED1_Pin|LED2_Pin|LED3_Pin, GPIO_PIN_SET);
		}
	}
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if(USART3 == huart->Instance){
		Tx3Started = pdFALSE;
	}
}

#ifndef UART_HALF_DUPLEX_RX_DMA
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	
	  /* Prevent unused argument(s) compilation warning */
	  UNUSED(huart);

	  if(huart->Instance == USART4){ //single wire
		  if(uart4_rx_len >= UART_BUFFER_SIZE)  //溢出判断
		  {
			  uart4_rx_len = 0;
			  memset(uart4_rx_buffer,0x00,UART_BUFFER_SIZE);
			  debug("%s:uart4 buffer is overflow '%d' !!!\r\n",__func__,uart4_rx_len);
		  }
		  else
		  {
			  uart4_rx_buffer[uart4_rx_len++] = aRxBuffer4;  //接收数据转存
			  //check LLC Head
			  if((uart4_rx_len == 1)&&(uart4_rx_buffer[0] != 0xAA)){
			  	uart4_rx_len = 0;
			  	memset(uart4_rx_buffer,0x00,UART_BUFFER_SIZE);
				dberr("%s:recv the bad 1 byte '%02x' of LLC Head from uart4\r\n",__func__,uart4_rx_buffer[0]);
			  }
			  else if((uart4_rx_len == 2)&&(uart4_rx_buffer[1] != 0x05)){
			  	uart4_rx_len = 0;
			  	memset(uart4_rx_buffer,0x00,UART_BUFFER_SIZE);
				dberr("%s:recv the bad 2 byte '%02x' of LLC Head from uart4\r\n",__func__,uart4_rx_buffer[1]);
			  }
			  else{
			  	  if(uart4_rx_len == 2)	//recv the LLC head completed !
				  	Rx4Started = pdTRUE;
			  	  //check LLC Tail
				  if(uart4_rx_buffer[uart4_rx_len-1] == 0xA5) //判断结束位
				  {
				  	  #ifdef UART_COMM_DBG
				  	  uart_recv_data_show(__func__,huart,uart4_rx_buffer,uart4_rx_len);
					  #endif
				  	  Rx4Started = pdFALSE;
					  if(!is_transparent_mode){
						#ifdef UART_COMM_DBG
						  jz_uart_write_ex(&huart4, (uint8_t *)&uart4_rx_buffer, uart4_rx_len);
						  uart4_rx_len = 0;
						  memset(uart4_rx_buffer,0x00,UART_BUFFER_SIZE); //清空数组
						#else
						  osMutexRelease(UartRxSem4);
						#endif
					  }
					  else{
						  Tx3Started = pdTRUE;
						  memcpy(uart3_tx_buffer,uart4_rx_buffer,uart4_rx_len);
						  HAL_UART_Transmit_DMA(&huart3,uart3_tx_buffer, uart4_rx_len);
						  uart4_rx_len = 0;
						  memset(uart4_rx_buffer,0x00,UART_BUFFER_SIZE); //清空数组
					  }
				  }
			  }
		  }  
		  HAL_UART_Receive_IT(huart, (uint8_t *)&aRxBuffer4, 1);   //再开启接收中断
	  }
}
#endif

void HAL_UART_RxIdleCpltCallback(UART_HandleTypeDef *huart)
{
	  uint32_t tmp_flag = 0;
      uint32_t temp;

	  //debug("%s-------->\r\n",__func__);
      if(USART3 == huart->Instance){ //communication with PC
	        tmp_flag =__HAL_UART_GET_FLAG(huart,UART_FLAG_IDLE); //获取IDLE标志位
	                
	        if((tmp_flag != RESET))//idle标志被置位
	        { 
	        	#ifdef UART_COMM_DBG
	        	debug("%s>>>>>>>>>uart3.\r\n",__func__);
				#endif
                __HAL_UART_CLEAR_IDLEFLAG(huart);//清除标志位         
                HAL_UART_DMAStop(huart); //                                         
                temp = huart->Instance->ISR;  //清除状态寄存器SR,读取SR寄存器可以实现清除SR寄存器的功能
                temp = huart->Instance->RDR; //读取数据寄存器中的数据
                temp  =  __HAL_DMA_GET_COUNTER(&hdma_usart3_rx);// 获取DMA中未传输的数据个数，NDTR寄存器分析见下面                                         
                uart3_rx_len =  UART_BUFFER_SIZE - temp; //总计数减去未传输的数据个数，得到已经接收的数据个数
                #ifdef UART_COMM_DBG
                uart_recv_data_show(__func__,huart,uart3_rx_buffer, uart3_rx_len);
				#endif
				if(uart3_rx_len == 0){
					#ifdef UART_COMM_DBG
					dberr("%s:uart3 recv invalid data len ?\r\n",__func__);
					#endif
				}
                else if(is_transparent_mode){
					jz_uart_write_ex(&huart4, (uint8_t *)&uart3_rx_buffer, uart3_rx_len);//将收到的信息发送出去
				}
				else{
					#ifdef UART_COMM_DBG
					memcpy(uart3_tx_buffer,uart3_rx_buffer,uart3_rx_len);
					HAL_UART_Transmit_DMA(&huart3,uart3_tx_buffer, uart3_rx_len);
					#endif
				}
				HAL_UART_Receive_DMA(huart,uart3_rx_buffer,UART_BUFFER_SIZE);//重新打开DMA接收
	        }
      }
	  else if(USART4 == huart->Instance){//single-wire
	  	#ifdef UART_HALF_DUPLEX_RX_DMA
		  tmp_flag =__HAL_UART_GET_FLAG(huart,UART_FLAG_IDLE); //获取IDLE标志位
				  
		  if((tmp_flag != RESET))//idle标志被置位
		  { 
		  	  #ifdef UART_COMM_DBG
		  	  debug("%s>>>>>>>>>uart4.\r\n",__func__);
			  #endif
			  __HAL_UART_CLEAR_IDLEFLAG(huart);//清除标志位		 
			  HAL_UART_DMAStop(huart); //										  
			  temp = huart->Instance->ISR;	//清除状态寄存器SR,读取SR寄存器可以实现清除SR寄存器的功能
			  temp = huart->Instance->RDR; //读取数据寄存器中的数据
			  temp	=  __HAL_DMA_GET_COUNTER(&hdma_usart4_rx);// 获取DMA中未传输的数据个数，NDTR寄存器分析见下面										 
			  uart4_rx_len =  UART_BUFFER_SIZE - temp; //总计数减去未传输的数据个数，得到已经接收的数据个数
			  #ifdef UART_COMM_DBG
			  uart_recv_data_show(__func__,huart,uart4_rx_buffer,uart4_rx_len);
			  #endif
			  if(uart4_rx_len == 0){
			  	#ifdef UART_COMM_DBG
				dberr("%s:uart4 single-wire recv invalid data len ?\r\n",__func__);
				#endif
				HAL_UART_Receive_DMA(huart,uart4_rx_buffer,UART_BUFFER_SIZE);//重新打开DMA接收
			  }
              else if(is_transparent_mode){
			  	Tx3Started = pdTRUE;
			  	memcpy(uart3_tx_buffer,uart4_rx_buffer,uart4_rx_len);
				HAL_UART_Transmit_DMA(&huart3,uart3_tx_buffer, uart4_rx_len);
				HAL_UART_Receive_DMA(huart,uart4_rx_buffer,UART_BUFFER_SIZE);//重新打开DMA接收 
			  }
			  else{
			  	#ifdef UART_COMM_DBG
				jz_uart_write_ex(huart, (uint8_t *)&uart4_rx_buffer, uart4_rx_len);
				#else
				osMutexRelease(UartRxSem4);
				#endif
			  } 
		  }
		#endif
	  }
}

int jz_uart_write_ex(void *fd, u8 * buffer, int lens)
{
	int ret = -1;
	if (lens <=  0)
		return 0;

	UART_HandleTypeDef* hdl = (UART_HandleTypeDef*)fd;
	USART_TypeDef* ins = hdl->Instance;

	if (USART4 == ins) {	//single wire
		HAL_HalfDuplex_EnableTransmitter(hdl);
		Tx4Started = pdTRUE;
	}

	if(HAL_UART_Transmit_Retry(hdl, buffer, lens,UartTimeOut(1000)) == HAL_OK){	
		ret = lens;
		Tx4Started = pdFALSE;
	}
	else{
		#ifdef UART_COMM_DBG
		dberr("%s:error\r\n",__func__);
		#endif
	}
	//SET_BIT(huart2.Instance->CR1, USART_CR1_RE_Pos);

	if (USART4 == ins) {	//single wire
		HAL_HalfDuplex_EnableReceiver(hdl);
		#ifdef UART_HALF_DUPLEX_RX_DMA
		__HAL_UART_CLEAR_IDLEFLAG(hdl);//清除标志位
		HAL_UART_Receive_DMA(hdl,uart4_rx_buffer,UART_BUFFER_SIZE);//重新打开DMA接收 
		#else
		HAL_UART_Receive_IT(hdl, (uint8_t *)&aRxBuffer4, 1);
		#endif
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
	ret = uart4_rx_len;
	memcpy(buffer,uart4_rx_buffer,ret);
    uart4_rx_len = 0;
    memset(uart4_rx_buffer,0x00,UART_BUFFER_SIZE); //清空数组		 
	if(ret < 3){
		dberr("%s:recv less data of '%d' ???\r\n",__func__,ret);
		ret = 0;
	}
	else if(buffer[0] != 0xAA || buffer[1] != 0x05 || buffer[ret-1] != 0xA5){
		int i = 0;
		dberr("%s:recv invalid data:\r\n",__func__);
		for(i=0;i<ret;i++){
			dberr("%02x-",buffer[i]);
		}
		dberr("\r\n");
		ret = 0;
	}
	if(ret < 3){
		#ifdef UART_HALF_DUPLEX_RX_DMA
		HAL_UART_Receive_DMA(&huart4,uart4_rx_buffer,UART_BUFFER_SIZE);//重新打开DMA接收
		#else
		HAL_UART_Receive_IT(&huart4, (uint8_t *)&aRxBuffer4, 1);
		#endif
	}
	return (int)ret;
}

void * jz_uart_init_ex(int usart_no)
{
	void *hdl = NULL;
	switch(usart_no){
		case 4:		//single wire
			debug("%s ：uart 3&4>>>>>\r\n",__func__);
			if(jd_om_sem_new(&UartRxSem4) != osOK){
				dberr("%s:create usart4 semaphore fail.\r\n",__func__);
				return NULL;
			}
			//enable uart3 mode
			uart3_rx_len = 0;
			__HAL_UART_CLEAR_IDLEFLAG(&huart3);//清除标志位
			__HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);  //使能idle中断
			HAL_UART_Receive_DMA(&huart3,uart3_rx_buffer,UART_BUFFER_SIZE);//使能接收中断
			//enable uart4 half signle-wire mode
			uart4_rx_len = 0;
			HAL_HalfDuplex_EnableReceiver(&huart4);
			#ifdef UART_HALF_DUPLEX_RX_DMA
			__HAL_UART_CLEAR_IDLEFLAG(&huart4);//清除标志位
			__HAL_UART_ENABLE_IT(&huart4, UART_IT_IDLE);  //使能idle中断
			HAL_UART_Receive_DMA(&huart4,uart4_rx_buffer,UART_BUFFER_SIZE);//使能接收中断
			#else
			HAL_UART_Receive_IT(&huart4, (uint8_t *)&aRxBuffer4, 1);
			#endif
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

