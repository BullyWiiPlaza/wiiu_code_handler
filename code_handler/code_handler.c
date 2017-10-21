#include "code_handler.h"

#include <limits.h>
#include <stdbool.h>
#include "operating_system/operating_system_utilities.h"
#include "code_handler_functions.h"
#include "general/endian.h"
#include "general/rounding.h"
#include "general/bit_manipulations.h"
#include "architecture_specific.h"
#include "error_handling.h"
#include "code_handler_gecko_registers.h"

/* State variables of the code handler */
bool conditionFlag;
unsigned int loadedPointer = 0;
unsigned int timeDependenceDelay = 0;
unsigned int codeHandlerExecutionsCount;

#define CODE_LINE_BYTES 8

void parseRegularCode(unsigned char *codes, const enum CodeType *codeType,
					  enum Pointer *pointer, enum ValueSize *valueSize,
					  int *bytesIndex, unsigned char **address,
					  unsigned char **value, int *valueBytes) {
	char pointerAndValueSize = codes[(*bytesIndex)];
	(*pointer) = (enum Pointer) getUpperNibble(pointerAndValueSize);
	log_printf("Pointer: %i\n", (*pointer));

	switch (*codeType) {
		case CODE_TYPE_STRING_WRITE:
			(*bytesIndex)++;
			unsigned char *valueBytesAddress = &codes[(*bytesIndex)];
			(*valueBytes) = readRealShort(valueBytesAddress);
			(*bytesIndex) += 2;
			break;

		default:
			/*case RAM_WRITE:
		case IF_EQUAL:
		case IF_NOT_EQUAL:
		case IF_GREATER:
		case IF_LESS:
		case IF_GREATER_OR_EQUAL:
		case IF_LESS_THAN_OR_EQUAL:*/
			(*valueSize) = (enum ValueSize) getLowerNibble(pointerAndValueSize);
			log_printf("Value Size: %i\n", *valueSize);

			if (*codeType == CODE_TYPE_INTEGER_OPERATION
				|| *codeType == CODE_TYPE_FLOAT_OPERATION) {
				(*valueBytes) = getBytes(VALUE_SIZE_THIRTY_TWO_BIT);
			} else {
				(*valueBytes) = getBytes(*valueSize);
			}

			(*bytesIndex) += 3;
			break;
	}

	(*address) = &codes[(*bytesIndex)];
	(*bytesIndex) += sizeof(int);

	if ((*pointer) == POINTER_POINTER) {
		log_printf("Offset: %p\n", (void *) (long) readRealInteger((*address)));
		log_print("Checking loaded pointer...\n");
		if (loadedPointer != 0) {
			log_print("Applying loaded pointer...\n");
			(*address) += loadedPointer;
		} else {
			log_print("Invalid pointer, not executing...\n");
		}
		log_printf("Address: %p\n", (void *) (long) readRealInteger((*address)));
	} else {
		log_printf("Address: %p\n", (void *) (long) readRealInteger((*address)));
	}

	switch (*codeType) {
		case CODE_TYPE_LOAD_POINTER:
			(*value) = &codes[(*bytesIndex)];
			log_printf("Range start: %p\n", (void *) (long) readRealInteger(*value));
			break;

		default:
			(*value) = &codes[(*bytesIndex)];
			log_printf("Value: %p {Length: %i}\n", (void *) (long) readRealInteger(*value), *valueBytes);
			break;
	}

	switch (*codeType) {
		case CODE_TYPE_STRING_WRITE:;
			int increment = roundUp(*valueBytes, CODE_LINE_BYTES);
			(*bytesIndex) += increment;
			break;

		default:
			(*bytesIndex) += sizeof(int) * 2;
	}
}

bool isLoadedPointerValid(const enum Pointer *pointer) {
	return ((*pointer) == POINTER_POINTER && loadedPointer != 0) || (*pointer) == POINTER_NO_POINTER;
}

#define CODE_LINE_POINTER_MAXIMUM_INCREMENT INT_MAX

