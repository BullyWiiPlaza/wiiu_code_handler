#include "code_handler_gecko_registers.h"

unsigned int integer_registers[GECKO_REGISTERS_COUNT];
float float_registers[GECKO_REGISTERS_COUNT];

void initialize_gecko_registers() {
	memset(integer_registers, 0, sizeof(integer_registers));
	memset(float_registers, 0, sizeof(float_registers));
}

int get_address_increment(enum ValueSize value_size) {
	int value_increment = 0;
	if (is_big_endian()) {
		value_increment = sizeof(int);

		switch (value_size) {
			case VALUE_SIZE_EIGHT_BIT:
				return value_increment - sizeof(char);

			case VALUE_SIZE_SIXTEEN_BIT:
				return value_increment - sizeof(short);

			case VALUE_SIZE_THIRTY_TWO_BIT:
				return value_increment - sizeof(int);
		}

		OSFatal("Value size unmatched");
		return -1;
	}

	return value_increment;
}

void load_integer(const unsigned char *registerIndexPointer, enum ValueSize valueSize,
				  unsigned char *addressPointer) {
	int registerIndex = *registerIndexPointer;
	unsigned int value = read_real_value(valueSize, addressPointer);
	integer_registers[registerIndex] = value;
}

void store_integer(const unsigned char *register_index_pointer,
				   enum ValueSize value_size, unsigned char *pointer_to_address) {
	int address_increment = get_address_increment(value_size);
	int register_index = *register_index_pointer;
	auto target_value = (unsigned char *) (integer_registers + register_index);
	unsigned int value = 0;

	switch (value_size) {
		case VALUE_SIZE_EIGHT_BIT:
			value = *(target_value + address_increment);
			break;

		case VALUE_SIZE_SIXTEEN_BIT:
			value = *((unsigned short *) (target_value + address_increment));
			break;

		case VALUE_SIZE_THIRTY_TWO_BIT:
			value = *((unsigned int *) (target_value + address_increment));
			break;
	}

	auto address_pointer = (unsigned char *) (long) read_real_integer(pointer_to_address);
	log_printf("[STORE_INTEGER] Storing %p {Value Size: %i} at address %p...\n", (void *) (long) value,
			   get_bytes(value_size), (void *) address_pointer);

	if (real_memory_accesses_are_enabled) {
		*(int *) address_pointer = value;
	}
}

void load_float(const unsigned char *register_index_pointer, unsigned char *address_pointer) {
	int register_index = *register_index_pointer;
	unsigned int value = read_real_value(VALUE_SIZE_THIRTY_TWO_BIT, address_pointer);
	float floatValue;
	memcpy(&floatValue, &value, sizeof(int));
	float_registers[register_index] = floatValue;
	log_printf("Stored value: %.2f\n", float_registers[register_index]);
}

void store_float(const unsigned char *register_index_pointer, unsigned char *pointer_to_address) {
	int register_index = *register_index_pointer;
	float value = float_registers[register_index];
	auto address_pointer = (unsigned char *) (long) read_real_integer(pointer_to_address);
	log_printf("[STORE_FLOAT] Storing %.2f at address %p...\n", value, (void *) address_pointer);

	if (real_memory_accesses_are_enabled) {
		*(float *) address_pointer = value;
	}
}

