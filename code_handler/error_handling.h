#pragma once

#include <stdio.h>
#include "general/endian.h"

#define MESSAGE_BUFFER_SIZE 100

void setErrorMessageBuffer(char *baseErrorMessage, unsigned char *codes) {
	snprintf(baseErrorMessage, MESSAGE_BUFFER_SIZE, "%s at code line %08x %08x",
			 baseErrorMessage, readRealInteger(codes), readRealInteger(codes + sizeof(int)));
}