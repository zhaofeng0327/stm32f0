#include "uart_comm.h"
#include "typedef.h"
#include "utils.h"


#define dzlog_info debug
#define dzlog_debug debug
#define dzlog_error dberr

#define WAIT_RESPONSE_TIME_OUT (3000)

extern bool is_transparent_mode;
static bool one_session_ok = pdFALSE;

static unsigned char is_req_type_valid(unsigned char type)
{
	if (type > 0xCF || type < 0xC0) {
		dzlog_error("invalid req type\r\n");
		return 0;
	} else {
		return 1;
	}
}


static unsigned char is_res_type_valid(unsigned char type)
{
	if (type > 0xAF || type < 0xA0) {
		dzlog_error("invalid res type '%x'\r\n",type);
		return 0;
	} else {
		return 1;
	}
}

void print_hex(char *func,char *data, int len)
{
#if 1
	int i;
	char hex[128] = { 0 };
	dzlog_debug("%s:\r\n", func);
	for (i = 0; i < len; i++) {
		if (0 == i % 16) {
			dzlog_debug("%s\r\n", hex);
			memset(hex, 0, 128);
		}
		sprintf(hex + 3 * (i % 16), "%02x ", data[i]);
	}
	dzlog_debug("%s\r\n", hex);
#endif
}

static unsigned char get_active_res(int ulTimeOut/*millisecond*/)
{
	unsigned int res = 0;
	if(jd_om_task_notify_wait(&res,ulTimeOut) != osOK)	//timeout
		res = 0;
	return (unsigned char)res;
}

static void set_active_res(jd_om_comm *hdl,unsigned char res)
{
	jd_om_task_notify(&(hdl->uart_comm_des.thread_handle_send),res);
	dzlog_debug("set active res as 0x%02x.\r\n", res);
}

osStatus jd_master_com_send_request(jd_om_comm *hdl,unsigned char type)
{
	int payload_size;
	unsigned char res_type;
	osStatus ret;

	switch (type) {
	case REQ_DEVICE_INFO:
	{
		res_type = REQ_DEVICE_INFO;
		payload_size = 0;
		dzlog_debug("host send REQ_DEVICE_INFO !!!!!!\r\n");
		break;
	}

	default:
		return osErrorOS;
	}

	int packet_size = sizeof(MSG_UART_HEAD_T) + payload_size + CHECKSUM_SIZE;
	MSG_UART_PACKAGE_T *pkt = jd_om_malloc(packet_size);

	pkt->head.start = START_CMD;
	pkt->head.slave = SLAVE_1;
	pkt->head.type = res_type;
	pkt->head.payload_len = payload_size;

	//memcpy((char *)pkt + sizeof(MSG_UART_HEAD_T), (char *)data, payload_size);

	unsigned short chksum = crc16((char *)pkt, packet_size - CHECKSUM_SIZE);
	memcpy((char *)pkt + packet_size - CHECKSUM_SIZE, &chksum, CHECKSUM_SIZE);

	ret = jd_om_mq_send(&(hdl->uart_comm_des.send_queue), (void *)pkt);
	if (osErrorOS == ret)
		dzlog_error("res[%02x] to queue error\r\n", res_type);

	return ret;
}


osStatus jd_master_com_send_response(jd_om_comm *hdl,unsigned char type, void *data)
{
	int payload_size;
	unsigned char res_type;
	osStatus ret;

	switch (type) {
		case REQ_BATT_SN_BMS:
		{
			res_type = RES_BATT_SN_BMS;
			payload_size = sizeof(RES_BATT_SN_BMS_T);
			break;
		}

		default:
			return osErrorOS;
	}

	int packet_size = sizeof(MSG_UART_HEAD_T) + payload_size + CHECKSUM_SIZE;
	MSG_UART_PACKAGE_T *pkt = jd_om_malloc(packet_size);

	pkt->head.start = START_CMD;
	pkt->head.slave = SLAVE_1;
	pkt->head.type = res_type;
	pkt->head.payload_len = payload_size;

	memcpy((char *)pkt + sizeof(MSG_UART_HEAD_T), (char *)data, payload_size);

	unsigned short chksum = crc16((char *)pkt, packet_size - CHECKSUM_SIZE);
	memcpy((char *)pkt + packet_size - CHECKSUM_SIZE, &chksum, CHECKSUM_SIZE);

	ret = jd_om_mq_send(&(hdl->uart_comm_des.send_queue), (void *)pkt);
	if (osErrorOS == ret)
		dzlog_error("res[%02x] to queue error\r\n", res_type);
	else{
		//wait uart send task to set responsed
		jd_om_sem_wait(&(hdl->uart_comm_des.sem),0);
	}

	return ret;
}

