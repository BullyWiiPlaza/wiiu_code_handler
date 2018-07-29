#include "code_handler.h"

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/in.h>
#include "operating_system/operating_system_utilities.h"
#include "code_handler_functions.h"
#include "general/endian.h"
#include "general/rounding.h"
#include "general/bit_manipulations.h"
#include "architecture_specific.h"
#include "error_handling.h"
#include "code_handler_gecko_registers.h"

/* Do NOT reset during code handler executions */
unsigned int code_handler_executions_count = 0;

#define ILLEGAL_POINTER 0

/* Reset during code handler executions */
bool condition_flag;
uintptr_t loaded_pointer;
unsigned int time_dependence_delay;

#define CODE_LINE_BYTES 8

void apply_timer_reset() {
	time_dependence_delay = 0;
}

void apply_terminator() {
	condition_flag = false;
	loaded_pointer = ILLEGAL_POINTER;
}

#define MAXIMUM_ARGUMENTS_COUNT 8

int handle_procedure_call(const unsigned char *codes, int bytes_index) {
	unsigned int lower_destination_register_index = codes[bytes_index];
	log_printf("Lower destination register index: %i\n", lower_destination_register_index);
	if (lower_destination_register_index > (GECKO_REGISTERS_COUNT - 1)) {
		OSFatal("Lower destination register index exceeded");
	}

	bytes_index += sizeof(char);
	unsigned int upper_destination_register_index = codes[bytes_index];
	log_printf("Upper destination register index: %i\n", upper_destination_register_index);
	if (upper_destination_register_index > (GECKO_REGISTERS_COUNT - 1)) {
		OSFatal("Upper destination register index exceeded");
	}

	bytes_index += sizeof(char);
	unsigned int arguments_count = codes[bytes_index];
	log_printf("Arguments Count: %i\n", arguments_count);
	if (arguments_count > MAXIMUM_ARGUMENTS_COUNT) {
		OSFatal("Maximum arguments count exceeded");
	}

	bytes_index += sizeof(char);
	uintptr_t real_address = read_real_integer(&codes[bytes_index]);
	log_printf("Function Address: %p\n", (void *) real_address);
	bytes_index += sizeof(int);
	log_print("Allocating arguments...\n");
	unsigned int *arguments = (unsigned int *) calloc(MAXIMUM_ARGUMENTS_COUNT, sizeof(int));

	if (arguments == 0) {
		OSFatal("Allocating arguments failed");
	} else {
		log_print("Allocated!\n");
		for (unsigned int argument_index = 0; argument_index < arguments_count; argument_index++) {
			const unsigned char *argument_source = &codes[bytes_index];
			arguments[argument_index] = read_real_integer(argument_source);
			bytes_index += sizeof(int);
		}

		// If this is uneven we skip another 32-bit value since it wasn't an argument
		if (arguments_count % 2 != 0) {
			bytes_index += sizeof(int);
		}

		unsigned long returnValue = call_address(real_address, arguments);

		log_printf("Return Value: %llx\n", (unsigned long long int) returnValue);
		integer_registers[lower_destination_register_index] = (unsigned int) returnValue;
		integer_registers[upper_destination_register_index] = (unsigned int) (returnValue >> 32u);

		free(arguments);
	}

	return bytes_index;
}

