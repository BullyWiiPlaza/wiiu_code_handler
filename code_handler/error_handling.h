#pragma once

#include <stdio.h>
#include "general/endian.h"

#define MESSAGE_BUFFER_SIZE 100

void set_error_message_buffer(char *base_error_message, unsigned char *codes) {
	snprintf(base_error_message, MESSAGE_BUFFER_SIZE, "%s at code line %08x %08x",
			 base_error_message, read_real_integer(codes), read_real_integer(codes + sizeof(int)));
}