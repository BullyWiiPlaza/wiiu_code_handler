#pragma once

#include <stdbool.h>
#include "code_handler_enumerations.h"

extern bool enableRealMemoryAccesses;

void skipWriteMemory(unsigned char *address, unsigned char *value,
					 int valueLength, unsigned char *stepSize,
					 unsigned char *increment, unsigned char *iterationsCount);

void writeString(unsigned char *address, unsigned char *value, int valueLength);

void writeValue(unsigned char *address, const unsigned char *value, int valueLength);

bool compareValue(unsigned char *address, unsigned char *value, const enum ValueSize *valueSize,
				  unsigned char *upperValue, int valueLength, enum ComparisonType comparisonType);

void executeAssembly(unsigned char *instructions, int size);

void applyCorrupter(unsigned char *beginning, unsigned char *end, unsigned char *searchValue,
					unsigned char *replacementValue);

unsigned int readRealValue(enum ValueSize valueSize, unsigned char *pointerToAddress);

unsigned int loadPointer(unsigned char *address, unsigned char *value);