void apply_search_template_code_type(const unsigned char *codes, int *length, int *bytes_index) {
	unsigned short target_match_index = read_real_short(&codes[(*bytes_index)]);
	log_printf("Target Match Index: %i\n", target_match_index);
	(*bytes_index) += sizeof(short);
	unsigned int search_template_length = codes[(*bytes_index)];
	log_printf("Search Template Length: %i\n", search_template_length);
	(*bytes_index) += sizeof(char);
	uintptr_t start_address = (unsigned int) (read_real_short(&codes[(*bytes_index)]) * 0x10000);
	log_printf("Start Address: %p\n", (void *) (int *) start_address);
	(*bytes_index) += sizeof(short);
	uintptr_t end_address = (unsigned int) (read_real_short(&codes[(*bytes_index)]) * 0x10000);
	log_printf("End Address: %p\n", (void *) (int *) end_address);
	(*bytes_index) += sizeof(short);
	const unsigned char *search_template = &codes[(*bytes_index)];
	bool template_found = false;
	uintptr_t found_address = search_for_template(search_template, search_template_length, start_address,
												  end_address, 1, target_match_index, &template_found);
	log_printf("Template Found: %i\n", template_found);

	// Code type header plus search template lines
	unsigned char address_loader_code_line[CODE_LINE_BYTES] = {CODE_TYPE_LOAD_POINTER_DIRECTLY, 0x00, 0x00, 0x00,
															   0x00, 0x00, 0x00, 0x00};

	// Find the amount of bytes taken in total by the search template line(s)
	int occupied_search_template_length = search_template_length;
	int remainder = search_template_length % CODE_LINE_BYTES;
	if (remainder != 0) {
		occupied_search_template_length += CODE_LINE_BYTES - remainder;
	}

	if (template_found) {
		log_printf("Found Address: %p\n", (void *) (int *) found_address);

		// Get the byte order right
		found_address = is_big_endian() ? found_address : htonl((uint32_t) found_address);
	}

	// Modify the code list by pasting the load pointer statement over the search code type
	*(unsigned int *) (address_loader_code_line + sizeof(int)) = (unsigned int) found_address;
	size_t address_loader_line_bytes_size = sizeof(address_loader_code_line);
	memcpy((void *) codes, address_loader_code_line, address_loader_line_bytes_size);

	// Delete the search template from the code list
	const unsigned char *remaining_code_list = search_template + occupied_search_template_length;
	size_t remaining_code_list_length = (size_t) (*length);
	memcpy((void *) search_template, remaining_code_list, remaining_code_list_length - occupied_search_template_length);
	(*length) -= occupied_search_template_length;

	(*bytes_index) = 0;
}

void parse_regular_code(unsigned char *codes, const enum CodeType *code_type,
						enum Pointer *pointer, enum ValueSize *valueSize,
						int *bytes_index, unsigned char **address,
						unsigned char **value, int *value_bytes) {
	unsigned char pointer_and_value_size = codes[(*bytes_index)];
	(*pointer) = (enum Pointer) get_upper_nibble(pointer_and_value_size);
	log_printf("Pointer: %i\n", (*pointer));

	switch (*code_type) {
		case CODE_TYPE_STRING_WRITE:
			(*bytes_index)++;
			unsigned char *value_bytes_address = &codes[(*bytes_index)];
			(*value_bytes) = read_real_short(value_bytes_address);
			(*bytes_index) += 2;
			break;

		default:
			/*case RAM_WRITE:
		case IF_EQUAL:
		case IF_NOT_EQUAL:
		case IF_GREATER:
		case IF_LESS:
		case IF_GREATER_OR_EQUAL:
		case IF_LESS_THAN_OR_EQUAL:*/
			(*valueSize) = (enum ValueSize) get_lower_nibble(pointer_and_value_size);
			log_printf("Value Size: %i\n", *valueSize);

			if (*code_type == CODE_TYPE_INTEGER_OPERATION
				|| *code_type == CODE_TYPE_FLOAT_OPERATION) {
				(*value_bytes) = get_bytes(VALUE_SIZE_THIRTY_TWO_BIT);
			} else {
				(*value_bytes) = get_bytes(*valueSize);
			}

			(*bytes_index) += 3;
			break;
	}

	(*address) = &codes[(*bytes_index)];
	(*bytes_index) += sizeof(int);

	if ((*pointer) == POINTER_POINTER) {
		if (*code_type == CODE_TYPE_RAM_WRITE) {
			(*address) -= sizeof(short);
			log_printf("Offset: %p\n", (void *) (long) read_real_short((*address)));
		} else {
			log_printf("Offset: %p\n", (void *) (long) read_real_integer((*address)));
		}
		log_print("Checking loaded pointer...\n");
		if (loaded_pointer != ILLEGAL_POINTER) {
			log_print("Applying loaded pointer...\n");
			if (*code_type == CODE_TYPE_RAM_WRITE) {
				unsigned char *address_pointer = (unsigned char *) (read_real_short(*address) + loaded_pointer);
				*address = (unsigned char *) &address_pointer;
			} else {
				*address += loaded_pointer;
			}
		} else {
			log_print("Invalid pointer, not executing...\n");
		}
		if (real_memory_accesses_are_enabled) {
			log_printf("Address: %p\n", (void *) (long) read_real_integer((*address)));
		}
	} else {
		log_printf("Address: %p\n", (void *) (long) read_real_integer((*address)));
	}

	switch (*code_type) {
		case CODE_TYPE_LOAD_POINTER:
			(*value) = &codes[(*bytes_index)];
			log_printf("Range start: %p\n", (void *) (long) read_real_integer(*value));
			break;

		default:
			if ((*code_type == CODE_TYPE_RAM_WRITE) && (*pointer == POINTER_POINTER)) {
				(*value) = &codes[(*bytes_index - sizeof(int))];
			} else {
				(*value) = &codes[(*bytes_index)];
			}
			log_printf("Value: %p {Length: %i}\n", (void *) (long) read_real_integer(*value), *value_bytes);
			break;
	}

	switch (*code_type) {
		case CODE_TYPE_STRING_WRITE:;
			int increment = round_up(*value_bytes, CODE_LINE_BYTES);
			(*bytes_index) += increment;
			break;

		default:
			if (!(*code_type == CODE_TYPE_RAM_WRITE && *pointer == POINTER_POINTER)) {
				(*bytes_index) += sizeof(int) * 2;
			}
			break;
	}
}