static osStatus jd_master_com_send_exception_response(jd_om_comm *hdl,unsigned char type, void *data)
{
	int payload_size;
	unsigned char res_type;
	osStatus ret;

	res_type = (type&0x0F)|0xA0;
	payload_size = sizeof(RES_COMMON_T);

	int packet_size = sizeof(MSG_UART_HEAD_T) + payload_size + CHECKSUM_SIZE;
	MSG_UART_PACKAGE_T *pkt = jd_om_malloc(packet_size);

	pkt->head.start = START_CMD;
	pkt->head.slave = SLAVE_1;
	pkt->head.type = res_type;
	pkt->head.payload_len = payload_size;

	memcpy((char *)pkt + sizeof(MSG_UART_HEAD_T), (char *)data, payload_size);

	unsigned short chksum = crc16((char *)pkt, packet_size - CHECKSUM_SIZE);
	memcpy((char *)pkt + packet_size - CHECKSUM_SIZE, &chksum, CHECKSUM_SIZE);

	ret = jd_om_mq_send(&(hdl->uart_comm_des.send_queue), (void *)pkt);
	if (osErrorOS == ret)
		dzlog_error("res[%02x] to queue error\r\n", res_type);

	return ret;
}


#define SECONDS_IN_A_DAY (86400U)
#define SECONDS_IN_A_HOUR (3600U)
#define SECONDS_IN_A_MINUTE (60U)
#define DAYS_IN_A_YEAR (365U)
#define YEAR_RANGE_START (1970U)
#define YEAR_RANGE_END (2099U)

static char recv_data_dispatch(jd_om_comm *hdl,char *pt)
{
	char payload[MAX_QUEUE_ELEMENT_SIZE] = { 0 };
	MSG_UART_HEAD_T* head = (MSG_UART_HEAD_T*)pt;
	static unsigned char last_session_id = -1;
	static unsigned int old_pkt_id = 1;
	unsigned int payload_len = 0;
	unsigned char type;
	bool has_send_res = pdFALSE;

	// 获取start字段
	unsigned char start = head->start;

	//	判断start字段值是否正�?
	if (START_CMD != start) {
		dzlog_error("recv invalid packet\r\n");
		has_send_res = pdTRUE;	//don't send res as invalid start code.
		goto FREE;
	}

	// 获取type字段
	type = head->type;
	dzlog_debug("recv type %02x\r\n", type);

	//判断start字段值是否正�?
	#if 0
	if (!is_req_type_valid(type)){
		has_send_res = pdTRUE;	//don't send res as valid command type
		goto FREE;
	}

	if(head->packet_id == last_session_id){
		dzlog_error("recv repeat packet\r\n");
		goto FREE;
	}
	else{
		last_session_id = head->packet_id;
	}
	#endif
	// 获取payload_len字段
	//payload_len = 0;
	//memcpy(&payload_len, &(head->payload_len), sizeof(payload_len));
	payload_len = head->payload_len;

	// 校验payload_len合法�?
	if (payload_len >= MAX_QUEUE_ELEMENT_SIZE) {
		dzlog_error("recv invalid payload_len:%d\r\n",payload_len);
		goto FREE;
	}

	// 获取payload
	memset(payload, 0, MAX_QUEUE_ELEMENT_SIZE);
	memcpy(payload, pt + sizeof(MSG_UART_HEAD_T), payload_len);


	//获取checksum
	unsigned short chksum;
	memcpy(&chksum, pt + sizeof(MSG_UART_HEAD_T) + payload_len, sizeof(chksum));
	if (chksum != crc16(pt, sizeof(MSG_UART_HEAD_T) + payload_len)) {
		dzlog_error("checksum wrong\r\n");
		goto FREE;
	}

	switch (type) {
		case REQ_BATT_SN_BMS:
		{
			REQ_BATT_SN_BMS_T *req = (REQ_BATT_SN_BMS_T *)(pt + sizeof(MSG_UART_HEAD_T));
			RES_BATT_SN_BMS_T res;
			char indicate[]="JieDianTestBattery";
			res.code = 0;
			res.sn_len = strlen(indicate);
			strcpy((char *)res.sn,indicate);
			//send response to host
			one_session_ok = pdFALSE;
			if(jd_master_com_send_response(hdl,type,(void *)&res) == osOK){
				is_transparent_mode = one_session_ok;
				debug("%s:response sn %s###################\r\n",__func__,
					is_transparent_mode?"success,ready to entry transparent mode":"fail");
			}
			has_send_res = pdTRUE;
			break;
		}

		default:
			break;
	}

FREE:
	if(has_send_res != pdTRUE){
		if(is_req_type_valid(type)){
			RES_COMMON_T res;
			
			dzlog_error("%s:command type '%02x' exception res start to send...\r\n",__func__,type);
			res.code = 1;
			jd_master_com_send_exception_response(hdl,type,(void *)&res);
		}
		else
			dzlog_error("%s:[unknown type=%02x]won't send res as invalid packet received!!!\r\n",
				__func__,type);			
	}
	jd_om_free(pt);
	return 0;
}

