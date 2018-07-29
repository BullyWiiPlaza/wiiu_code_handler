#pragma once

#include "operating_system/operating_system_utilities.h"

enum CodeType {
	CODE_TYPE_RAM_WRITE = 0x00,
	CODE_TYPE_STRING_WRITE = 0x01,
	CODE_TYPE_SKIP_WRITE = 0x02,
	CODE_TYPE_IF_EQUAL = 0x03,
	CODE_TYPE_IF_NOT_EQUAL = 0x04,
	CODE_TYPE_IF_GREATER = 0x05,
	CODE_TYPE_IF_LESS = 0x06,
	CODE_TYPE_IF_GREATER_THAN_OR_EQUAL = 0x07,
	CODE_TYPE_IF_LESS_THAN_OR_EQUAL = 0x08,
	CODE_TYPE_AND = 0x09,
	CODE_TYPE_OR = 0x0A,
	CODE_TYPE_IF_VALUE_BETWEEN = 0x0B,
	CODE_TYPE_ADD_TIME_DEPENDENCE = 0x0C,
	CODE_TYPE_RESET_TIMER = 0x0D,
	CODE_TYPE_LOAD_INTEGER = 0x10,
	CODE_TYPE_STORE_INTEGER = 0x11,
	CODE_TYPE_LOAD_FLOAT = 0x12,
	CODE_TYPE_STORE_FLOAT = 0x13,
	CODE_TYPE_INTEGER_OPERATION = 0x14,
	CODE_TYPE_FLOAT_OPERATION = 0x15,
	CODE_TYPE_FILL_MEMORY_AREA = 0x20,
	CODE_TYPE_LOAD_POINTER = 0x30,
	CODE_TYPE_ADD_OFFSET_TO_POINTER = 0x31,
	CODE_TYPE_LOAD_POINTER_DIRECTLY = 0x32,
	CODE_TYPE_EXECUTE_ASSEMBLY = 0xC0,
	CODE_TYPE_PERFORM_SYSTEM_CALL = 0xC1,
	CODE_TYPE_TERMINATOR = 0xD0,
	CODE_TYPE_NO_OPERATION = 0xD1,
	CODE_TYPE_TIMER_TERMINATION = 0xD2,
	CODE_TYPE_CORRUPTER = 0xF0,
	CODE_TYPE_SEARCH_TEMPLATE = 0xF6,
	CODE_TYPE_PROCEDURE_CALL = 0xF8,
};

enum ValueSize {
	VALUE_SIZE_EIGHT_BIT = 0x00,
	VALUE_SIZE_SIXTEEN_BIT = 0x01,
	VALUE_SIZE_THIRTY_TWO_BIT = 0x02
};

enum IntegerRegisterOperation {
	INTEGER_REGISTER_OPERATION_ADDITION = 0x00,
	INTEGER_REGISTER_OPERATION_SUBTRACTION = 0x01,
	INTEGER_REGISTER_OPERATION_MULTIPLICATION = 0x02,
	INTEGER_REGISTER_OPERATION_DIVISION = 0x03
};

enum IntegerDirectValueOperation {
	INTEGER_DIRECT_VALUE_OPERATION_ADDITION = 0x04,
	INTEGER_DIRECT_VALUE_OPERATION_SUBTRACTION = 0x05,
	INTEGER_DIRECT_VALUE_OPERATION_DIVISION = 0x06,
	INTEGER_DIRECT_VALUE_OPERATION_MULTIPLICATION = 0x07,
};

enum FloatRegisterOperation {
	FLOAT_REGISTER_OPERATION_ADDITION = 0x00,
	FLOAT_REGISTER_OPERATION_SUBTRACTION = 0x01,
	FLOAT_REGISTER_OPERATION_MULTIPLICATION = 0x02,
	FLOAT_REGISTER_OPERATION_DIVISION = 0x03,
	FLOAT_REGISTER_OPERATION_FLOAT_TO_INTEGER = 0x08
};

enum FloatDirectValueOperation {
	FLOAT_DIRECT_VALUE_OPERATION_ADDITION = 0x04,
	FLOAT_DIRECT_VALUE_OPERATION_SUBTRACTION = 0x05,
	FLOAT_DIRECT_VALUE_OPERATION_MULTIPLICATION = 0x06,
	FLOAT_DIRECT_VALUE_OPERATION_DIVISION = 0x07,
};

enum ComparisonType {
	COMPARISON_TYPE_EQUAL = CODE_TYPE_IF_EQUAL,
	COMPARISON_TYPE_NOT_EQUAL = CODE_TYPE_IF_NOT_EQUAL,
	COMPARISON_TYPE_GREATER_THAN = CODE_TYPE_IF_GREATER,
	COMPARISON_TYPE_LESS_THAN = CODE_TYPE_IF_LESS,
	COMPARISON_TYPE_GREATER_THAN_OR_EQUAL = CODE_TYPE_IF_GREATER_THAN_OR_EQUAL,
	COMPARISON_TYPE_LESS_THAN_OR_EQUAL = CODE_TYPE_IF_LESS_THAN_OR_EQUAL,
	COMPARISON_TYPE_AND = CODE_TYPE_AND,
	COMPARISON_TYPE_OR = CODE_TYPE_OR,
	COMPARISON_TYPE_VALUE_BETWEEN = CODE_TYPE_IF_VALUE_BETWEEN,
};

int getBytes(enum ValueSize valueSize);

enum Pointer {
	POINTER_NO_POINTER = 0x00,
	POINTER_POINTER = 0x01
};