int getCodeLinePointerIncrement(unsigned char *haystack, const unsigned char *needle, int maximumDistance) {
	unsigned char *currentLocation = haystack;
	size_t needleSize = sizeof(needle);

	while (currentLocation - haystack < maximumDistance) {
		for (int needleIndex = 0; needleIndex < needleSize; needleIndex++) {
			unsigned char needleCharacter = needle[needleIndex];
			unsigned char currentCharacter = currentLocation[needleIndex];
			if (currentCharacter != needleCharacter) {
				goto incrementCurrentLocation;
			}
		}

		return (int) (currentLocation - haystack);

		incrementCurrentLocation:
		currentLocation++;
	}

	return CODE_LINE_POINTER_MAXIMUM_INCREMENT;
}

unsigned char terminatorCodeLine[] = {CODE_TYPE_TERMINATOR, 0x00, 0x00, 0x00, 0xDE, 0xAD, 0xCA, 0xFE};
unsigned char resetTimerCodeLine[] = {CODE_TYPE_RESET_TIMER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void branchToAfterTerminator(unsigned char *codes, const int *codeLineIndex,
							 int *bytesIndex, unsigned char *terminator) {
	unsigned char *currentCodePosition = codes + (*bytesIndex);

	int remainingCodeLength = (*codeLineIndex) - (*bytesIndex);
	int increment = getCodeLinePointerIncrement(currentCodePosition, terminator, remainingCodeLength);

	if (increment == CODE_LINE_POINTER_MAXIMUM_INCREMENT) {
		char messageBuffer[MESSAGE_BUFFER_SIZE] = "Condition not terminated";
		setErrorMessageBuffer(messageBuffer, codes);
		OSFatal(messageBuffer);
	} else {
		log_printf("Skipping ahead by %p bytes...\n", (void *) (long) increment);
		(*bytesIndex) += increment;
	}
}

void parseConditionalCode(unsigned char *codes, enum Pointer *pointer,
						  enum ValueSize *valueSize, int *bytesIndex,
						  unsigned char **address, unsigned char **value,
						  int *valueBytes, int *length, enum ComparisonType comparisonType) {
	enum CodeType codeType = CODE_TYPE_IF_NOT_EQUAL;
	parseRegularCode(codes, &codeType, pointer, valueSize, bytesIndex, address, value, valueBytes);
	unsigned char *upperValue = &codes[(*bytesIndex - sizeof(int))];

	if (isLoadedPointerValid(pointer)) {
		conditionFlag = compareValue(*address, *value, valueSize, upperValue, *valueBytes, comparisonType);
		log_printf("Comparison result: %i\n", conditionFlag);
		branchToAfterTerminator(codes, length, bytesIndex, terminatorCodeLine);
	}
}

void runCodeHandlerInternal(unsigned char *codes, int length) {
	int bytesIndex = 0;
	enum CodeType codeType = (enum CodeType) codes[bytesIndex];
	log_printf("Code Type: 0x%x\n", codeType);
	bytesIndex++;

	enum Pointer pointer;
	enum ValueSize valueSize;
	unsigned char *address;
	unsigned char *value;
	int valueBytes;

	switch (codeType) {
		case CODE_TYPE_RAM_WRITE:
			log_printf("### RAM Write ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);

			if (isLoadedPointerValid(&pointer)) {
				writeValue(address, value, valueBytes);
			}
			break;

		case CODE_TYPE_STRING_WRITE:
			log_printf("### String Write ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);

			if (isLoadedPointerValid(&pointer)) {
				writeString(address, value, valueBytes);
			}
			break;

		case CODE_TYPE_SKIP_WRITE:
			log_printf("### Skip Write ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);
			bytesIndex -= sizeof(int);
			unsigned char *stepSize = &codes[bytesIndex];
			log_printf("Step size: %p\n", (void *) (long) readRealInteger(stepSize));
			bytesIndex += sizeof(int);
			unsigned char *increment = &codes[bytesIndex];
			log_printf("Value increment: %p\n", (void *) (long) readRealInteger(increment));
			bytesIndex += sizeof(int);
			bytesIndex += sizeof(int);
			unsigned char *iterationsCount = &codes[2];
			log_printf("Iterations count: %i\n", readRealShort(iterationsCount));

			if (isLoadedPointerValid(&pointer)) {
				skipWriteMemory(address, value, valueBytes, stepSize, increment, iterationsCount);
			}
			break;

		case CODE_TYPE_IF_EQUAL:
			log_printf("### If equal ###\n");
			parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &length,
								 (enum ComparisonType) codeType);
			break;

		case CODE_TYPE_IF_NOT_EQUAL:
			log_printf("### If not equal ###\n");
			parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &length,
								 (enum ComparisonType) codeType);
			break;

		case CODE_TYPE_IF_GREATER:
			log_printf("### If greater than ###\n");
			parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &length,
								 (enum ComparisonType) codeType);
			break;

		case CODE_TYPE_IF_LESS:
			log_printf("### If less than ###\n");
			parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &length,
								 (enum ComparisonType) codeType);
			break;

		case CODE_TYPE_IF_GREATER_THAN_OR_EQUAL:
			log_printf("### If greater than or equal ###\n");
			parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &length,
								 (enum ComparisonType) codeType);
			break;

		case CODE_TYPE_IF_LESS_THAN_OR_EQUAL:
			log_printf("### If less than or equal ###\n");
			parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &length,
								 (enum ComparisonType) codeType);
			break;

		case CODE_TYPE_AND:
			log_printf("### Apply AND ###\n");
			parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &length,
								 (enum ComparisonType) codeType);
			break;

		case CODE_TYPE_OR:
			log_printf("### Apply OR ###\n");
			parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &length,
								 (enum ComparisonType) codeType);
			break;

		case CODE_TYPE_IF_VALUE_BETWEEN:
			log_printf("### If Value Between ###\n");
			parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &length,
								 (enum ComparisonType) codeType);
			break;

		case CODE_TYPE_ADD_TIME_DEPENDENCE:
			log_printf("### Add Time Dependence ###\n");
			bytesIndex += sizeof(int) - sizeof(char);
			value = &codes[bytesIndex];
			timeDependenceDelay = readRealInteger(value);
			log_printf("[ADD_TIME_DEPENDENCE] %i\n", timeDependenceDelay);

			if (timeDependenceDelay != 0 && codeHandlerExecutionsCount % timeDependenceDelay != 0) {
				branchToAfterTerminator(codes, &length, &bytesIndex, resetTimerCodeLine);
			} else {
				log_printf("Not branching: Executions count: %i\n", codeHandlerExecutionsCount);
			}

			bytesIndex += sizeof(int);
			break;

		case CODE_TYPE_RESET_TIMER:
			log_printf("### Reset Timer ###\n");
			bytesIndex += sizeof(resetTimerCodeLine) - 1;
			timeDependenceDelay = 0;
			log_print("[RESET_TIMER] Executed\n");
			break;

		case CODE_TYPE_LOAD_INTEGER:
			log_printf("### Load Integer ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);
			int excessiveBytes = sizeof(int) * 2;
			unsigned char *registerIndex = &codes[bytesIndex - sizeof(int) - excessiveBytes - sizeof(char)];
			log_printf("Register index: %i\n", *registerIndex);
			bytesIndex -= excessiveBytes;
			loadInteger(registerIndex, valueSize, address);
			break;

		case CODE_TYPE_STORE_INTEGER:
			log_printf("### Store Integer ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);
			excessiveBytes = sizeof(int) * 2;
			registerIndex = &codes[bytesIndex - sizeof(int) - excessiveBytes - sizeof(char)];
			log_printf("Register index: %i\n", *registerIndex);
			bytesIndex -= excessiveBytes;
			storeInteger(registerIndex, valueSize, address);
			break;

		case CODE_TYPE_LOAD_FLOAT:
			log_printf("### Load Float ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);
			excessiveBytes = sizeof(int) * 2;
			registerIndex = &codes[bytesIndex - sizeof(int) - excessiveBytes - sizeof(char)];
			log_printf("Register index: %i\n", *registerIndex);
			bytesIndex -= excessiveBytes;
			loadFloat(registerIndex, address);
			break;

		case CODE_TYPE_STORE_FLOAT:
			log_printf("### Store Float ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);
			excessiveBytes = sizeof(int) * 2;
			registerIndex = &codes[bytesIndex - sizeof(int) - excessiveBytes - sizeof(char)];
			log_printf("Register index: %i\n", *registerIndex);
			bytesIndex -= excessiveBytes;
			storeFloat(registerIndex, address);
			break;

		case CODE_TYPE_INTEGER_OPERATION:
			log_printf("### Integer Operation ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);
			excessiveBytes = sizeof(int) * 2;
			int integerOperationIndex = bytesIndex - sizeof(int) - excessiveBytes - sizeof(char) - 2;
			unsigned char *integerOperationPointer = &codes[integerOperationIndex];
			unsigned char *integerDestinationRegisterPointer = &codes[integerOperationIndex + 1];
			unsigned char *integerSecondOperandRegisterPointer = &codes[integerOperationIndex + 2];
			int integerOperationType = *integerOperationPointer;

			if (integerOperationType > INTEGER_REGISTER_OPERATION_DIVISION) {
				enum IntegerDirectValueOperation directValueOperation = (enum IntegerDirectValueOperation) integerOperationType;
				applyIntegerDirectValueOperation(integerDestinationRegisterPointer, address, directValueOperation);
			} else {
				enum IntegerRegisterOperation registerOperation = (enum IntegerRegisterOperation) integerOperationType;
				applyIntegerRegisterOperation(integerDestinationRegisterPointer, integerSecondOperandRegisterPointer,
											  registerOperation);
			}

			bytesIndex -= sizeof(int) * 2;

			break;

		case CODE_TYPE_FLOAT_OPERATION:
			log_printf("### Float Operation ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);
			excessiveBytes = sizeof(int) * 2;
			int floatOperationIndex = bytesIndex - sizeof(int) - excessiveBytes - sizeof(char) - 2;
			unsigned char *floatOperationPointer = &codes[floatOperationIndex];
			unsigned char *floatDestinationRegisterPointer = &codes[floatOperationIndex + 1];
			unsigned char *floatSecondOperandRegisterPointer = &codes[floatOperationIndex + 2];
			int floatOperationType = *floatOperationPointer;

			if (floatOperationType > FLOAT_REGISTER_OPERATION_DIVISION &&
				floatOperationType < FLOAT_REGISTER_OPERATION_FLOAT_TO_INTEGER) {
				enum FloatDirectValueOperation directValueOperation = (enum FloatDirectValueOperation) floatOperationType;
				applyFloatDirectValueOperation(floatDestinationRegisterPointer, address, directValueOperation);
			} else {
				enum FloatRegisterOperation registerOperation = (enum FloatRegisterOperation) floatOperationType;
				applyFloatRegisterOperation(floatDestinationRegisterPointer, floatSecondOperandRegisterPointer,
											registerOperation);
			}

			bytesIndex -= sizeof(int) * 2;
			break;

		case CODE_TYPE_FILL_MEMORY_AREA:
			log_printf("### Fill Memory Area ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);
			int offset = readRealInteger(&codes[bytesIndex - sizeof(int)]);
			log_printf("Offset: %p\n", (void *) (long) offset);
			if (isLoadedPointerValid(&pointer)) {
				unsigned int stepSizeInt = 4;
				stepSize = getCharacterPointer(&stepSizeInt, sizeof(int));
				unsigned int incrementInt = 0;
				increment = getCharacterPointer(&incrementInt, sizeof(int));
				unsigned int iterationsCountInt = offset / sizeof(int);
				iterationsCount = getCharacterPointer(&iterationsCountInt, sizeof(short));
				skipWriteMemory(address, value, sizeof(int), stepSize, increment, iterationsCount);
			}

			break;

		case CODE_TYPE_LOAD_POINTER:
			log_printf("### Load Pointer ###\n");
			parseRegularCode(codes, &codeType, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes);
			loadedPointer = loadPointer(address, value);
			break;

		case CODE_TYPE_ADD_OFFSET_TO_POINTER:;
			log_printf("### Add To Pointer ###\n");
			bytesIndex += 3;
			int pointerOffset = readRealInteger(&codes[bytesIndex]);
			loadedPointer += pointerOffset;
			log_printf("[ADD_POINTER]: *%p += %p\n", (void *) (long) loadedPointer, (void *) (long) pointerOffset);
			bytesIndex += sizeof(int);
			break;

		case CODE_TYPE_EXECUTE_ASSEMBLY:
			log_printf("### Execute Assembly ###\n");
			bytesIndex++;
			int assemblyLines = readRealShort(&codes[bytesIndex]);
			int assemblyBytesCount = assemblyLines * CODE_LINE_BYTES;
			bytesIndex += sizeof(short);
			bytesIndex += sizeof(int);
			value = &codes[bytesIndex];
			bytesIndex += (assemblyLines * CODE_LINE_BYTES);
			executeAssembly(value, assemblyBytesCount);
			break;

		case CODE_TYPE_PERFORM_SYSTEM_CALL:
			log_printf("### Perform System Call ###\n");
			bytesIndex++;
			value = &codes[bytesIndex];
			bytesIndex += sizeof(short);
			bytesIndex += sizeof(int);
			executeSystemCall(readRealShort(value));
			break;

		case CODE_TYPE_TERMINATOR:
			log_printf("### Terminator ###\n");
			bytesIndex += sizeof(terminatorCodeLine) - 1;
			log_printf("[Before] Loaded pointer: %p\n", (void *) (long) loadedPointer);
			log_printf("[Before] Condition flag: %i\n", conditionFlag);
			conditionFlag = false;
			loadedPointer = 0;
			log_printf("[After] Loaded pointer: %p\n", (void *) (long) loadedPointer);
			log_printf("[After] Condition flag: %i\n", conditionFlag);
			break;

		case CODE_TYPE_NO_OPERATION:
			log_printf("### No Operation ###\n");
			bytesIndex += 3;
			bytesIndex += sizeof(int);
			log_print("[NO_OPERATION] Do nothing\n");
			break;

		case CODE_TYPE_TIMER_TERMINATION:
			log_printf("### Timer Termination ###\n");
			bytesIndex += sizeof(terminatorCodeLine) - 1;
			log_print("[TIMER_TERMINATION] Executed\n");
			break;

		case CODE_TYPE_CORRUPTER:
			log_printf("### Corrupter ###\n");
			bytesIndex += 3;

			unsigned char *beginning = &codes[bytesIndex];
			log_printf("Beginning: %p\n", (void *) (long) readRealInteger(beginning));
			bytesIndex += sizeof(int);

			unsigned char *end = &codes[bytesIndex];
			log_printf("End: %p\n", (void *) (long) readRealInteger(end));
			bytesIndex += sizeof(int);

			unsigned char *searchValue = &codes[bytesIndex];
			log_printf("Search value: %p\n", (void *) (long) readRealInteger(searchValue));
			bytesIndex += sizeof(int);

			unsigned char *replacementValue = &codes[bytesIndex];
			log_printf("Replacement value: %p\n", (void *) (long) readRealInteger(replacementValue));
			bytesIndex += sizeof(int);

			bytesIndex += sizeof(int);

			applyCorrupter(beginning, end, searchValue, replacementValue);
			break;

		default:;
#define ERROR_BUFFER_SIZE 100
			char errorMessageBuffer[ERROR_BUFFER_SIZE];
			snprintf(errorMessageBuffer, ERROR_BUFFER_SIZE, "Unhandled code type: %i\n", codeType);
			OSFatal(errorMessageBuffer);
			break;
	}

	// Those have been consumed
	length -= bytesIndex;

	// Is not supposed to happen
	if (length < 0) {
		char errorMessageBuffer[ERROR_BUFFER_SIZE];
		snprintf(errorMessageBuffer, ERROR_BUFFER_SIZE, "Negative length: %i", length);
		OSFatal(errorMessageBuffer);
	}

	if (length % CODE_LINE_BYTES != 0) {
		char errorMessageBuffer[ERROR_BUFFER_SIZE];
		snprintf(errorMessageBuffer, ERROR_BUFFER_SIZE, "Illegal code length: %i", length);
		OSFatal(errorMessageBuffer);
	}

	// Keep running the code handler till all codes are handled
	if (length > 0) {
		printf("\n");
		return runCodeHandler(codes + bytesIndex, length);
	}

	// One full code handler execution is done
	codeHandlerExecutionsCount++;
}

void initializeAllStates() {
	conditionFlag = false;
	loadedPointer = 0;
	codeHandlerExecutionsCount = 0;
	timeDependenceDelay = 0;
	initializeGeckoRegisters();
}

void runCodeHandler(unsigned char *codes, int length) {
	initializeAllStates();
	runCodeHandlerInternal(codes, length);
}