static void uart_recv_queue_task(void *p)
{
	jd_om_comm *hdl = (jd_om_comm *)p;
	void *pt = NULL;
	//unsigned int disconnect_cnt = 0,lose_times = 0;
	//unsigned int loseMaximumSec = 0;
	//bool ever_disconnect_notify = pdFALSE;
	//REQ_SHOW_T req;
	dzlog_debug("uart_recv_queue_task start>>>\r\n");
	while (1) {
		//dzlog_debug("%s:@@1@@\r\n",__func__);
		jd_om_mq_recv(&(hdl->uart_comm_des.recv_queue), (void **)&pt, 1000/*millisecond timeout*/);		
		if(pt == NULL){
			//dzlog_error("%s: timeout\r\n",__func__);
		}
		else{
			if(strncmp((char *)pt,ReqNameByAT,strlen(ReqNameByAT))==0){
				dzlog_debug("%s:recv req AT name\r\n",__func__);
				jd_om_free(pt);
			}
			else{
				recv_data_dispatch(hdl,(char *)pt);
			}
		}
	}
}

static void send_err_recovery(jd_om_comm *hdl,unsigned char ActiveReq)
{

}

static void uart_send_queue_task(void *p)
{
	jd_om_comm *hdl = (jd_om_comm *)p;
	int slave = SLAVE_1;
	int ret;
	int lens;
	void *pt = NULL;
	jd_om_comm_addr to_addr;
	char slave_addr[16] = { 0 };
	int cnt_resend = 0;
	int send_err = 0;
	unsigned char active_req = 0;

	dzlog_debug("uart_send_queue_task start>>>\r\n");
	while (1) {
		MSG_UART_HEAD_T *head = NULL;
		char res;

		jd_om_mq_recv(&(hdl->uart_comm_des.send_queue), (void **)&pt, 0);
		if(pt == NULL){
			dzlog_error("%s: invalid message: NULL\r\n",__func__);
			continue;
		}

		head = (MSG_UART_HEAD_T *)pt;
		lens = sizeof(MSG_UART_HEAD_T) + head->payload_len + CHECKSUM_SIZE;
		print_hex((char *)__func__,pt, lens);
		res = head->type;
		#if 0
		if (!is_res_type_valid(res)) {
			dzlog_error("invalid request 0x%02x\r\n", res);
			jd_om_free(pt);
			continue;
		}
		#endif
		
		slave = head->slave;
		sprintf(slave_addr, "0.%d.0", slave);
		to_addr.addr = tlc_iaddr(slave_addr);

		active_req = head->type;
		
		dzlog_debug("send msg type '%x'\r\n",res);
		cnt_resend = 1;
RESEND_DATA:
		send_err = 0;
		ret = jd_om_send(hdl, &to_addr, pt, lens, 0);

		if (ret <= 0) {
			dzlog_error("send fail count %d\r\n", cnt_resend);

			if (res == RES_BATT_SN_BMS) {
				if (cnt_resend++ <= 3) {
					jd_om_msleep(500);
					goto RESEND_DATA;
				}
			}
			send_err = 1;
		}
		//dzlog_debug("send msg completed\r\n");
		jd_om_free(pt);
		if (send_err)
			send_err_recovery(hdl,active_req);
		else if(res == RES_BATT_SN_BMS){			
			one_session_ok = (ret > 0)?pdTRUE:pdFALSE;
			jd_om_sem_signal(&(hdl->uart_comm_des.sem));
		}
	}
}

