/*
 * 设计目的:平台相关
 * linux os
 */
#include "jd_os_middleware.h"

int jd_om_comm_open(
		jd_om_comm * handle,
		int usart_no){
#if( TLC_COMM == 1 )
	handle->ptlc 	= tlc_open(0, usart_no);
	if(!handle->ptlc)
		return -1;
	return 0;
#else
	return -1;
#endif
}

int jd_om_comm_close(
		jd_om_comm * handle){
#if( TLC_COMM == 1 )
	if( !handle->ptlc )
		return -1;
	return tlc_close(handle->ptlc);
#else
	return -1;
#endif
}

int jd_om_send(	jd_om_comm * handle,
				jd_om_comm_addr * toaddr,
				void * data,
				int data_len,
				int msec){
#if( TLC_COMM == 1 )
	return tlc_send(handle->ptlc,(u8*) data, data_len, toaddr->addr);
#else
	return -1;
#endif
}

int jd_om_recv(
		jd_om_comm * handle,
		jd_om_comm_addr *fromaddr,
		void * data,
		int data_len){
#if( TLC_COMM == 1 )
	return  tlc_recv(handle->ptlc, (u8*) data, data_len, &fromaddr->addr);
#else
	return -1;
#endif
}

int jd_om_send_2_batt(jd_om_comm * handle, void * data, int data_len){
    //1.2.1 1
    //data[0] = 0xff;
    //data[1] = batt_id;
    //g_sw_id = batt_id;
#if( TLC_COMM == 1 )
	return tlc_send_raw(handle->ptlc,(u8*) data, data_len);
#else
	return -1;
#endif
}

//        tlc  <--->  tlc-->write read
//android ---> master --->battery
int jd_om_recv_f_batt(jd_om_comm * handle, void * data, int data_len){
#if( TLC_COMM == 1 )
	return  tlc_recv_raw(handle->ptlc, (u8*) data, data_len);
#else
	return -1;
#endif
    //batt_id = g_sw_id;
}


JD_OM_THREAD jd_om_thread_create( const char *pcName, void( *pxThread )( void *pvParameters ), void *pvArg, int iStackSize, int iPriority )
{
	JD_OM_THREAD xCreatedTask;
	portBASE_TYPE xResult;
	JD_OM_THREAD xReturn;

    xResult = xTaskCreate( pxThread, pcName, iStackSize, pvArg, iPriority, &xCreatedTask );

    if( xResult == pdPASS )
    {
        xReturn = xCreatedTask;
    }
    else
    {
        xReturn = NULL;
    }

    return xReturn;
}

void jd_om_thread_start(void){
	vTaskStartScheduler();
}

void jd_om_thread_switch(void){
	taskYIELD();
}

void jd_om_pthread_cond_signal(JD_OM_THREAD *pxThread){
	xTaskNotifyGive(*pxThread);
}

void jd_om_pthread_cond_timedwait(u32 ulTimeout){
    if( ulTimeout != 0UL )
    {
    	ulTaskNotifyTake( pdTRUE, ulTimeout / portTICK_PERIOD_MS );
    }
    else
    {
    	ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
    }

    return;

}

osStatus jd_om_task_notify(JD_OM_THREAD *pxThread,unsigned char InValue){
	xTaskNotify( *pxThread, (uint32_t)InValue, eSetValueWithOverwrite );
	return osOK;
}

osStatus jd_om_task_notify_wait(u32 *OutValue,u32 ulTimeOut){
	uint32_t ulNotificationValue;
	const uint32_t ulMax = 0xffffffffUL;
	BaseType_t res = pdFALSE;
	
	if(ulTimeOut>0)
		res = xTaskNotifyWait( 0, ulMax, &ulNotificationValue, ulTimeOut/portTICK_PERIOD_MS );
	else
		res = xTaskNotifyWait( 0, ulMax, &ulNotificationValue, portMAX_DELAY );
	*OutValue = ulNotificationValue;
	return (res == pdTRUE)?osOK:osErrorOS;
}

void jd_om_msleep(u32 millisec){
	u32 _millisec = portTICK_PERIOD_MS;

	if(millisec > portTICK_PERIOD_MS)
		_millisec = millisec;
	#if 0
	vTaskDelay(_millisec/portTICK_PERIOD_MS);
	#else
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	vTaskDelayUntil(&xLastWakeTime, _millisec/portTICK_PERIOD_MS);
	#endif
}

osStatus jd_om_sem_new( JD_OM_SEM *pxSemaphore)
{
	osStatus xReturn = osErrorOS;

    *pxSemaphore = xSemaphoreCreateBinary();

    if( *pxSemaphore != NULL )
    {
        xReturn = osOK;
    }

    return xReturn;
}

