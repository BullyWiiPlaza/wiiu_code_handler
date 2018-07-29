#include "code_handler_functions.h"

#include "general/endian.h"

bool real_memory_accesses_are_enabled = false;

unsigned long call_address(uintptr_t real_address, const unsigned int *arguments) {
	typedef unsigned long functionType(int, int, int, int, int, int, int, int);
	functionType *function = (functionType *) real_address;
	unsigned long returnValue = 0x1111111122222222;

	if (real_memory_accesses_are_enabled) {
		// We count instead of incrementing an index variable because: operation on 'argumentIndex' may be undefined
		returnValue = function(arguments[0], arguments[1],
							   arguments[2], arguments[3],
							   arguments[4], arguments[5],
							   arguments[6], arguments[7]);
	}

	return returnValue;
}

uintptr_t search_for_template(const unsigned char *search_template, unsigned int length,
							  uintptr_t address, uintptr_t end_address,
							  int stepSize, unsigned short target_match_index,
							  bool *template_found) {
	int matchesCount = 0;
	for (uintptr_t currentAddress = address; currentAddress < end_address; currentAddress += stepSize) {
		int comparisonResult = 0;

		if (real_memory_accesses_are_enabled) {
			comparisonResult = memcmp((const void *) currentAddress, (const void *) search_template, (size_t) length);
		}

		if (comparisonResult == 0) {
			matchesCount++;
		}

		if ((matchesCount - 1) == target_match_index) {
			*template_found = true;
			return currentAddress;
		}
	}

	*template_found = false;
	return 0;
}

void incrementValue(int valueLength, unsigned int realIncrement, unsigned char *modifiableValue) {
	switch (valueLength) {
		case sizeof(char):
			modifiableValue[3] += realIncrement;
			break;

		case sizeof(short):;
			short incrementedShortValue = read_real_short(modifiableValue);
			incrementedShortValue += realIncrement;
			incrementedShortValue = read_real_short((unsigned char *) &incrementedShortValue);
			memcpy(&modifiableValue[sizeof(int) - sizeof(short)], &incrementedShortValue, sizeof(short));
			break;

		case sizeof(int):;
			int incrementedIntValue = read_real_integer(modifiableValue);
			incrementedIntValue += realIncrement;
			incrementedIntValue = read_real_integer((unsigned char *) &incrementedIntValue);
			memcpy(&modifiableValue[sizeof(int) - sizeof(int)], &incrementedIntValue, sizeof(int));
			break;

		default:
			OSFatal("Unhandled value length");
	}
}

void skip_write_memory(unsigned char *address, unsigned char *value,
					   int value_length, unsigned char *step_size,
					   unsigned char *increment, unsigned char *iterations_count) {
	uintptr_t realAddress = read_real_integer(address);
	unsigned short realIterationsCount = read_real_short(iterations_count);
	unsigned int realIncrement = read_real_integer(increment);
	unsigned int realStepSize = read_real_integer(step_size);
	unsigned char modifiableValue[sizeof(int)];
	memcpy(modifiableValue, value, sizeof(int));

	for (int iterationIndex = 0; iterationIndex < realIterationsCount; iterationIndex++) {
		unsigned int byteIndex = sizeof(int) - value_length;
		for (; byteIndex < sizeof(int); byteIndex++) {
			unsigned int *targetAddress = (unsigned int *) (((unsigned char *) realAddress) + byteIndex);
			unsigned char byte = modifiableValue[byteIndex];
			log_printf("[SKIP_WRITE]: *%p = 0x%x\n", (void *) targetAddress, byte);

			if (real_memory_accesses_are_enabled) {
				*targetAddress = byte;
			}
		}

		incrementValue(value_length, realIncrement, modifiableValue);
		realAddress += realStepSize;
	}
}

void write_string(unsigned char *address, unsigned char *value, int value_length) {
	uintptr_t realAddress = read_real_integer(address);
	log_printf("[STRING_WRITE]: *%p = 0x%x {Length: %i}\n", (void *) realAddress, *(unsigned int *) value, value_length);

	if (real_memory_accesses_are_enabled) {
		memcpy(address, value, (size_t) value_length);
	}
}

void write_value(unsigned char *address, const unsigned char *value, int value_length) {
	unsigned int byteIndex = sizeof(int) - value_length;
	for (; byteIndex < sizeof(int); byteIndex++) {
		uintptr_t targetAddress = (((uintptr_t) read_real_integer(address)) + byteIndex);
		unsigned char byte = value[byteIndex];
		log_printf("[RAM_WRITE]: *%p = 0x%x\n", (void *) targetAddress, byte);

		if (real_memory_accesses_are_enabled) {
			*(unsigned char *) targetAddress = byte;
		}
	}
}

