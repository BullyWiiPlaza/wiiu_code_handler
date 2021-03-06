#pragma once

#include <stdio.h>
#include <memory.h>

// #define ADDRESSES_SHIFT 0x1338

void kernel_copy_data(unsigned char *destination, unsigned char *source, int size);

void OSFatal(const char *message);

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define log_print(string) printf("[%s] %s@L%d: " string,  __FILENAME__, __func__, __LINE__)

#if defined(WIN32)

// TODO Fix for Microsoft compilers
#define log_printf(format_string, arguments...) printf(format_string, ##arguments)

#else

#define log_printf(format_string, arguments...) printf("[%s] %s@L%d: " format_string,  __FILENAME__, __func__, __LINE__, ##arguments)

#endif