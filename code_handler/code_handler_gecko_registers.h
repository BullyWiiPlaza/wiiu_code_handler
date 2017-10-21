#pragma once

#include "code_handler_functions.h"
#include "general/endian.h"

#define GECKO_REGISTERS_COUNT 8

extern unsigned int integerRegisters[GECKO_REGISTERS_COUNT];
extern float floatRegisters[GECKO_REGISTERS_COUNT];

void initializeGeckoRegisters();

void loadInteger(const unsigned char *registerIndexPointer, enum ValueSize valueSize,
				 unsigned char *addressPointer);

void storeInteger(const unsigned char *registerIndexPointer,
				  enum ValueSize valueSize, unsigned char *pointerToAddress);

void loadFloat(const unsigned char *registerIndexPointer, unsigned char *addressPointer);

void storeFloat(const unsigned char *registerIndexPointer, unsigned char *pointerToAddress);

void applyFloatRegisterOperation(const unsigned char *firstRegisterIndexPointer,
								 const unsigned char *secondRegisterIndexPointer,
								 enum FloatRegisterOperation registerOperation);

void applyFloatDirectValueOperation(const unsigned char *firstRegisterIndexPointer,
									unsigned char *valuePointer,
									enum FloatDirectValueOperation directValueOperation);

void applyIntegerRegisterOperation(const unsigned char *firstRegisterIndexPointer,
								   const unsigned char *secondRegisterIndexPointer,
								   enum IntegerRegisterOperation registerOperation);

void applyIntegerDirectValueOperation(const unsigned char *firstRegisterIndexPointer,
									  unsigned char *valuePointer,
									  enum IntegerDirectValueOperation directValueOperation);