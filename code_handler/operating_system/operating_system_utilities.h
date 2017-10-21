#pragma once

#include <stdio.h>
#include <memory.h>

#define ADDRESSES_SHIFT 0x1338

void kernelCopyData(unsigned char *destination, unsigned char *source, int size);

void OSFatal(const char *message);

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define log_print(string) printf("[%s] %s@L%d: " string,  __FILENAME__, __func__, __LINE__)

#define log_printf(formatString, arguments...) printf("[%s] %s@L%d: " formatString,  __FILENAME__, __func__, __LINE__, ##arguments)