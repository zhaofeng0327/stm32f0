#include "uart.h"
#include "utils.h"
#include "stm32f0xx_hal.h"
#include "jd_os_middleware.h"
#include "cmsis_os.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart6;
extern bool is_transparent_mode;

volatile uint8_t uart3_ch;
volatile uint8_t uart4_ch;

typedef struct {
	volatile uint8_t	idle;
	volatile uint32_t	head;
	volatile uint32_t	tail;
	volatile uint32_t	len;
	u8			ring_buf[LLC_PACK_SZ_MAX];
}RingBuf;

RingBuf uart3_ringbuf;
RingBuf uart4_ringbuf;

JD_OM_SEM uart3_rx_sem;
JD_OM_SEM uart4_rx_sem;



void init_ringbuf(RingBuf *rbuf)
{
	memset(rbuf, 0, sizeof(RingBuf));
}

int write_ringbuf(RingBuf *rbuf, u8 data)
{
	if (rbuf->len >= LLC_PACK_SZ_MAX) //判断缓冲区是否已满
		return 0;
	rbuf->ring_buf[rbuf->tail++] = data;
	rbuf->tail = (rbuf->tail) % LLC_PACK_SZ_MAX;//防止越界非法访问
	rbuf->len++;
	if (0xA5 == data) {
		rbuf->idle = 1;
		if (rbuf == &uart3_ringbuf)
			osMutexRelease(uart3_rx_sem);
		else if (rbuf == &uart4_ringbuf)
			osMutexRelease(uart4_rx_sem);
	}
	return 1;
}

void uart_config(UART_HandleTypeDef *hdl, USART_TypeDef *num)
{
	hdl->Instance = num;
	hdl->Init.BaudRate = 115200;
	hdl->Init.WordLength = UART_WORDLENGTH_8B;
	hdl->Init.StopBits = UART_STOPBITS_1;
	hdl->Init.Parity = UART_PARITY_NONE;
	hdl->Init.Mode = UART_MODE_TX_RX;
	hdl->Init.HwFlowCtl = UART_HWCONTROL_NONE;
	hdl->Init.OverSampling = UART_OVERSAMPLING_16;
	hdl->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	hdl->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
}



void *jz_uart_init_ex(int usart_no)
{
	debug("%s %d>>>>>\r\n", __func__, usart_no);
	switch (usart_no) {
#if 0
	case 3:
	{
		uart_config(&huart3, USART3);
		if (HAL_UART_Init(&huart3) != HAL_OK) {
			debug("uart3 init error\r\n");
			return NULL;
		} else {
			if (jd_om_sem_new(&uart3_rx_sem) != osOK) {
				dberr("%s:create usart3 semaphore fail.\r\n", __func__);
				return NULL;
			}
			init_ringbuf(&uart3_ringbuf);
			HAL_UART_Receive_IT(&huart3, (uint8_t *)&uart3_ch, 1);
			return &huart3;
		}
		break;
	}
#endif
	case 4:
	{

		uart_config(&huart3, USART3);
		if (HAL_UART_Init(&huart3) != HAL_OK) {
			debug("uart3 init error\r\n");
			return NULL;
		} else {
			if (jd_om_sem_new(&uart3_rx_sem) != osOK) {
				dberr("%s:create usart3 semaphore fail.\r\n", __func__);
				return NULL;
			}
			init_ringbuf(&uart3_ringbuf);
			HAL_UART_Receive_IT(&huart3, (uint8_t *)&uart3_ch, 1);
		}


		uart_config(&huart4, USART4);
		if (HAL_HalfDuplex_Init(&huart4) != HAL_OK) {
			debug("uart4 init error\r\n");
			return NULL;
		} else {
			if (jd_om_sem_new(&uart4_rx_sem) != osOK) {
				dberr("%s:create usart4 semaphore fail.\r\n", __func__);
				return NULL;
			}
			init_ringbuf(&uart4_ringbuf);
			HAL_UART_Receive_IT(&huart4, (uint8_t *)&uart4_ch, 1);
			return &huart4;
		}
		break;
	}
	default:
		return 0;
	}

	return 0;
}

void jz_uart_close_ex(void *fd)
{
	debug("%s >>>>>\r\n", __func__);
	HAL_UART_DeInit(&huart3);
	HAL_UART_DeInit(&huart4);
}


