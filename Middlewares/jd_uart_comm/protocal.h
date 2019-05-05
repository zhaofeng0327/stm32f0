#ifndef __PROTOCAL_H__
#define __PROTOCAL_H__

#define FW_VER	"1.0.1"
#define bool BaseType_t
//#define TASK_DEBUG

#define START_CMD						0xFF	//协议首字�?

#define REQ_DEVICE_INFO					0xC0	//柜机请求坞托信息
#define REQ_BATT_SN_BMS					0xC1	//柜机请求读取SN及BMS信息

#define RES_DEVICE_INFO					0xA0	//柜机请求坞托信息响应
#define RES_BATT_SN_BMS					0xA1	//电池响应SN及BMS信息

#define MAX_QUEUE_SIZE					(128)
#define MAX_QUEUE_ELEMENT_SIZE			(1024)
#define CHECKSUM_SIZE					(2)

#define MAX_FILE_PATH_SIZE				(128)
#define MD5_SIZE						(16)

#define DEVICE_ID_LEN (32+4)	// such as "1e6a6be3-7a58-43f6-9d51-29906a34de19"

#define STRUCT_PACKED __attribute__((packed))

typedef enum {
	SLAVE_1 = 1,
	SLAVE_2,
	SLAVE_3,
	SLAVE_4,
	SLAVE_5,
	SLAVE_6,
	SLAVE_7,
	SLAVE_8,
	SLAVE_9,
}SLAVE_ENUM;

//协议�?
typedef struct {
	unsigned char start;
	unsigned char slave;
	unsigned char type;
	unsigned char packet_id;
	unsigned int payload_len;
}STRUCT_PACKED MSG_UART_HEAD_T;

//协议数据�?
typedef struct {
	MSG_UART_HEAD_T head;
	void *		payload;
	unsigned char		checksum[CHECKSUM_SIZE];
}STRUCT_PACKED MSG_UART_PACKAGE_T;

/*********************REQ_BATT_SN_BMS/RES_BATT_SN_BMS***********************/
typedef struct{
	unsigned char opt;	//0：加密电池；1：不加密电池
}STRUCT_PACKED REQ_BATT_SN_BMS_T;

typedef struct{
	char code;		//0：ok; 1:fail
	unsigned char sn_len;		//sn 有效长度
	unsigned char sn[32]; 	//sn
	unsigned char Temp;		//温度: 16进制温度值（单位：摄氏度）+偏移量 40(避免负温度) 
	unsigned char Vol_H;	//电压：高位
	unsigned char Vol_L;	//电压：低位
	unsigned char ratio;	//16进制电量百分比(RC/FCC)值：5,10,20,30,40,50,60,70,80,90,100.
}STRUCT_PACKED RES_BATT_SN_BMS_T;

/*********************COMMON RES for exception***********************/
typedef struct{
	char code;		//��Ӧ���룺ok Ϊ0��failΪ1.
}STRUCT_PACKED RES_COMMON_T;

/*************************************************
Function: crc16
Description: CRC16校验码计�?
Input:
       ptr     内容
       count   长度
Output:
Return:                CRC16校验�?
Others:
*************************************************/
unsigned short crc16(char *ptr, int count);

bool ends_with(const char * haystack, const char * needle);

#endif