bool compare_value(unsigned char *address, unsigned char *value,
				   const enum ValueSize *value_size, unsigned char *upper_value,
				   int value_length, enum ComparisonType comparison_type) {
	uintptr_t realAddress = read_real_integer(address);
	uintptr_t realValue = read_real_integer(value);
	uintptr_t realUpperLimit = read_real_integer(upper_value);

	if (comparison_type != COMPARISON_TYPE_VALUE_BETWEEN) {
		log_printf("[COMPARE]: *%p {comparison %i} %p {Last %i bytes}\n", (void *) realAddress, comparison_type,
				   (void *) realValue, value_length);
	}

	unsigned int addressValue = 0;

	switch (value_length) {
		case sizeof(int):
			if (real_memory_accesses_are_enabled) {
				addressValue = read_real_integer((unsigned char *) realAddress);
			}

			realValue = read_real_integer(value + sizeof(int) - sizeof(int));
			break;

		case sizeof(short):
			if (real_memory_accesses_are_enabled) {
				addressValue = read_real_short((unsigned char *) realAddress);
			}

			realValue = read_real_short(value + sizeof(int) - sizeof(short));
			break;

		case sizeof(char):
			if (real_memory_accesses_are_enabled) {
				addressValue = read_real_byte((unsigned char *) realAddress);
			}

			realValue = read_real_byte(value + sizeof(int) - sizeof(char));
			break;

		default:
			OSFatal("Unhandled value size");
	}

	bool bitOperatorResult = 0;

	switch (comparison_type) {
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

	if (real_memory_accesses_are_enabled) {
		comparisonResult = memcmp((void *) realAddress, (void *) value, (size_t) value_length) == 0;
	}

	switch (comparison_type) {
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
			unsigned int readValue = read_real_value(*value_size, address);
			uintptr_t lowerLimit = realValue;
			log_printf("[COMPARE]: (*%p > %p) && (*%p < %p) {Last %i bytes}\n", (void *) realAddress,
					   (void *) lowerLimit, (void *) realAddress, (void *) realUpperLimit, value_length);
			return readValue > lowerLimit && readValue < realUpperLimit;

		default:
			OSFatal("Unhandled comparison type");

			// Dummy return
			return false;
	}
}

void execute_assembly(unsigned char *instructions, unsigned int size) {
	unsigned int realInstructions = read_real_integer(instructions);
	log_printf("[EXECUTE_ASSEMBLY]: 0x%08x... {Size: %i}\n", realInstructions, size);

	if (real_memory_accesses_are_enabled) {
		// Write the assembly to an executable code region
		uintptr_t destinationAddress = (0x10000000 - size);
		kernel_copy_data((unsigned char *) destinationAddress, instructions, size);

		// Execute the assembly from there
		void (*function)() = (void (*)()) destinationAddress;
		function();

		// Clear the memory contents again
		memset((void *) instructions, 0, (size_t) size);
		kernel_copy_data((unsigned char *) destinationAddress, instructions, size);
	}
}

void apply_corrupter(unsigned char *beginning, unsigned char *end, unsigned char *search_value,
					 unsigned char *replacement_value) {
	uintptr_t startAddress = read_real_integer(beginning);
	uintptr_t endAddress = read_real_integer(end);
	unsigned int realSearchValue = read_real_integer(search_value);
	unsigned int realReplacementValue = read_real_integer(replacement_value);

	log_print("[CORRUPTER] Replacing values...\n");

	uintptr_t currentAddress = startAddress;
	for (; currentAddress < endAddress; currentAddress += sizeof(int)) {
		uintptr_t readValue = 0;

		log_printf("Reading value from address %p...\n", (void *) currentAddress);
		if (real_memory_accesses_are_enabled) {
			readValue = currentAddress;
		}

		log_printf("Read value is: %p\n", (void *) readValue);

		if (readValue == realSearchValue) {
			log_printf("Replacing with %08x...\n", realReplacementValue);
			if (real_memory_accesses_are_enabled) {
				currentAddress = realReplacementValue;
			}
		} else {
			log_print("Value differs...\n");
		}
	}
}

unsigned int read_real_value(enum ValueSize value_size, unsigned char *pointer_to_address) {
	unsigned char *addressPointer = (unsigned char *) (uintptr_t) read_real_integer(pointer_to_address);
	log_printf("[GET_VALUE] Getting value from %p {Value Size: %i}...\n", (void *) addressPointer, get_bytes(value_size));

	if (real_memory_accesses_are_enabled) {
		switch (value_size) {
			case VALUE_SIZE_EIGHT_BIT:
				return read_real_byte(addressPointer);

			case VALUE_SIZE_SIXTEEN_BIT:
				return read_real_short(addressPointer);

			case VALUE_SIZE_THIRTY_TWO_BIT:
				return read_real_integer(addressPointer);
		}

		OSFatal("Unmatched value size");

		// Dummy return, will never execute
		return 0;
	}

	// For PC testing only
	return 0x43219876;
}

uintptr_t loadPointer(unsigned char *address, unsigned char *value) {
	uintptr_t realAddress = read_real_integer(address);
	unsigned int rangeStartingAddress = read_real_integer(value);
	unsigned int rangeEndingAddress = read_real_integer(value + sizeof(int));
	uintptr_t deReferenced = 0;

	if (real_memory_accesses_are_enabled) {
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