#ifndef RDT_COMMON_H
#define RDT_COMMON_H

#include <arpa/inet.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define MSS 1024
#define WINDOW_SIZE 5
#define TIMEOUT_SEC 0
#define TIMEOUT_USEC 500000

#define FLAG_DATA 0
#define FLAG_ACK 1
#define FLAG_FIN 2

struct Packet
{
	uint32_t seq_num;
	uint32_t ack_num;
	uint8_t flags;
	uint16_t data_size;
	char data[MSS];
};

inline void log_debug(const bool debug_mode, const char* format, ...)
{
	if (!debug_mode)
		return;
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
}

#endif