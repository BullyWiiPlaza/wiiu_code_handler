#include "code_handler_functions.h"

#include "general/endian.h"

bool real_memory_accesses_are_enabled = false;

unsigned long call_address(uintptr_t real_address, const unsigned int *arguments) {
	typedef unsigned long functionType(int, int, int, int, int, int, int, int);
	auto function = (functionType *) real_address;
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
	int matches_count = 0;
	for (uintptr_t current_address = address; current_address < end_address; current_address += stepSize) {
		int comparison_result = 0;

		if (real_memory_accesses_are_enabled) {
			comparison_result = memcmp((const void *) current_address, (const void *) search_template, (size_t) length);
		}

		if (comparison_result == 0) {
			matches_count++;
		}

		if ((matches_count - 1) == target_match_index) {
			*template_found = true;
			return current_address;
		}
	}

	*template_found = false;
	return 0;
}

void increment_value(int value_length, unsigned int real_increment, unsigned char *modifiable_value) {
	switch (value_length) {
		case sizeof(char):
			modifiable_value[3] += real_increment;
			break;

		case sizeof(short): {
			short incremented_short_value = read_real_short(modifiable_value);
			incremented_short_value += real_increment;
			incremented_short_value = read_real_short((unsigned char *) &incremented_short_value);
			memcpy(&modifiable_value[sizeof(int) - sizeof(short)], &incremented_short_value, sizeof(short));
		}
			break;

		case sizeof(int): {
			int incremented_int_value = read_real_integer(modifiable_value);
			incremented_int_value += real_increment;
			incremented_int_value = read_real_integer((unsigned char *) &incremented_int_value);
			memcpy(&modifiable_value[sizeof(int) - sizeof(int)], &incremented_int_value, sizeof(int));
		}
			break;

		default:
			OSFatal("Unhandled value length");
	}
}

void skip_write_memory(unsigned char *address, unsigned char *value,
					   int value_length, unsigned char *step_size,
					   unsigned char *increment, unsigned char *iterations_count) {
	uintptr_t real_address = read_real_integer(address);
	unsigned short real_iterations_count = read_real_short(iterations_count);
	unsigned int real_increment = read_real_integer(increment);
	unsigned int real_step_size = read_real_integer(step_size);
	unsigned char modifiable_value[sizeof(int)];
	memcpy(modifiable_value, value, sizeof(int));

	for (int iteration_index = 0; iteration_index < real_iterations_count; iteration_index++) {
		unsigned int byte_index = sizeof(int) - value_length;
		for (; byte_index < sizeof(int); byte_index++) {
			auto target_address = (unsigned int *) (((unsigned char *) real_address) + byte_index);
			unsigned char byte = modifiable_value[byte_index];
			log_printf("[SKIP_WRITE]: *%p = 0x%x\n", (void *) target_address, byte);

			if (real_memory_accesses_are_enabled) {
				*target_address = byte;
			}
		}

		increment_value(value_length, real_increment, modifiable_value);
		real_address += real_step_size;
	}
}

void write_string(unsigned char *address, unsigned char *value, int value_length) {
	uintptr_t real_address = read_real_integer(address);
	log_printf("[STRING_WRITE]: *%p = 0x%x {Length: %i}\n", (void *) real_address, *(unsigned int *) value,
			   value_length);

	if (real_memory_accesses_are_enabled) {
		memcpy(address, value, (size_t) value_length);
	}
}

void write_value(unsigned char *address, const unsigned char *value, int value_length) {
	unsigned int byte_index = sizeof(int) - value_length;
	for (; byte_index < sizeof(int); byte_index++) {
		uintptr_t target_address = (((uintptr_t) read_real_integer(address)) + byte_index);
		unsigned char byte = value[byte_index];
		log_printf("[RAM_WRITE]: *%p = 0x%x\n", (void *) target_address, byte);

		if (real_memory_accesses_are_enabled) {
			*(unsigned char *) target_address = byte;
		}
	}
}