/*!
 * @brief uart receive Task responsible for loopback.
 */
static void uart_recv_task(void *p)
{
	jd_om_comm *uart_hdl = (jd_om_comm *)p;
	int r_len;
	jd_om_comm_addr from_addr;
	char buf[MAX_QUEUE_ELEMENT_SIZE] = { 0 };
	char *pt;

	dzlog_debug("uart_recv_task start>>>\r\n");

	//task of receving.
	while (1) {
		memset(buf, 0, MAX_QUEUE_ELEMENT_SIZE);

		r_len = jd_om_recv(uart_hdl, &from_addr, buf, MAX_QUEUE_ELEMENT_SIZE);
		if ((r_len > sizeof(MSG_UART_HEAD_T)) ||
			((r_len == strlen(ReqNameByAT)) && (strncmp(buf,ReqNameByAT,r_len)==0))) {
			print_hex((char *)__func__,buf, r_len);
			pt = jd_om_malloc(r_len);
			memcpy(pt, buf, r_len);
			jd_om_mq_send(&(uart_hdl->uart_comm_des.recv_queue), pt);
			//dzlog_debug("%s:send recv queue end...\r\n",__func__);
		}
		else{
			dzlog_error("recv msg from uart. invalid len:%d\r\n", r_len);
		}
	}
}

int uart_comm_task_init(jd_om_comm *uart_hdl,int usart_no)
{
	int ret = 0;

	ret = jd_om_comm_open(uart_hdl, usart_no);
	if (0 != ret) {
		dzlog_error("open uart number %d fail ret %d.\r\n", usart_no, ret);
		vTaskSuspend(NULL);
	}

	uart_hdl->uart_comm_des.thread_handle_recv = jd_om_thread_create("Uart_recv_queue_task", uart_recv_queue_task, uart_hdl, 5*1024/*configMINIMAL_STACK_SIZE + 10*/, recv_queue_task_PRIORITY);
	if(uart_hdl->uart_comm_des.thread_handle_recv == NULL){
		dzlog_error("create recv thread fail.\r\n");
        while (1)
            ;
	}
	dzlog_info("Uart_recv_queue_task create successful !\r\n");
	
	uart_hdl->uart_comm_des.thread_handle_send = jd_om_thread_create("Uart_send_queue_task", uart_send_queue_task, uart_hdl, 1024/*configMINIMAL_STACK_SIZE + 10*/, send_queue_task_PRIORITY);
	if(uart_hdl->uart_comm_des.thread_handle_send == NULL){
		dzlog_error("create send thread fail.\r\n");
        while (1)
            ;
	}
	dzlog_info("Uart_send_queue_task create successful !\r\n");

    if(jd_om_thread_create("Uart_recv_task", uart_recv_task, (void *)uart_hdl, 1024, uart_task_PRIORITY)==NULL)
    {
        debug("create Uart recv Task failed!.\r\n");
        while (1)
            ;
    }
	dzlog_info("Uart recv task create successful !\r\n");

	return 0;
}

//HOOK function to detect task stack overflow!
void vApplicationStackOverflowHook( TaskHandle_t xTask, signed char *pcTaskName )
{
     dzlog_error("task:[%s] ######stack overflow######\r\n", pcTaskName);
}


