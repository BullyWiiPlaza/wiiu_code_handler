#include "code_handler_gecko_registers.h"

unsigned int integerRegisters[GECKO_REGISTERS_COUNT];
float floatRegisters[GECKO_REGISTERS_COUNT];

void initializeGeckoRegisters() {
	memset(integerRegisters, 0, sizeof(integerRegisters));
	memset(floatRegisters, 0, sizeof(floatRegisters));
}

int getAddressIncrement(enum ValueSize valueSize) {
	int valueIncrement = 0;
	if (isBigEndian()) {
		valueIncrement = sizeof(int);

		switch (valueSize) {
			case VALUE_SIZE_EIGHT_BIT:
				return valueIncrement - sizeof(char);

			case VALUE_SIZE_SIXTEEN_BIT:
				return valueIncrement - sizeof(short);

			case VALUE_SIZE_THIRTY_TWO_BIT:
				return valueIncrement - sizeof(int);
		}

		OSFatal("Value size unmatched");
		return -1;
	}

	return valueIncrement;
}

void loadInteger(const unsigned char *registerIndexPointer, enum ValueSize valueSize,
				 unsigned char *addressPointer) {
	int registerIndex = *registerIndexPointer;
	unsigned int value = readRealValue(valueSize, addressPointer);
	integerRegisters[registerIndex] = value;
}

void storeInteger(const unsigned char *registerIndexPointer,
				  enum ValueSize valueSize, unsigned char *pointerToAddress) {
	int addressIncrement = getAddressIncrement(valueSize);
	int registerIndex = *registerIndexPointer;
	unsigned char *targetValue = (unsigned char *) (integerRegisters + registerIndex);
	unsigned int value = 0;

	switch (valueSize) {
		case VALUE_SIZE_EIGHT_BIT:
			value = *(targetValue + addressIncrement);
			break;

		case VALUE_SIZE_SIXTEEN_BIT:
			value = *((unsigned short *) (targetValue + addressIncrement));
			break;

		case VALUE_SIZE_THIRTY_TWO_BIT:
			value = *((unsigned int *) (targetValue + addressIncrement));
			break;
	}

	unsigned char *addressPointer = (unsigned char *) (long) readRealInteger(pointerToAddress);
	log_printf("[STORE_INTEGER] Storing %p {Value Size: %i} at address %p...\n", (void *) (long) value,
			   getBytes(valueSize),
			   (void *) addressPointer);

	if (realMemoryAccessesAreEnabled) {
		*(int *) addressPointer = value;
	}
}

void loadFloat(const unsigned char *registerIndexPointer, unsigned char *addressPointer) {
	int registerIndex = *registerIndexPointer;
	unsigned int value = readRealValue(VALUE_SIZE_THIRTY_TWO_BIT, addressPointer);
	float floatValue;
	memcpy(&floatValue, &value, sizeof(int));
	floatRegisters[registerIndex] = floatValue;
	log_printf("Stored value: %.2f\n", floatRegisters[registerIndex]);
}

void storeFloat(const unsigned char *registerIndexPointer, unsigned char *pointerToAddress) {
	int registerIndex = *registerIndexPointer;
	float value = floatRegisters[registerIndex];
	unsigned char *addressPointer = (unsigned char *) (long) readRealInteger(pointerToAddress);
	log_printf("[STORE_FLOAT] Storing %.2f at address %p...\n", value, (void *) addressPointer);

	if (realMemoryAccessesAreEnabled) {
		*(float *) addressPointer = value;
	}
}

void applyFloatRegisterOperation(const unsigned char *firstRegisterIndexPointer,
								 const unsigned char *secondRegisterIndexPointer,
								 enum FloatRegisterOperation registerOperation) {
	int firstRegisterIndex = *firstRegisterIndexPointer;
	int secondRegisterIndex = *secondRegisterIndexPointer;
	float firstRegisterValue = floatRegisters[firstRegisterIndex];
	float secondRegisterValue = floatRegisters[secondRegisterIndex];

	switch (registerOperation) {
		case FLOAT_REGISTER_OPERATION_ADDITION:
			floatRegisters[firstRegisterIndex] = firstRegisterValue + secondRegisterValue;
			break;

		case FLOAT_REGISTER_OPERATION_SUBTRACTION:
			floatRegisters[firstRegisterIndex] = firstRegisterValue - secondRegisterValue;
			break;

		case FLOAT_REGISTER_OPERATION_MULTIPLICATION:
			floatRegisters[firstRegisterIndex] = firstRegisterValue * secondRegisterValue;
			break;

		case FLOAT_REGISTER_OPERATION_DIVISION:
			floatRegisters[firstRegisterIndex] = firstRegisterValue / secondRegisterValue;
			break;

		case FLOAT_REGISTER_OPERATION_FLOAT_TO_INTEGER:
			integerRegisters[secondRegisterIndex] = (unsigned int) firstRegisterValue;
			log_printf("Integer register value: %f converted -> %d\n", firstRegisterValue,
					   integerRegisters[secondRegisterIndex]);
			break;

		default:
			OSFatal("Invalid float register operation");
			break;
	}
}