bool is_loaded_pointer_valid(const enum Pointer *pointer) {
	return ((*pointer) == POINTER_POINTER && loaded_pointer != ILLEGAL_POINTER)
	|| (*pointer) == POINTER_NO_POINTER;
}

#define CODE_LINE_POINTER_MAXIMUM_INCREMENT INT_MAX

int get_code_line_pointer_increment(unsigned char *haystack, const unsigned char *needle, int maximum_distance) {
	unsigned char *current_location = haystack;
	size_t needle_size = sizeof(needle);

	while (current_location - haystack < maximum_distance) {
		for (unsigned int needle_index = 0; needle_index < needle_size; needle_index++) {
			unsigned char needle_character = needle[needle_index];
			unsigned char current_character = current_location[needle_index];
			if (current_character != needle_character) {
				goto increment_current_location;
			}
		}

		return (int) (current_location - haystack);

		increment_current_location:
		current_location++;
	}

	return CODE_LINE_POINTER_MAXIMUM_INCREMENT;
}

unsigned char terminator_code_line[] = {CODE_TYPE_TERMINATOR, 0x00, 0x00, 0x00, 0xDE, 0xAD, 0xCA, 0xFE};
unsigned char reset_timer_code_line[] = {CODE_TYPE_RESET_TIMER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void branch_to_after_terminator(unsigned char *codes, const int *codeLineIndex,
								int *bytes_index, unsigned char *terminator) {
	unsigned char *current_code_position = codes + (*bytes_index);

	int remaining_code_length = (*codeLineIndex) - (*bytes_index);
	int increment = get_code_line_pointer_increment(current_code_position, terminator, remaining_code_length);

	if (increment == CODE_LINE_POINTER_MAXIMUM_INCREMENT) {
		char message_buffer[MESSAGE_BUFFER_SIZE] = "Condition not terminated";
		set_error_message_buffer(message_buffer, codes);
		OSFatal(message_buffer);
	} else {
		log_printf("Skipping ahead by %p bytes...\n", (void *) (long) increment);
		(*bytes_index) += increment;
	}
}

void parse_conditional_code(unsigned char *codes, enum Pointer *pointer,
							enum ValueSize *value_size, int *bytesIndex,
							unsigned char **address, unsigned char **value,
							int *value_bytes, int *length, enum ComparisonType comparison_type) {
	enum CodeType code_type = CODE_TYPE_IF_NOT_EQUAL;
	parse_regular_code(codes, &code_type, pointer, value_size, bytesIndex, address, value, value_bytes);
	unsigned char *upper_value = &codes[(*bytesIndex - sizeof(int))];

	if (is_loaded_pointer_valid(pointer)) {
		condition_flag = compare_value(*address, *value, value_size, upper_value, *value_bytes, comparison_type);
		log_printf("Comparison result: %i\n", condition_flag);
		branch_to_after_terminator(codes, length, bytesIndex, terminator_code_line);
	}
}

void run_code_handler_internal(unsigned char *codes, int codes_length) {
	// Keep running the code handler till all codes are handled
	while (codes_length > 0) {
		int bytes_index = 0;
		enum CodeType code_type = (enum CodeType) codes[bytes_index];
		log_printf("Code Type: 0x%x\n", code_type);
		bytes_index++;

		enum Pointer pointer;
		enum ValueSize value_size;
		unsigned char *address;
		unsigned char *value;
		int value_bytes;

		switch (code_type) {
			case CODE_TYPE_RAM_WRITE:
				log_printf("### RAM Write ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);

				if (is_loaded_pointer_valid(&pointer)) {
					write_value(address, value, value_bytes);
				}
				break;

			case CODE_TYPE_STRING_WRITE:
				log_printf("### String Write ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);

				if (is_loaded_pointer_valid(&pointer)) {
					write_string(address, value, value_bytes);
				}
				break;

			case CODE_TYPE_SKIP_WRITE:
				log_printf("### Skip Write ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);
				bytes_index -= sizeof(int);
				unsigned char *step_size = &codes[bytes_index];
				log_printf("Step size: %p\n", (void *) (long) read_real_integer(step_size));
				bytes_index += sizeof(int);
				unsigned char *increment = &codes[bytes_index];
				log_printf("Value increment: %p\n", (void *) (long) read_real_integer(increment));
				bytes_index += sizeof(int);
				bytes_index += sizeof(int);
				unsigned char *iterations_count = &codes[2];
				log_printf("Iterations count: %i\n", read_real_short(iterations_count));

				if (is_loaded_pointer_valid(&pointer)) {
					skip_write_memory(address, value, value_bytes, step_size, increment, iterations_count);
				}
				break;

			case CODE_TYPE_IF_EQUAL:
				log_printf("### If equal ###\n");
				parse_conditional_code(codes, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes,
									   &codes_length, (enum ComparisonType) code_type);
				break;

			case CODE_TYPE_IF_NOT_EQUAL:
				log_printf("### If not equal ###\n");
				parse_conditional_code(codes, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes,
									   &codes_length,
									   (enum ComparisonType) code_type);
				break;

			case CODE_TYPE_IF_GREATER:
				log_printf("### If greater than ###\n");
				parse_conditional_code(codes, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes,
									   &codes_length,
									   (enum ComparisonType) code_type);
				break;

			case CODE_TYPE_IF_LESS:
				log_printf("### If less than ###\n");
				parse_conditional_code(codes, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes,
									   &codes_length,
									   (enum ComparisonType) code_type);
				break;

			case CODE_TYPE_IF_GREATER_THAN_OR_EQUAL:
				log_printf("### If greater than or equal ###\n");
				parse_conditional_code(codes, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes,
									   &codes_length,
									   (enum ComparisonType) code_type);
				break;

			case CODE_TYPE_IF_LESS_THAN_OR_EQUAL:
				log_printf("### If less than or equal ###\n");
				parse_conditional_code(codes, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes,
									   &codes_length,
									   (enum ComparisonType) code_type);
				break;

			case CODE_TYPE_AND:
				log_printf("### Apply AND ###\n");
				parse_conditional_code(codes, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes,
									   &codes_length,
									   (enum ComparisonType) code_type);
				break;

			case CODE_TYPE_OR:
				log_printf("### Apply OR ###\n");
				parse_conditional_code(codes, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes,
									   &codes_length,
									   (enum ComparisonType) code_type);
				break;

			case CODE_TYPE_IF_VALUE_BETWEEN:
				log_printf("### If Value Between ###\n");
				parse_conditional_code(codes, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes,
									   &codes_length,
									   (enum ComparisonType) code_type);
				break;

			case CODE_TYPE_ADD_TIME_DEPENDENCE:
				log_printf("### Add Time Dependence ###\n");
				bytes_index += sizeof(int) - sizeof(char);
				value = &codes[bytes_index];
				time_dependence_delay = read_real_integer(value);
				log_printf("[ADD_TIME_DEPENDENCE] %i\n", time_dependence_delay);

				if (time_dependence_delay != 0 && code_handler_executions_count % time_dependence_delay != 0) {
					branch_to_after_terminator(codes, &codes_length, &bytes_index, reset_timer_code_line);
				} else {
					log_printf("Not branching: Executions count: %i\n", code_handler_executions_count);
				}

				bytes_index += sizeof(int);
				break;

			case CODE_TYPE_RESET_TIMER:
				log_printf("### Reset Timer ###\n");
				bytes_index += sizeof(reset_timer_code_line) - 1;
				apply_timer_reset();
				log_print("[RESET_TIMER] Executed\n");
				break;

			case CODE_TYPE_LOAD_INTEGER:
				log_printf("### Load Integer ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);
				int excessive_bytes = sizeof(int) * 2;
				unsigned char *register_index = &codes[bytes_index - sizeof(int) - excessive_bytes - sizeof(char)];
				log_printf("Register index: %i\n", *register_index);
				bytes_index -= excessive_bytes;
				load_integer(register_index, value_size, address);
				break;

			case CODE_TYPE_STORE_INTEGER:
				log_printf("### Store Integer ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);
				excessive_bytes = sizeof(int) * 2;
				register_index = &codes[bytes_index - sizeof(int) - excessive_bytes - sizeof(char)];
				log_printf("Register index: %i\n", *register_index);
				bytes_index -= excessive_bytes;
				store_integer(register_index, value_size, address);
				break;

			case CODE_TYPE_LOAD_FLOAT:
				log_printf("### Load Float ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);
				excessive_bytes = sizeof(int) * 2;
				register_index = &codes[bytes_index - sizeof(int) - excessive_bytes - sizeof(char)];
				log_printf("Register index: %i\n", *register_index);
				bytes_index -= excessive_bytes;
				load_float(register_index, address);
				break;

			case CODE_TYPE_STORE_FLOAT:
				log_printf("### Store Float ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);
				excessive_bytes = sizeof(int) * 2;
				register_index = &codes[bytes_index - sizeof(int) - excessive_bytes - sizeof(char)];
				log_printf("Register index: %i\n", *register_index);
				bytes_index -= excessive_bytes;
				store_float(register_index, address);
				break;

			case CODE_TYPE_INTEGER_OPERATION:
				log_printf("### Integer Operation ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);
				excessive_bytes = sizeof(int) * 2;
				int integer_operation_index = bytes_index - sizeof(int) - excessive_bytes - sizeof(char) - 2;
				unsigned char *integer_operation_pointer = &codes[integer_operation_index];
				unsigned char *integer_destination_register_pointer = &codes[integer_operation_index + 1];
				unsigned char *integer_second_operand_register_pointer = &codes[integer_operation_index + 2];
				int integer_operation_type = *integer_operation_pointer;

				if (integer_operation_type > INTEGER_REGISTER_OPERATION_DIVISION) {
					enum IntegerDirectValueOperation direct_value_operation = (enum IntegerDirectValueOperation) integer_operation_type;
					apply_integer_direct_value_operation(integer_destination_register_pointer, address,
														 direct_value_operation);
				} else {
					enum IntegerRegisterOperation register_operation = (enum IntegerRegisterOperation) integer_operation_type;
					apply_integer_register_operation(integer_destination_register_pointer,
													 integer_second_operand_register_pointer,
													 register_operation);
				}

				bytes_index -= sizeof(int) * 2;

				break;

			case CODE_TYPE_FLOAT_OPERATION:
				log_printf("### Float Operation ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);
				excessive_bytes = sizeof(int) * 2;
				int float_operation_index = bytes_index - sizeof(int) - excessive_bytes - sizeof(char) - 2;
				unsigned char *float_operation_pointer = &codes[float_operation_index];
				unsigned char *float_destination_register_pointer = &codes[float_operation_index + 1];
				unsigned char *float_second_operand_register_pointer = &codes[float_operation_index + 2];
				int float_operation_type = *float_operation_pointer;

				if (float_operation_type > FLOAT_REGISTER_OPERATION_DIVISION &&
					float_operation_type < FLOAT_REGISTER_OPERATION_FLOAT_TO_INTEGER) {
					enum FloatDirectValueOperation direct_value_operation = (enum FloatDirectValueOperation) float_operation_type;
					applyFloat_direct_value_operation(float_destination_register_pointer, address,
													  direct_value_operation);
				} else {
					enum FloatRegisterOperation register_operation = (enum FloatRegisterOperation) float_operation_type;
					apply_float_register_operation(float_destination_register_pointer,
												   float_second_operand_register_pointer,
												   register_operation);
				}

				bytes_index -= sizeof(int) * 2;
				break;

			case CODE_TYPE_FILL_MEMORY_AREA:
				log_printf("### Fill Memory Area ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);
				int offset = read_real_integer(&codes[bytes_index - sizeof(int)]);
				log_printf("Offset: %p\n", (void *) (long) offset);
				if (is_loaded_pointer_valid(&pointer)) {
					unsigned int step_size_int = 4;
					step_size = get_character_pointer(&step_size_int, sizeof(int));
					unsigned int increment_int = 0;
					increment = get_character_pointer(&increment_int, sizeof(int));
					unsigned int iterations_count_int = offset / sizeof(int);
					iterations_count = get_character_pointer(&iterations_count_int, sizeof(short));
					skip_write_memory(address, value, sizeof(int), step_size, increment, iterations_count);
				}

				break;

			case CODE_TYPE_LOAD_POINTER:
				log_printf("### Load Pointer ###\n");
				parse_regular_code(codes, &code_type, &pointer, &value_size, &bytes_index, &address, &value, &value_bytes);
				loaded_pointer = loadPointer(address, value);
				break;

			case CODE_TYPE_ADD_OFFSET_TO_POINTER:;
				log_printf("### Add To Pointer ###\n");
				bytes_index += 3;
				int pointer_offset = read_real_integer(&codes[bytes_index]);
				loaded_pointer += pointer_offset;
				log_printf("[ADD_POINTER]: *%p += %p\n", (void *) (long) loaded_pointer, (void *) (long) pointer_offset);
				bytes_index += sizeof(int);
				break;

			case CODE_TYPE_LOAD_POINTER_DIRECTLY:
				log_printf("### Loading Pointer Directly ###\n");
				bytes_index += sizeof(int) - sizeof(char);
				loaded_pointer = read_real_integer(&codes[bytes_index]);
				log_printf("Loaded Pointer: %p\n", (void *) loaded_pointer);
				bytes_index += sizeof(int);
				break;

			case CODE_TYPE_EXECUTE_ASSEMBLY:
				log_printf("### Execute Assembly ###\n");
				bytes_index++;
				unsigned int assembly_lines = read_real_short(&codes[bytes_index]);
				unsigned int assembly_bytes_count = assembly_lines * CODE_LINE_BYTES;
				bytes_index += sizeof(short);
				bytes_index += sizeof(int);
				value = &codes[bytes_index];
				bytes_index += (assembly_lines * CODE_LINE_BYTES);
				execute_assembly(value, assembly_bytes_count);
				break;

			case CODE_TYPE_PERFORM_SYSTEM_CALL:
				log_printf("### Perform System Call ###\n");
				bytes_index++;
				value = &codes[bytes_index];
				bytes_index += sizeof(short);
				bytes_index += sizeof(int);
				execute_system_call(read_real_short(value));
				break;

			case CODE_TYPE_TERMINATOR:
				log_printf("### Terminator ###\n");
				bytes_index += sizeof(terminator_code_line) - 1;
				log_printf("[Before] Loaded pointer: %p\n", (void *) (long) loaded_pointer);
				log_printf("[Before] Condition flag: %i\n", condition_flag);
				apply_terminator();
				log_printf("[After] Loaded pointer: %p\n", (void *) (long) loaded_pointer);
				log_printf("[After] Condition flag: %i\n", condition_flag);
				break;

			case CODE_TYPE_NO_OPERATION:
				log_printf("### No Operation ###\n");
				bytes_index += 3;
				bytes_index += sizeof(int);
				log_print("[NO_OPERATION] Do nothing\n");
				break;

			case CODE_TYPE_TIMER_TERMINATION:
				log_printf("### Timer Termination ###\n");
				bytes_index += sizeof(terminator_code_line) - 1;
				log_print("[TIMER_TERMINATION] Executed\n");
				break;

			case CODE_TYPE_CORRUPTER:
				log_printf("### Corrupter ###\n");
				bytes_index += 3;

				unsigned char *beginning = &codes[bytes_index];
				log_printf("Beginning: %p\n", (void *) (long) read_real_integer(beginning));
				bytes_index += sizeof(int);

				unsigned char *end = &codes[bytes_index];
				log_printf("End: %p\n", (void *) (long) read_real_integer(end));
				bytes_index += sizeof(int);

				unsigned char *search_value = &codes[bytes_index];
				log_printf("Search value: %p\n", (void *) (long) read_real_integer(search_value));
				bytes_index += sizeof(int);

				unsigned char *replacement_value = &codes[bytes_index];
				log_printf("Replacement value: %p\n", (void *) (long) read_real_integer(replacement_value));
				bytes_index += sizeof(int);

				bytes_index += sizeof(int);

				apply_corrupter(beginning, end, search_value, replacement_value);
				break;

				/*
					 F6 search code type: Searches the given template and loads it as pointer if found.
					 PPPP = Index of the desired match
					 XX = Search template length in bytes
					 YYYY = Higher 16-bit of the address to start at
					 ZZZZ = Higher 16-bit of the upper limit address
					 GG, HH, II, JJ, KK, LL, MM, NN, ... = Bytes of the search template
					 followed by anything using the loaded pointer or not
					 F6PPPPXX YYYYZZZZ
					 GGHHIIJJ KKLLMMNN
					 ...
					 D0000000 DEADCAFE
				 */
			case CODE_TYPE_SEARCH_TEMPLATE:
				log_printf("### Search Template ###\n");
				apply_search_template_code_type(codes, &codes_length, &bytes_index);

				break;

				/*
					 F8 procedure call code type: Calls an address with up to 8 integer arguments
					 XX = Amount of 32-bit arguments (maximum: 8)
					 YYYYYYYY = The address to call
					 GGGGGGGG, HHHHHHHH, ... = Integer arguments
					 GG = Destination register (0 - 7) for the lower 32-bit integer return value
					 RR = Destination register (0 - 7) for the upper 32-bit integer return value
					 F8GGRRXX YYYYYYYY
					GGGGGGGG HHHHHHHH
					...
				*/
			case CODE_TYPE_PROCEDURE_CALL:
				log_printf("### Remote Procedure Call ###\n");
				bytes_index = handle_procedure_call(codes, bytes_index);

				break;

			default:;
#define ERROR_BUFFER_SIZE 100
				char error_message_buffer[ERROR_BUFFER_SIZE];
				snprintf(error_message_buffer, ERROR_BUFFER_SIZE, "Unhandled code type: %i\n", code_type);
				OSFatal(error_message_buffer);
				break;
		}

		// Those have been consumed
		codes_length -= bytes_index;

		// Is not supposed to happen
		if (codes_length < 0) {
			char error_message_buffer[ERROR_BUFFER_SIZE];
			snprintf(error_message_buffer, ERROR_BUFFER_SIZE, "Negative length: %i", codes_length);
			OSFatal(error_message_buffer);
		}

		if (codes_length % CODE_LINE_BYTES != 0) {
			char error_Message_buffer[ERROR_BUFFER_SIZE];
			snprintf(error_Message_buffer, ERROR_BUFFER_SIZE, "Illegal code length: %i", codes_length);
			OSFatal(error_Message_buffer);
		}

		printf("\n");
		codes = codes + bytes_index;
	}

	// One full code handler execution is done
	code_handler_executions_count++;
}

void initialize() {
	apply_terminator();
	apply_timer_reset();
	initialize_gecko_registers();
}

void run_code_handler(unsigned char *codes, int length) {
	initialize();
	run_code_handler_internal(codes, length);
}