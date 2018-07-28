#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "code_handler_enumerations.h"

extern bool realMemoryAccessesAreEnabled;

unsigned long callAddress(uintptr_t realAddress, const unsigned int *arguments);

uintptr_t searchForTemplate(const unsigned char *searchTemplate, unsigned int length,
							uintptr_t address, uintptr_t endAddress,
							int stepSize, unsigned short targetMatchIndex,
							bool *templateFound);

void skipWriteMemory(unsigned char *address, unsigned char *value,
					 int valueLength, unsigned char *stepSize,
					 unsigned char *increment, unsigned char *iterationsCount);

void writeString(unsigned char *address, unsigned char *value, int valueLength);

void writeValue(unsigned char *address, const unsigned char *value, int valueLength);

bool compareValue(unsigned char *address, unsigned char *value, const enum ValueSize *valueSize,
				  unsigned char *upperValue, int valueLength, enum ComparisonType comparisonType);

void executeAssembly(unsigned char *instructions, unsigned int size);

void applyCorrupter(unsigned char *beginning, unsigned char *end, unsigned char *searchValue,
					unsigned char *replacementValue);

unsigned int readRealValue(enum ValueSize valueSize, unsigned char *pointerToAddress);

uintptr_t loadPointer(unsigned char *address, unsigned char *value);