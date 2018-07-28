#include "code_handler_functions.h"

#include "general/endian.h"

bool realMemoryAccessesAreEnabled = false;

unsigned long callAddress(uintptr_t realAddress, const unsigned int *arguments) {
	typedef unsigned long functionType(int, int, int, int, int, int, int, int);
	functionType *function = (functionType *) realAddress;
	unsigned long returnValue = 0x1111111122222222;

	if (realMemoryAccessesAreEnabled) {
		// We count instead of incrementing an index variable because: operation on 'argumentIndex' may be undefined
		returnValue = function(arguments[0], arguments[1],
							   arguments[2], arguments[3],
							   arguments[4], arguments[5],
							   arguments[6], arguments[7]);
	}

	return returnValue;
}

uintptr_t searchForTemplate(const unsigned char *searchTemplate, unsigned int length,
							uintptr_t address, uintptr_t endAddress,
							int stepSize, unsigned short targetMatchIndex,
							bool *templateFound) {
	int matchesCount = 0;
	for (uintptr_t currentAddress = address; currentAddress < endAddress; currentAddress += stepSize) {
		int comparisonResult = 0;

		if (realMemoryAccessesAreEnabled) {
			comparisonResult = memcmp((const void *) currentAddress, (const void *) searchTemplate, (size_t) length);
		}

		if (comparisonResult == 0) {
			matchesCount++;
		}

		if ((matchesCount - 1) == targetMatchIndex) {
			*templateFound = true;
			return currentAddress;
		}
	}

	*templateFound = false;
	return 0;
}

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
	uintptr_t realAddress = readRealInteger(address);
	unsigned short realIterationsCount = readRealShort(iterationsCount);
	unsigned int realIncrement = readRealInteger(increment);
	unsigned int realStepSize = readRealInteger(stepSize);
	unsigned char modifiableValue[sizeof(int)];
	memcpy(modifiableValue, value, sizeof(int));

	for (int iterationIndex = 0; iterationIndex < realIterationsCount; iterationIndex++) {
		unsigned int byteIndex = sizeof(int) - valueLength;
		for (; byteIndex < sizeof(int); byteIndex++) {
			unsigned int *targetAddress = (unsigned int *) (((unsigned char *) realAddress) + byteIndex);
			unsigned char byte = modifiableValue[byteIndex];
			log_printf("[SKIP_WRITE]: *%p = 0x%x\n", (void *) targetAddress, byte);

			if (realMemoryAccessesAreEnabled) {
				*targetAddress = byte;
			}
		}

		incrementValue(valueLength, realIncrement, modifiableValue);
		realAddress += realStepSize;
	}
}

void writeString(unsigned char *address, unsigned char *value, int valueLength) {
	uintptr_t realAddress = readRealInteger(address);
	log_printf("[STRING_WRITE]: *%p = 0x%x {Length: %i}\n", (void *) realAddress, *(unsigned int *) value, valueLength);

	if (realMemoryAccessesAreEnabled) {
		memcpy(address, value, (size_t) valueLength);
	}
}

void writeValue(unsigned char *address, const unsigned char *value, int valueLength) {
	unsigned int byteIndex = sizeof(int) - valueLength;
	for (; byteIndex < sizeof(int); byteIndex++) {
		uintptr_t targetAddress = (((uintptr_t) readRealInteger(address)) + byteIndex);
		unsigned char byte = value[byteIndex];
		log_printf("[RAM_WRITE]: *%p = 0x%x\n", (void *) targetAddress, byte);

		if (realMemoryAccessesAreEnabled) {
			*(unsigned char *) targetAddress = byte;
		}
	}
}