int jd_om_sem_wait( JD_OM_SEM *pxSemaphore, u32 ulTimeout )
{
	TickType_t xStartTime, xEndTime, xElapsed;
	unsigned long ulReturn;

    xStartTime = xTaskGetTickCount();

    if( ulTimeout != 0UL )
    {
        if( xSemaphoreTake( *pxSemaphore, ulTimeout / portTICK_PERIOD_MS ) == pdTRUE )
        {
            xEndTime = xTaskGetTickCount();
            xElapsed = (xEndTime - xStartTime) * portTICK_PERIOD_MS;
            ulReturn = xElapsed;
        }
        else
        {
            ulReturn = SYS_ARCH_TIMEOUT;
        }
    }
    else
    {
        while( xSemaphoreTake( *pxSemaphore, portMAX_DELAY ) != pdTRUE );
        xEndTime = xTaskGetTickCount();
        xElapsed = ( xEndTime - xStartTime ) * portTICK_PERIOD_MS;

        if( xElapsed == 0UL )
        {
            xElapsed = 1UL;
        }

        ulReturn = xElapsed;
    }

    return ulReturn;
}

void jd_om_sem_signal( JD_OM_SEM *pxSemaphore )
{
    xSemaphoreGive( *pxSemaphore );
}

osStatus jd_om_mutex_create(JD_OM_MUTEX *pxMutex){
	osStatus xReturn = osErrorOS;

	*pxMutex = xSemaphoreCreateMutex();

	if( *pxMutex != NULL )
	{
		xReturn = osOK;
	}

	return xReturn;
}

int jd_om_mutex_try_lock(JD_OM_MUTEX *pxMutex){
	return (xSemaphoreTake(*pxMutex, 0) == pdTRUE) ? 0:1;
}

void jd_om_mutex_lock(JD_OM_MUTEX *pxMutex){
	while(pdPASS != xSemaphoreTake(*pxMutex, portMAX_DELAY));
}

void jd_om_mutex_unlock(JD_OM_MUTEX *pxMutex){
	xSemaphoreGive(*pxMutex);
}

void jd_om_mutex_delete(JD_OM_MUTEX *pxMutex){
	vSemaphoreDelete(*pxMutex);
}

osStatus jd_om_mq_create(JD_OM_QUEUE *pxQueue,int qsize){
	osStatus xReturn = osErrorOS;
	*pxQueue = xQueueCreate( qsize, sizeof( void * ) );
	if( *pxQueue != NULL )
	{
		xReturn = osOK;
	}
	return xReturn;
}

osStatus jd_om_mq_send_to_head(JD_OM_QUEUE *pxQueue,void * pdata){
	while( xQueueSendToFront( *pxQueue, &pdata, portMAX_DELAY ) != pdTRUE );
	return osOK;
}

osStatus jd_om_mq_send(JD_OM_QUEUE *pxQueue,void * pdata){
	while( xQueueSendToBack( *pxQueue, &pdata, portMAX_DELAY ) != pdTRUE );
	return osOK;
}

osStatus jd_om_mq_send_from_isr(JD_OM_QUEUE *pxQueue,void * pdata,BaseType_t *pxHigherPriorityTaskWoken){
	if(xQueueSendToBackFromISR(*pxQueue, &pdata,pxHigherPriorityTaskWoken) == pdTRUE)
		return osOK;
	else
		return osErrorOS;
}

int jd_om_mq_counts(JD_OM_QUEUE *pxQueue){
	return (int)uxQueueMessagesWaiting( *pxQueue );
}

u32 jd_om_mq_recv(JD_OM_QUEUE *pxQueue,
                       void ** pdata,
                       u32 millisec){
	void *pvDummy;
	TickType_t xStartTime, xEndTime, xElapsed;
	unsigned long ulReturn;

    xStartTime = xTaskGetTickCount();

    if( NULL == pdata )
    {
        pdata = &pvDummy;
    }

    if( millisec != 0UL )
    {
        if( pdTRUE == xQueueReceive( *pxQueue, &( *pdata ), millisec/ portTICK_PERIOD_MS ) )
        {
            xEndTime = xTaskGetTickCount();
            xElapsed = ( xEndTime - xStartTime ) * portTICK_PERIOD_MS;

            ulReturn = xElapsed;
        }
        else
        {
            /* Timed out. */
            *pdata = NULL;
            ulReturn = SYS_ARCH_TIMEOUT;
        }
    }
    else
    {
        while( pdTRUE != xQueueReceive( *pxQueue, &( *pdata ), portMAX_DELAY ) );
        xEndTime = xTaskGetTickCount();
        xElapsed = ( xEndTime - xStartTime ) * portTICK_PERIOD_MS;

        if( xElapsed == 0UL )
        {
            xElapsed = 1UL;
        }

        ulReturn = xElapsed;
    }

    return ulReturn;
}

void jd_om_delete_mq(JD_OM_QUEUE *pxQueue){
	unsigned long ulMessagesWaiting;
	
	ulMessagesWaiting = uxQueueMessagesWaiting( *pxQueue );
	//configASSERT( ( ulMessagesWaiting == 0 ) );
	vQueueDelete( *pxQueue );
}

void * jd_om_malloc(size_t size){
	void *p = (void *)pvPortMalloc(size);
	uint8_t *temp = (uint8_t *)p;
	if (p)
	{
	    for (uint32_t count = 0U; count < size; count++)
	    {
	        temp[count] = 0U;
	    }
	}
	else
		dberr("%s:fail!!!\r\n",__func__);
	return p;
}

void jd_om_free(void* p){
	vPortFree(p);
}

