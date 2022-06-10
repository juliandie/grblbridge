
#ifndef LIB_LOG_H_
#define LIB_LOG_H_

#include <stdio.h> // printf
#include <string.h> // strerror
#include <errno.h> // errno
#include <time.h> // timespec, clock_gettime

#define _UNUSED __attribute__((unused))

#define LIB_LOG_LVL_DEBUG	(7)
#define LIB_LOG_LVL_INFO	(6)
#define LIB_LOG_LVL_WARN	(4)
#define LIB_LOG_LVL_ERR		(3)

#ifndef LIB_LOG_LEVEL
#define LIB_LOG_LEVEL (LIB_LOG_LVL_DEBUG)
#endif

#define LIB_LOG(fmt, ...) \
	{printf(fmt, ##__VA_ARGS__);}

#define LIB_LOG_DBG(fmt, ...) \
	{ if(LIB_LOG_LEVEL >= LIB_LOG_LVL_DEBUG){ \
		struct timespec tp; \
		clock_gettime(CLOCK_REALTIME, &tp); \
		printf("[DBG] %lu %s(%s):%d -- "fmt"\n", \
			((tp.tv_sec * 1000000000ul) + tp.tv_nsec) / 1000, \
			__func__, __FILE__, __LINE__, ##__VA_ARGS__); \
			fflush(stdout);} }

#define LIB_LOG_INFO(fmt, ...) \
	{ if(LIB_LOG_LEVEL>=LIB_LOG_LVL_INFO){printf("[INFO] %s "fmt"\n", \
		__func__, ##__VA_ARGS__);} }

#define LIB_LOG_WARN(fmt, ...) \
	{ if(LIB_LOG_LEVEL>=LIB_LOG_LVL_WARN){printf("[WARN] %s "fmt"\n", \
		__func__, ##__VA_ARGS__);} }

#define LIB_LOG_ERR(fmt, ...) \
	{ if(LIB_LOG_LEVEL>=LIB_LOG_LVL_ERR){printf("[ERR] %s "fmt": %s (%d)\n", \
		__func__, ##__VA_ARGS__, strerror(errno), errno);} }

void lib_dump(const void* p, size_t size, const char* prefix, ...);
void lib_hexdump(const void* p, size_t size, const char* prefix, ...);

#endif