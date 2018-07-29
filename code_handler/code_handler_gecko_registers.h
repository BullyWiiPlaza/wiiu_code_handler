#pragma once

#include "code_handler_functions.h"
#include "general/endian.h"

#define GECKO_REGISTERS_COUNT 8

extern unsigned int integer_registers[GECKO_REGISTERS_COUNT];
extern float float_registers[GECKO_REGISTERS_COUNT];

void initialize_gecko_registers();

void load_integer(const unsigned char *registerIndexPointer, enum ValueSize valueSize,
				  unsigned char *addressPointer);

void store_integer(const unsigned char *register_index_pointer,
				   enum ValueSize value_size, unsigned char *pointer_to_address);

void load_float(const unsigned char *register_index_pointer, unsigned char *address_pointer);

void store_float(const unsigned char *register_index_pointer, unsigned char *pointer_to_address);

void apply_float_register_operation(const unsigned char *first_register_index_pointer,
									const unsigned char *second_register_index_pointer,
									enum FloatRegisterOperation register_operation);

void applyFloat_direct_value_operation(const unsigned char *first_register_index_pointer,
									   unsigned char *value_pointer,
									   enum FloatDirectValueOperation direct_value_operation);

void apply_integer_register_operation(const unsigned char *first_register_index_pointer,
									  const unsigned char *second_register_index_pointer,
									  enum IntegerRegisterOperation register_operation);

void apply_integer_direct_value_operation(const unsigned char *first_register_index_pointer,
										  unsigned char *value_pointer,
										  enum IntegerDirectValueOperation direct_value_operation);