bool compare_value(unsigned char *address, unsigned char *value,
				   const enum ValueSize *value_size, unsigned char *upper_value,
				   int value_length, enum ComparisonType comparison_type) {
	uintptr_t real_address = read_real_integer(address);
	uintptr_t real_value = read_real_integer(value);
	uintptr_t real_ipper_limit = read_real_integer(upper_value);

	if (comparison_type != COMPARISON_TYPE_VALUE_BETWEEN) {
		log_printf("[COMPARE]: *%p {comparison %i} %p {Last %i bytes}\n", (void *) real_address, comparison_type,
				   (void *) real_value, value_length);
	}

	unsigned int addressValue = 0;

	switch (value_length) {
		case sizeof(int):
			if (real_memory_accesses_are_enabled) {
				addressValue = read_real_integer((unsigned char *) real_address);
			}

			real_value = read_real_integer(value + sizeof(int) - sizeof(int));
			break;

		case sizeof(short):
			if (real_memory_accesses_are_enabled) {
				addressValue = read_real_short((unsigned char *) real_address);
			}

			real_value = read_real_short(value + sizeof(int) - sizeof(short));
			break;

		case sizeof(char):
			if (real_memory_accesses_are_enabled) {
				addressValue = read_real_byte((unsigned char *) real_address);
			}

			real_value = read_real_byte(value + sizeof(int) - sizeof(char));
			break;

		default:
			OSFatal("Unhandled value size");
	}

	bool bit_operator_result = false;

	switch (comparison_type) {
		case COMPARISON_TYPE_AND:
			bit_operator_result = (addressValue & real_value) == 0;
			break;

		case COMPARISON_TYPE_OR:
			bit_operator_result = (addressValue | real_value) == 0;
			break;

		default:
			break;
	}

	int comparison_result = 0;

	if (real_memory_accesses_are_enabled) {
		comparison_result = memcmp((void *) real_address, (void *) value, (size_t) value_length) == 0;
	}

	switch (comparison_type) {
		case COMPARISON_TYPE_EQUAL:
			return comparison_result == 0;

		case COMPARISON_TYPE_NOT_EQUAL:
			return comparison_result != 0;

		case COMPARISON_TYPE_GREATER_THAN:
			return comparison_result > 0;

		case COMPARISON_TYPE_LESS_THAN:
			return comparison_result < 0;

		case COMPARISON_TYPE_GREATER_THAN_OR_EQUAL:
			return (comparison_result > 0) || (comparison_result == 0);

		case COMPARISON_TYPE_LESS_THAN_OR_EQUAL:
			return (comparison_result < 0) || (comparison_result == 0);

		case COMPARISON_TYPE_AND:
			return bit_operator_result;

		case COMPARISON_TYPE_OR:
			return bit_operator_result;

		case COMPARISON_TYPE_VALUE_BETWEEN: {
			unsigned int read_value = read_real_value(*value_size, address);
			uintptr_t lower_limit = real_value;
			log_printf("[COMPARE]: (*%p > %p) && (*%p < %p) {Last %i bytes}\n", (void *) real_address,
					   (void *) lower_limit, (void *) real_address, (void *) real_ipper_limit, value_length);
			return read_value > lower_limit && read_value < real_ipper_limit;
		}

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
		uintptr_t destination_address = (0x10000000 - size);
		kernel_copy_data((unsigned char *) destination_address, instructions, size);

		// Execute the assembly from there
		auto function = (void (*)()) destination_address;
		function();

		// Clear the memory contents again
		memset((void *) instructions, 0, (size_t) size);
		kernel_copy_data((unsigned char *) destination_address, instructions, size);
	}
}

void apply_corrupter(unsigned char *beginning, unsigned char *end, unsigned char *search_value,
					 unsigned char *replacement_value) {
	uintptr_t start_address = read_real_integer(beginning);
	uintptr_t end_address = read_real_integer(end);
	unsigned int real_search_value = read_real_integer(search_value);
	unsigned int real_replacement_value = read_real_integer(replacement_value);

	log_print("[CORRUPTER] Replacing values...\n");

	uintptr_t current_address = start_address;
	for (; current_address < end_address; current_address += sizeof(int)) {
		uintptr_t read_value = 0;

		log_printf("Reading value from address %p...\n", (void *) current_address);
		if (real_memory_accesses_are_enabled) {
			read_value = current_address;
		}

		log_printf("Read value is: %p\n", (void *) read_value);

		if (read_value == real_search_value) {
			log_printf("Replacing with %08x...\n", real_replacement_value);
			if (real_memory_accesses_are_enabled) {
				current_address = real_replacement_value;
			}
		} else {
			log_print("Value differs...\n");
		}
	}
}

unsigned int read_real_value(enum ValueSize value_size, unsigned char *pointer_to_address) {
	auto address_pointer = (unsigned char *) (uintptr_t) read_real_integer(pointer_to_address);
	log_printf("[GET_VALUE] Getting value from %p {Value Size: %i}...\n", (void *) address_pointer,
			   get_bytes(value_size));

	if (real_memory_accesses_are_enabled) {
		switch (value_size) {
			case VALUE_SIZE_EIGHT_BIT:
				return read_real_byte(address_pointer);

			case VALUE_SIZE_SIXTEEN_BIT:
				return read_real_short(address_pointer);

			case VALUE_SIZE_THIRTY_TWO_BIT:
				return read_real_integer(address_pointer);
		}

		OSFatal("Unmatched value size");

		// Dummy return, will never execute
		return 0;
	}

	// For PC testing only
	return 0x43219876;
}

uintptr_t loadPointer(unsigned char *address, unsigned char *value) {
	uintptr_t real_address = read_real_integer(address);
	unsigned int range_starting_address = read_real_integer(value);
	unsigned int range_ending_address = read_real_integer(value + sizeof(int));
	uintptr_t de_referenced = 0;

	if (real_memory_accesses_are_enabled) {
		de_referenced = *(uintptr_t *) real_address;
	}

	log_printf("[LOAD_POINTER] Is %p between %08x and %08x?\n", (void *) de_referenced, range_starting_address,
			   range_ending_address);

	if (de_referenced >= range_starting_address && de_referenced <= range_ending_address) {
		log_print("[LOAD_POINTER] Yes, pointer cached\n");
		return real_address;
	} else {
		log_print("[LOAD_POINTER] No, pointer NOT cached\n");
		return 0;
	}
}