bool compareValue(unsigned char *address, unsigned char *value,
				  const enum ValueSize *valueSize, unsigned char *upperValue,
				  int valueLength, enum ComparisonType comparisonType) {
	uintptr_t realAddress = readRealInteger(address);
	uintptr_t realValue = readRealInteger(value);
	uintptr_t realUpperLimit = readRealInteger(upperValue);

	if (comparisonType != COMPARISON_TYPE_VALUE_BETWEEN) {
		log_printf("[COMPARE]: *%p {comparison %i} %p {Last %i bytes}\n", (void *) realAddress, comparisonType,
				   (void *) realValue, valueLength);
	}

	unsigned int addressValue = 0;

	switch (valueLength) {
		case sizeof(int):
			if (realMemoryAccessesAreEnabled) {
				addressValue = readRealInteger((unsigned char *) realAddress);
			}

			realValue = readRealInteger(value + sizeof(int) - sizeof(int));
			break;

		case sizeof(short):
			if (realMemoryAccessesAreEnabled) {
				addressValue = readRealShort((unsigned char *) realAddress);
			}

			realValue = readRealShort(value + sizeof(int) - sizeof(short));
			break;

		case sizeof(char):
			if (realMemoryAccessesAreEnabled) {
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

	int comparisonResult = 0;

	if (realMemoryAccessesAreEnabled) {
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
			uintptr_t lowerLimit = realValue;
			log_printf("[COMPARE]: (*%p > %p) && (*%p < %p) {Last %i bytes}\n", (void *) realAddress,
					   (void *) lowerLimit, (void *) realAddress, (void *) realUpperLimit, valueLength);
			return readValue > lowerLimit && readValue < realUpperLimit;

		default:
			OSFatal("Unhandled comparison type");

			// Dummy return
			return false;
	}
}

void executeAssembly(unsigned char *instructions, unsigned int size) {
	unsigned int realInstructions = readRealInteger(instructions);
	log_printf("[EXECUTE_ASSEMBLY]: 0x%08x... {Size: %i}\n", realInstructions, size);

	if (realMemoryAccessesAreEnabled) {
		// Write the assembly to an executable code region
		uintptr_t destinationAddress = (0x10000000 - size);
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
	uintptr_t startAddress = readRealInteger(beginning);
	uintptr_t endAddress = readRealInteger(end);
	unsigned int realSearchValue = readRealInteger(searchValue);
	unsigned int realReplacementValue = readRealInteger(replacementValue);

	log_print("[CORRUPTER] Replacing values...\n");

	uintptr_t currentAddress = startAddress;
	for (; currentAddress < endAddress; currentAddress += sizeof(int)) {
		uintptr_t readValue = 0;

		log_printf("Reading value from address %p...\n", (void *) currentAddress);
		if (realMemoryAccessesAreEnabled) {
			readValue = currentAddress;
		}

		log_printf("Read value is: %p\n", (void *) readValue);

		if (readValue == realSearchValue) {
			log_printf("Replacing with %08x...\n", realReplacementValue);
			if (realMemoryAccessesAreEnabled) {
				currentAddress = realReplacementValue;
			}
		} else {
			log_print("Value differs...\n");
		}
	}
}

unsigned int readRealValue(enum ValueSize valueSize, unsigned char *pointerToAddress) {
	unsigned char *addressPointer = (unsigned char *) (uintptr_t) readRealInteger(pointerToAddress);
	log_printf("[GET_VALUE] Getting value from %p {Value Size: %i}...\n", (void *) addressPointer, getBytes(valueSize));

	if (realMemoryAccessesAreEnabled) {
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

uintptr_t loadPointer(unsigned char *address, unsigned char *value) {
	uintptr_t realAddress = readRealInteger(address);
	unsigned int rangeStartingAddress = readRealInteger(value);
	unsigned int rangeEndingAddress = readRealInteger(value + sizeof(int));
	uintptr_t deReferenced = 0;

	if (realMemoryAccessesAreEnabled) {
		deReferenced = *(uintptr_t *) realAddress;
	}

	log_printf("[LOAD_POINTER] Is %p between %08x and %08x?\n", (void *) deReferenced, rangeStartingAddress,
			   rangeEndingAddress);

	if (deReferenced >= rangeStartingAddress && deReferenced <= rangeEndingAddress) {
		log_print("[LOAD_POINTER] Yes, pointer cached\n");
		return realAddress;
	} else {
		log_print("[LOAD_POINTER] No, pointer NOT cached\n");
		return 0;
	}
}