int jz_uart_write_ex(void *fd, u8 *buffer, int lens)
{
	int ret = 0;

	if (lens <= 0)
		return 0;

	UART_HandleTypeDef *hdl = (UART_HandleTypeDef *)fd;
	USART_TypeDef *ins = hdl->Instance;

	if (USART3 == ins) {
		HAL_UART_Transmit(fd, buffer, lens, 0xFFFFFFFFU);
		ret = lens - hdl->TxXferCount;
		HAL_UART_Receive_IT(hdl, (uint8_t *)&uart3_ch, 1);
	} else if (USART4 == ins) {
		HAL_HalfDuplex_EnableTransmitter(hdl);
		HAL_UART_Transmit(fd, buffer, lens, 0xFFFFFFFFU);
		ret = lens - hdl->TxXferCount;
		HAL_HalfDuplex_EnableReceiver(hdl);
		HAL_UART_Receive_IT(hdl, (uint8_t *)&uart4_ch, 1);
	}

	return ret;
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle)
{
	if (USART3 == UartHandle->Instance) {
		write_ringbuf(&uart3_ringbuf, uart3_ch);
		HAL_UART_Receive_IT(UartHandle, (uint8_t *)&uart3_ch, 1);
	} else if (USART4 == UartHandle->Instance) {
		write_ringbuf(&uart4_ringbuf, uart4_ch);
		HAL_UART_Receive_IT(UartHandle, (uint8_t *)&uart4_ch, 1);
	}
}


void uart3_read_ex()
{
	RingBuf *rb = &uart3_ringbuf;
	uint32_t r = 0;
	unsigned char buffer[512] = { 0 };
	osMutexWait(uart3_rx_sem, 0xFFFFFFFF);
	r = rb->len;
	if (rb->head < rb->tail) {
		memcpy(buffer, rb->ring_buf + rb->head, r);
	} else {
		int len1 = LLC_PACK_SZ_MAX - rb->head;
		memcpy(buffer, rb->ring_buf + rb->head, len1);
		memcpy(buffer + len1, rb->ring_buf, r - len1);
	}
	rb->head = (rb->head + r) % LLC_PACK_SZ_MAX;
	rb->len -= r;

	printf("uart3 recv raw -----\r\n");
	dump_buffer(buffer, r);

	if (is_transparent_mode) {
		jz_uart_write_ex(&huart4, buffer, r);
	}
}


int jz_uart_read_ex(void *fd, u8 *buffer, int lens, uint32_t ulTimeout /*millisec*/)
{
	uint32_t timeout = 0;
	uint32_t r = 0;
	//uint32_t olen = 0;
	RingBuf *rb = 0;

	//uint32_t d = 1;

	if (NULL == fd)
		return 0;

	UART_HandleTypeDef *hdl = (UART_HandleTypeDef *)fd;

	USART_TypeDef *ins = hdl->Instance;

	 if (USART4 == ins) {
		rb = &uart4_ringbuf;
		while (1) {
			memset(buffer, 0, lens);
			osMutexWait(uart4_rx_sem, 0xFFFFFFFF);
			r = lens > rb->len ? rb->len : lens;
			if (rb->head < rb->tail) {
				memcpy(buffer, rb->ring_buf + rb->head, r);
			} else {
				int len1 = LLC_PACK_SZ_MAX - rb->head;
				memcpy(buffer, rb->ring_buf + rb->head, len1);
				memcpy(buffer + len1, rb->ring_buf, r - len1);
			}
			rb->head = (rb->head + r) % LLC_PACK_SZ_MAX;
			rb->len -= r;

			if (is_transparent_mode)
				jz_uart_write_ex(&huart3, buffer, r);
			else
				return r;
		}
	}

	return 0;
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	static unsigned char time_cnt = 0;

	if (time_cnt++ % 5 == 0) {
		if (!is_transparent_mode)
			HAL_GPIO_TogglePin(GPIOC, LED1_Pin | LED2_Pin | LED3_Pin);
		else
			HAL_GPIO_WritePin(GPIOC, LED1_Pin | LED2_Pin | LED3_Pin, GPIO_PIN_SET);

	}
}

static int inHandlerMode (void)
{
  return __get_IPSR() != 0;
}

int print_usart(char const*format, ...)
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
    if(vsnprintf(buf, sizeof(buf),format, ap) > 0){
        HAL_UART_Transmit(&huart6, (uint8_t *)buf, strlen(buf), 100);
    }
    va_end(ap);

    if(inHandlerMode() != 0)
        taskENABLE_INTERRUPTS();
}