void applyFloatDirectValueOperation(const unsigned char *firstRegisterIndexPointer,
									unsigned char *valuePointer,
									enum FloatDirectValueOperation directValueOperation) {
	int registerIndex = *firstRegisterIndexPointer;
	unsigned int currentValue = integerRegisters[registerIndex];
	unsigned int value = readRealInteger(valuePointer);

	switch (directValueOperation) {
		case FLOAT_DIRECT_VALUE_OPERATION_ADDITION:
			integerRegisters[registerIndex] = currentValue + value;
			break;

		case FLOAT_DIRECT_VALUE_OPERATION_SUBTRACTION:
			integerRegisters[registerIndex] = currentValue - value;
			break;

		case FLOAT_DIRECT_VALUE_OPERATION_DIVISION:
			integerRegisters[registerIndex] = currentValue / value;
			break;

		case FLOAT_DIRECT_VALUE_OPERATION_MULTIPLICATION:
			integerRegisters[registerIndex] = currentValue * value;
			break;

		default:
			OSFatal("Invalid direct float value operation");
			break;
	}
}

void applyIntegerRegisterOperation(const unsigned char *firstRegisterIndexPointer,
								   const unsigned char *secondRegisterIndexPointer,
								   enum IntegerRegisterOperation registerOperation) {
	int firstRegisterIndex = *firstRegisterIndexPointer;
	int secondRegisterIndex = *secondRegisterIndexPointer;
	unsigned int firstRegisterValue = integerRegisters[firstRegisterIndex];
	unsigned int secondRegisterValue = integerRegisters[secondRegisterIndex];

	switch (registerOperation) {
		case INTEGER_REGISTER_OPERATION_ADDITION:
			integerRegisters[firstRegisterIndex] = firstRegisterValue + secondRegisterValue;
			break;

		case INTEGER_REGISTER_OPERATION_SUBTRACTION:
			integerRegisters[firstRegisterIndex] = firstRegisterValue - secondRegisterValue;
			break;

		case INTEGER_REGISTER_OPERATION_MULTIPLICATION:
			integerRegisters[firstRegisterIndex] = firstRegisterValue * secondRegisterValue;
			break;

		case INTEGER_REGISTER_OPERATION_DIVISION:
			integerRegisters[firstRegisterIndex] = firstRegisterValue / secondRegisterValue;
			break;

		default:
			OSFatal("Invalid integer register operation");
			break;
	}
}

void applyIntegerDirectValueOperation(const unsigned char *firstRegisterIndexPointer,
									  unsigned char *valuePointer,
									  enum IntegerDirectValueOperation directValueOperation) {
	int registerIndex = *firstRegisterIndexPointer;
	unsigned int currentValue = integerRegisters[registerIndex];
	unsigned int value = readRealInteger(valuePointer);

	switch (directValueOperation) {
		case INTEGER_DIRECT_VALUE_OPERATION_ADDITION:
			integerRegisters[registerIndex] = currentValue + value;
			break;

		case INTEGER_DIRECT_VALUE_OPERATION_SUBTRACTION:
			integerRegisters[registerIndex] = currentValue - value;
			break;

		case INTEGER_DIRECT_VALUE_OPERATION_MULTIPLICATION:
			integerRegisters[registerIndex] = currentValue * value;
			break;

		case INTEGER_DIRECT_VALUE_OPERATION_DIVISION:
			integerRegisters[registerIndex] = currentValue / value;
			break;

		default:
			OSFatal("Invalid integer direct value operation");
			break;
	}
}