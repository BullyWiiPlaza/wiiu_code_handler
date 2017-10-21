#include "code_handler_functions.h"

#include "general/endian.h"

bool enableRealMemoryAccesses = false;

void incrementValue(int valueLength, unsigned int realIncrement, unsigned char *modifiableValue) {
	switch (valueLength) {
		case sizeof(char):
			modifiableValue[3] += realIncrement;
			break;

		case sizeof(short):;
			short incrementedShortValue = readRealShort(modifiableValue);
			incrementedShortValue += realIncrement;
			incrementedShortValue = readRealShort((unsigned char *) &incrementedShortValue);
			memcpy(&modifiableValue[sizeof(int) - sizeof(short)], &incrementedShortValue, sizeof(short));
			break;

		case sizeof(int):;
			int incrementedIntValue = readRealInteger(modifiableValue);
			incrementedIntValue += realIncrement;
			incrementedIntValue = readRealInteger((unsigned char *) &incrementedIntValue);
			memcpy(&modifiableValue[sizeof(int) - sizeof(int)], &incrementedIntValue, sizeof(int));
			break;

		default:
			OSFatal("Unhandled value length");
	}
}

void skipWriteMemory(unsigned char *address, unsigned char *value,
					 int valueLength, unsigned char *stepSize,
					 unsigned char *increment, unsigned char *iterationsCount) {
	unsigned int realAddress = readRealInteger(address);
	unsigned short realIterationsCount = readRealShort(iterationsCount);
	unsigned int realIncrement = readRealInteger(increment);
	unsigned int realStepSize = readRealInteger(stepSize);
	unsigned char modifiableValue[sizeof(int)];
	memcpy(modifiableValue, value, sizeof(int));

	for (int iterationIndex = 0; iterationIndex < realIterationsCount; iterationIndex++) {
		int byteIndex = sizeof(int) - valueLength;
		for (; byteIndex < sizeof(int); byteIndex++) {
			unsigned int *targetAddress = (unsigned int *) (((unsigned char *) realAddress) + byteIndex);
			unsigned char byte = modifiableValue[byteIndex];
			log_printf("[SKIP_WRITE]: *%p = 0x%x\n", (void *) targetAddress, byte);

			if (enableRealMemoryAccesses) {
				*targetAddress = byte;
			}
		}

		incrementValue(valueLength, realIncrement, modifiableValue);
		realAddress += realStepSize;
	}
}

void writeString(unsigned char *address, unsigned char *value, int valueLength) {
	unsigned int realAddress = readRealInteger(address);
	log_printf("[STRING_WRITE]: *%p = 0x%x {Length: %i}\n", (void *) realAddress, *(unsigned int *) value, valueLength);

	if (enableRealMemoryAccesses) {
		memcpy(address, value, (size_t) valueLength);
	}
}

void writeValue(unsigned char *address, const unsigned char *value, int valueLength) {
	int byteIndex = sizeof(int) - valueLength;
	for (; byteIndex < sizeof(int); byteIndex++) {
		unsigned int *targetAddress = (unsigned int *) (((unsigned char *) readRealInteger(address)) + byteIndex);
		unsigned char byte = value[byteIndex];
		log_printf("[RAM_WRITE]: *%p = 0x%x\n", (void *) targetAddress, byte);

		if (enableRealMemoryAccesses) {
			*targetAddress = byte;
		}
	}
}

bool compareValue(unsigned char *address, unsigned char *value,
				  const enum ValueSize *valueSize, unsigned char *upperValue,
				  int valueLength, enum ComparisonType comparisonType) {
	unsigned int realAddress = readRealInteger(address);
	unsigned int realValue = readRealInteger(value);
	unsigned int realUpperLimit = readRealInteger(upperValue);

	if (comparisonType != CODE_TYPE_IF_VALUE_BETWEEN) {
		log_printf("[COMPARE]: *%p {comparison %i} %p {Last %i bytes}\n", (void *) realAddress, comparisonType,
				   (void *) realValue, valueLength);
	}

	unsigned int addressValue = 0;

	switch (valueLength) {
		case sizeof(int):
			if (enableRealMemoryAccesses) {
				addressValue = readRealInteger((unsigned char *) realAddress);
			}

			realValue = readRealInteger(value + sizeof(int) - sizeof(int));
			break;

		case sizeof(short):
			if (enableRealMemoryAccesses) {
				addressValue = readRealShort((unsigned char *) realAddress);
			}

			realValue = readRealShort(value + sizeof(int) - sizeof(short));
			break;

		case sizeof(char):
			if (enableRealMemoryAccesses) {
				addressValue = readRealByte((unsigned char *) realAddress);
			}

			realValue = readRealByte(value + sizeof(int) - sizeof(char));
			break;

		default:
			OSFatal("Unhandled value size");
	}

	bool bitOperatorResult = 0;

	switch (comparisonType) {
		case COMPARISON_TYPE_AND:
			bitOperatorResult = (addressValue & realValue) == 0;
			break;

		case COMPARISON_TYPE_OR:
			bitOperatorResult = (addressValue | realValue) == 0;
			break;

		default:
			break;
	}

	bool comparisonResult = 0;

	if (enableRealMemoryAccesses) {
		comparisonResult = memcmp((void *) realAddress, (void *) value, (size_t) valueLength) == 0;
	}

	switch (comparisonType) {
		case COMPARISON_TYPE_EQUAL:
			return comparisonResult == 0;

		case COMPARISON_TYPE_NOT_EQUAL:
			return comparisonResult != 0;

		case COMPARISON_TYPE_GREATER_THAN:
			return comparisonResult > 0;

		case COMPARISON_TYPE_LESS_THAN:
			return comparisonResult < 0;

		case COMPARISON_TYPE_GREATER_THAN_OR_EQUAL:
			return (comparisonResult > 0) || (comparisonResult == 0);

		case COMPARISON_TYPE_LESS_THAN_OR_EQUAL:
			return (comparisonResult < 0) || (comparisonResult == 0);

		case COMPARISON_TYPE_AND:
			return bitOperatorResult;

		case COMPARISON_TYPE_OR:
			return bitOperatorResult;

		case COMPARISON_TYPE_VALUE_BETWEEN:;
			unsigned int readValue = readRealValue(*valueSize, address);
			unsigned int lowerLimit = realValue;
			log_printf("[COMPARE]: (*%p > %p) && (*%p < %p) {Last %i bytes}\n", (void *) realAddress,
					   (void *) lowerLimit, (void *) realAddress, (void *) realUpperLimit, valueLength);
			return readValue > lowerLimit && readValue < realUpperLimit;

		default:
			OSFatal("Unhandled comparison type");
	}
}

