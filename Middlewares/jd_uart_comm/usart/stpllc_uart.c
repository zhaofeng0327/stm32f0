/*
 * uart hw link layer
 * 2017-09-18
 * 盛耀微科技
 * wxj@glinkwin.com
 * Serial transfer protocol=STP
 * OS:linux
 *
 *
 * 串口传输采用base64 编码,0xAA 开始，0xA5结束
 * llc frame format:
 * sync |frame type| playload | eof |
 * 0xAA    0~0x09     data     0xA5
 *
 * frame type
 * 0x05: tlc layer data
 * 0x00: llc layer data
 */

#include "stpllc_uart.h"
#include "base64.h"
#include "utils.h"
#include "jd_os_middleware.h"


#if( LLC_TYPE == LLC_UART )

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#define SYNCHD 	0xAA
#define SYNCEOF 0xA5

#define TLC_FLAG 0x05
#define LLC_FLAG 0x00

static int uart_read(void *fd,u8 * buf, int lens) {
	return jz_uart_read_ex(fd, buf, lens,0);
}

static int uart_write(void *fd,u8 * buf, int lens) {
	int ret = jz_uart_write_ex(fd, buf, lens,UartTimeOut(3000)/*3 second*/);
	return ret;
}

static void *uart_open(int usart_no)
{
	return jz_uart_init_ex(usart_no);
}

static void uart_close(void *fd){
	jz_uart_close_ex(fd);
}

static int stpllc_req_dev_name_by_AT(u8 _char)
{
	const char ReqNameByAt[9]={'A','T','+','N','A','M','E','?','\r'};
	static bool check_AT = pdFALSE;
	static u8 check_cnt = 0;
	int ret=0;
	//debug("%s:the %d char is '%c'\r\n",__func__,char_num,(char)_char);
	if(check_cnt == 0 && ReqNameByAt[check_cnt] == _char){
		check_AT = pdTRUE;
		check_cnt++;
	}
	else if(check_AT && (ReqNameByAt[check_cnt] == _char)){
		//debug("%s:at comm check %d times\r\n",__func__,check_cnt);
		if((check_cnt+1) >= sizeof(ReqNameByAt)){
			ret = 1;
			check_AT = pdFALSE;
			check_cnt = 0;
		}
		else
			check_cnt++;
	}
	else{
		check_AT = pdFALSE;
		check_cnt = 0;
	}
	return ret;
}

void stpllc_init(stpllc * hw){
	hw->frame_lens = 0;
	hw->cache_lens = 0;
}

int stpllc_read(stpllc * hw,u8 * pdata,int lens) {
	int dlens = 0;
	u8 * buf = hw->pcache;
	u8 * frame = hw->pframerx;

	if( hw->cache_lens < 1 ){
		int retl = uart_read(hw->fd,buf,LLC_PACK_SZ_MAX);
		if( retl > 0 ){
			hw->cache_lens = retl;
		}	
	}

	// debug("recv raw --- \r\n");

	if ( hw->cache_lens > 0) {

		int i;
		for (i = 0; i < hw->cache_lens;) {

			frame[hw->frame_lens++] = buf[i++];
			
			if(stpllc_req_dev_name_by_AT(buf[i-1])){
				debug("%s:get req dev name by at command\r\n",__func__);
				#if 1
				uart_write(hw->fd,ResNameByAT,strlen(ResNameByAT));
				stpllc_init(hw);
				return 0;
				#else
				memcpy(pdata,ReqNameByAT,strlen(ReqNameByAT));
				return (int)strlen(ReqNameByAT);
				#endif
			}
			
			if ( buf[i-1] == SYNCHD ) {
				hw->frame_lens = 1;
			} else if (buf[i-1] == SYNCEOF) {
				
				if ( hw->frame_lens > 3 ) {

					if( lens >= (hw->frame_lens-3 ) ){//必须要能装下
						switch ( frame[1] ) {
						case TLC_FLAG:
							/*一个序列里面只可能只有一个包*/
							dlens = base64_dec((char*)pdata, frame + 2,hw->frame_lens - 3 );
							if ( dlens > 0 ) {
								if( (hw->cache_lens-i) > 0 ){
									memmove(buf,buf+i,hw->cache_lens-i);
								}
								hw->cache_lens -= i;
								hw->frame_lens = 0;
								return dlens;
							}else{
								dberr(">>>>>>>>>>>>>>>>>>>base64_dec error:%d\r\n",hw->frame_lens);
							}
							break;
						case LLC_FLAG:
							debug("LLC_FLAG\r\n");
							break;
						default:
							dberr("unknow data:%02x\r\n",frame[0]);
							break;
						}
					}else{
						dberr("llc frame lens to long\r\n");
					}
				}else{
					dberr("lens < 3 \r\n");
				}
				hw->frame_lens = 0;
			}
		}
		hw->cache_lens = 0;
	}
	return 0;
}

int stpllc_write(stpllc * hw, u8 * buffer, int lens) {
	int flens = base64_enc((char*)hw->pframe + 2, buffer, lens);
	hw->pframe[0] 		= SYNCHD;
	hw->pframe[1] 		= TLC_FLAG;
	hw->pframe[flens+2] = SYNCEOF;

	return uart_write(hw->fd,hw->pframe,flens+3);
}

int stpllc_write_raw(stpllc * hw, u8 * buffer, int lens) {
	return uart_write(hw->fd, buffer, lens);
}
int stpllc_read_raw(stpllc * hw,u8 * pdata,int lens) {
	return uart_read(hw->fd,pdata,lens);
}

stpllc * stpllc_open(int hwport, int usart_no){
	stpllc * llc;
#if( MALLOC_EN == 1 )
	llc = (stpllc *)jd_om_malloc(sizeof(stpllc));
#else
	static stpllc g_llc;
	llc = &g_llc;
#endif
	if(!llc)goto error;
	memset(llc,0,sizeof(stpllc));
	llc->hwport	  = hwport;
	llc->frame_sz = LLC_PACK_SZ_MAX;
#if( MALLOC_EN == 1 )
	llc->pcache = jd_om_malloc(LLC_PACK_SZ_MAX*2);
	llc->pframe = jd_om_malloc(LLC_PACK_SZ_MAX*2);
	llc->pframerx = jd_om_malloc(LLC_PACK_SZ_MAX*2);
#else
	static u8 buffer[LLC_PACK_SZ_MAX*2];
	static u8 bufferrx[LLC_PACK_SZ_MAX*2];
	llc->pframe = buffer;
	llc->pframerx = bufferrx;
#endif
	llc->fd = uart_open(usart_no);
	if( llc->fd == NULL ){
		goto error;
	}
	return llc;
error:
#if( MALLOC_EN == 1 )
	if( llc ){
		if( llc->pframe ){
			jd_om_free(llc->pframe );
		}
		if( llc->pframerx )
			jd_om_free(llc->pframerx );
		jd_om_free(llc);
	}
#endif
	return NULL;
}

int stpllc_close(stpllc * pllc){

	uart_close(pllc->fd);
#if( MALLOC_EN == 1 )
	jd_om_free(pllc->pcache);
	jd_om_free(pllc->pframe);
	jd_om_free(pllc->pframerx );
	jd_om_free(pllc);
#endif

	return 0;
}

#endif