void apply_float_register_operation(const unsigned char *first_register_index_pointer,
									const unsigned char *second_register_index_pointer,
									enum FloatRegisterOperation register_operation) {
	int first_register_index = *first_register_index_pointer;
	int second_register_index = *second_register_index_pointer;
	float first_register_value = float_registers[first_register_index];
	float second_register_value = float_registers[second_register_index];

	switch (register_operation) {
		case FLOAT_REGISTER_OPERATION_ADDITION:
			float_registers[first_register_index] = first_register_value + second_register_value;
			break;

		case FLOAT_REGISTER_OPERATION_SUBTRACTION:
			float_registers[first_register_index] = first_register_value - second_register_value;
			break;

		case FLOAT_REGISTER_OPERATION_MULTIPLICATION:
			float_registers[first_register_index] = first_register_value * second_register_value;
			break;

		case FLOAT_REGISTER_OPERATION_DIVISION:
			float_registers[first_register_index] = first_register_value / second_register_value;
			break;

		case FLOAT_REGISTER_OPERATION_FLOAT_TO_INTEGER:
			integer_registers[second_register_index] = (unsigned int) first_register_value;
			log_printf("Integer register value: %f converted -> %d\n", first_register_value,
					   integer_registers[second_register_index]);
			break;

		default:
			OSFatal("Invalid float register operation");
			break;
	}
}

void applyFloat_direct_value_operation(const unsigned char *first_register_index_pointer,
									   unsigned char *value_pointer,
									   enum FloatDirectValueOperation direct_value_operation) {
	int register_index = *first_register_index_pointer;
	unsigned int current_value = integer_registers[register_index];
	unsigned int value = read_real_integer(value_pointer);

	switch (direct_value_operation) {
		case FLOAT_DIRECT_VALUE_OPERATION_ADDITION:
			integer_registers[register_index] = current_value + value;
			break;

		case FLOAT_DIRECT_VALUE_OPERATION_SUBTRACTION:
			integer_registers[register_index] = current_value - value;
			break;

		case FLOAT_DIRECT_VALUE_OPERATION_DIVISION:
			integer_registers[register_index] = current_value / value;
			break;

		case FLOAT_DIRECT_VALUE_OPERATION_MULTIPLICATION:
			integer_registers[register_index] = current_value * value;
			break;

		default:
			OSFatal("Invalid direct float value operation");
			break;
	}
}

void apply_integer_register_operation(const unsigned char *first_register_index_pointer,
									  const unsigned char *second_register_index_pointer,
									  enum IntegerRegisterOperation register_operation) {
	int first_register_index = *first_register_index_pointer;
	int second_register_index = *second_register_index_pointer;
	unsigned int first_register_value = integer_registers[first_register_index];
	unsigned int second_register_value = integer_registers[second_register_index];

	switch (register_operation) {
		case INTEGER_REGISTER_OPERATION_ADDITION:
			integer_registers[first_register_index] = first_register_value + second_register_value;
			break;

		case INTEGER_REGISTER_OPERATION_SUBTRACTION:
			integer_registers[first_register_index] = first_register_value - second_register_value;
			break;

		case INTEGER_REGISTER_OPERATION_MULTIPLICATION:
			integer_registers[first_register_index] = first_register_value * second_register_value;
			break;

		case INTEGER_REGISTER_OPERATION_DIVISION:
			integer_registers[first_register_index] = first_register_value / second_register_value;
			break;

		default:
			OSFatal("Invalid integer register operation");
			break;
	}
}

void apply_integer_direct_value_operation(const unsigned char *first_register_index_pointer,
										  unsigned char *value_pointer,
										  enum IntegerDirectValueOperation direct_value_operation) {
	int register_index = *first_register_index_pointer;
	unsigned int current_value = integer_registers[register_index];
	unsigned int value = read_real_integer(value_pointer);

	switch (direct_value_operation) {
		case INTEGER_DIRECT_VALUE_OPERATION_ADDITION:
			integer_registers[register_index] = current_value + value;
			break;

		case INTEGER_DIRECT_VALUE_OPERATION_SUBTRACTION:
			integer_registers[register_index] = current_value - value;
			break;

		case INTEGER_DIRECT_VALUE_OPERATION_MULTIPLICATION:
			integer_registers[register_index] = current_value * value;
			break;

		case INTEGER_DIRECT_VALUE_OPERATION_DIVISION:
			integer_registers[register_index] = current_value / value;
			break;

		default:
			OSFatal("Invalid integer direct value operation");
			break;
	}
}