void executeAssembly(unsigned char *instructions, int size) {
	unsigned int realInstructions = readRealInteger(instructions);
	log_printf("[EXECUTE_ASSEMBLY]: 0x%08x... {Size: %i}\n", realInstructions, size);

	if (enableRealMemoryAccesses) {
		// Write the assembly to an executable code region
		int destinationAddress = 0x10000000 - size;
		kernelCopyData((unsigned char *) destinationAddress, instructions, size);

		// Execute the assembly from there
		void (*function)() = (void (*)()) destinationAddress;
		function();

		// Clear the memory contents again
		memset((void *) instructions, 0, (size_t) size);
		kernelCopyData((unsigned char *) destinationAddress, instructions, size);
	}
}

void applyCorrupter(unsigned char *beginning, unsigned char *end, unsigned char *searchValue,
					unsigned char *replacementValue) {
	unsigned int *startAddress = (unsigned int *) readRealInteger(beginning);
	unsigned int *endAddress = (unsigned int *) readRealInteger(end);
	unsigned int realSearchValue = readRealInteger(searchValue);
	unsigned int realReplacementValue = readRealInteger(replacementValue);

	log_print("[CORRUPTER] Replacing values...\n");

	unsigned int *currentAddress = startAddress;
	for (; currentAddress < endAddress; currentAddress++) {
		unsigned int readValue = 0;

		log_printf("Reading value from address %p...\n", currentAddress);
		if (enableRealMemoryAccesses) {
			readValue = *currentAddress;
		}

		log_printf("Read value is: %08x\n", readValue);

		if (readValue == realSearchValue) {
			log_printf("Replacing with %08x...\n", realReplacementValue);
			if (enableRealMemoryAccesses) {
				*currentAddress = realReplacementValue;
			}
		} else {
			log_print("Value differs...\n");
		}
	}
}

unsigned int readRealValue(enum ValueSize valueSize, unsigned char *pointerToAddress) {
	unsigned char *addressPointer = (unsigned char *) readRealInteger(pointerToAddress);
	log_printf("[GET_VALUE] Getting value from %p {Value Size: %i}...\n", (void *) addressPointer, getBytes(valueSize));

	if (enableRealMemoryAccesses) {
		switch (valueSize) {
			case VALUE_SIZE_EIGHT_BIT:
				return readRealByte(addressPointer);

			case VALUE_SIZE_SIXTEEN_BIT:
				return readRealShort(addressPointer);

			case VALUE_SIZE_THIRTY_TWO_BIT:
				return readRealInteger(addressPointer);
		}

		OSFatal("Unmatched value size");

		// Dummy return, will never execute
		return 0;
	}

	// For PC testing only
	return 0x43219876;
}

unsigned int loadPointer(unsigned char *address, unsigned char *value) {
	unsigned int realAddress = readRealInteger(address);
	unsigned int rangeStartingAddress = readRealInteger(value);
	unsigned int rangeEndingAddress = readRealInteger(value + sizeof(int));
	unsigned int deReferenced = 0;

	if (enableRealMemoryAccesses) {
		deReferenced = *(unsigned int *) realAddress;
	}

	log_printf("[LOAD_POINTER] Is %08x between %08x and %08x?\n", deReferenced, rangeStartingAddress,
			   rangeEndingAddress);

	if (deReferenced >= rangeStartingAddress && deReferenced <= rangeEndingAddress) {
		log_print("[LOAD_POINTER] Yes, pointer cached\n");
		return realAddress;
	} else {
		log_print("[LOAD_POINTER] No, pointer NOT cached\n");
		return 0;
	}
}