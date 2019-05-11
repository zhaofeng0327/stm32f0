
#include "typedef.h"
#include "utils.h"

void dump_buffer(u8 * buffer,int lens)
{
#if 0
	int i;
	for(i = 0;i<lens;i++){
		if((i%16)==0)debug("\r\n");
		debug("%02x ",buffer[i]);
	}
	debug("\r\n");
#endif
}
