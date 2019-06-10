/*
 * typedef
 * 2017-09-18
 * 盛耀微科技
 * wxj@glinkwin.com
 */



#ifndef __TYPEDEF_HH__
#define __TYPEDEF_HH__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define LLC_UART	0
#define LLC_UDP		1

#define MALLOC_EN	1
#define LLC_TYPE	LLC_UART

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define  configDebugPrintf   0

#if   (1 == configDebugPrintf)
#define dberr print_usart
#define dbinfo print_usart
#define debug print_usart
#else
#define dberr
#define dbinfo
#define debug
#endif

#define EXPORT_API

#endif
