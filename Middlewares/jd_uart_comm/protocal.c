#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <sys/stat.h>
#include "typedef.h"
#include "jd_os_middleware.h"

unsigned short crc16(char *ptr, int count)
{
	unsigned short crc;
	char i;

	crc = 0;
	while (--count >= 0) {
		crc = crc ^ (int)*ptr++ << 8;
		i = 8;
		do {
			if (crc & 0x8000)
				crc = crc << 1 ^ 0x1021;
			else
				crc = crc << 1;
		} while (--i);
	}

	return crc;
}

bool ends_with(const char * haystack, const char * needle)
{
	const char * end;
	int nlen = strlen(needle);
	int hlen = strlen(haystack);

	if( nlen > hlen )
		return pdFALSE;
	end = haystack + hlen - nlen;

	return (strcasecmp(end, needle) ? pdFALSE : pdTRUE);
}


