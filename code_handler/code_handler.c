#include "code_handler.h"

#include <limits.h>
#include <stdbool.h>
#include <asm/byteorder.h>
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
unsigned int codeHandlerExecutionsCount = 0;

#define ILLEGAL_POINTER 0

/* Reset during code handler executions */
bool conditionFlag;
uintptr_t loadedPointer;
unsigned int timeDependenceDelay;

#define CODE_LINE_BYTES 8

void applyTimerReset() {
	timeDependenceDelay = 0;
}

void applyTerminator() {
	conditionFlag = false;
	loadedPointer = ILLEGAL_POINTER;
}

#define MAXIMUM_ARGUMENTS_COUNT 8

int handleProcedureCall(const unsigned char *codes, int bytesIndex) {
	unsigned int lowerDestinationRegisterIndex = codes[bytesIndex];
	log_printf("Lower destination register index: %i\n", lowerDestinationRegisterIndex);
	if (lowerDestinationRegisterIndex > (GECKO_REGISTERS_COUNT - 1)) {
		OSFatal("Lower destination register index exceeded");
	}

	bytesIndex += sizeof(char);
	unsigned int upperDestinationRegisterIndex = codes[bytesIndex];
	log_printf("Upper destination register index: %i\n", upperDestinationRegisterIndex);
	if (upperDestinationRegisterIndex > (GECKO_REGISTERS_COUNT - 1)) {
		OSFatal("Upper destination register index exceeded");
	}

	bytesIndex += sizeof(char);
	unsigned int argumentsCount = codes[bytesIndex];
	log_printf("Arguments Count: %i\n", argumentsCount);
	if (argumentsCount > MAXIMUM_ARGUMENTS_COUNT) {
		OSFatal("Maximum arguments count exceeded");
	}

	bytesIndex += sizeof(char);
	uintptr_t realAddress = readRealInteger(&codes[bytesIndex]);
	log_printf("Function Address: %p\n", (void *) realAddress);
	bytesIndex += sizeof(int);
	log_print("Allocating arguments...\n");
	unsigned int *arguments = (unsigned int *) calloc(MAXIMUM_ARGUMENTS_COUNT, sizeof(int));

	if (arguments == 0) {
		OSFatal("Allocating arguments failed");
	} else {
		log_print("Allocated!\n");
		for (unsigned int argumentIndex = 0; argumentIndex < argumentsCount; argumentIndex++) {
			arguments[argumentIndex] = readRealInteger(&codes[bytesIndex]);
			bytesIndex += sizeof(int);
		}

		// If this is uneven we skip another 32-bit value since it wasn't an argument
		if (argumentsCount % 2 != 0) {
			bytesIndex += sizeof(int);
		}

		unsigned long returnValue = callAddress(realAddress, arguments);

		log_printf("Return Value: %llx\n", (unsigned long long int) returnValue);
		integerRegisters[lowerDestinationRegisterIndex] = (unsigned int) returnValue;
		integerRegisters[upperDestinationRegisterIndex] = (unsigned int) (returnValue >> 32u);

		free(arguments);
	}

	return bytesIndex;
}

void applySearchTemplateCodeType(const unsigned char *codes, int *length, int *bytesIndex) {
	unsigned short targetMatchIndex = readRealShort(&codes[(*bytesIndex)]);
	log_printf("Target Match Index: %i\n", targetMatchIndex);
	(*bytesIndex) += sizeof(short);
	unsigned int searchTemplateLength = codes[(*bytesIndex)];
	log_printf("Search Template Length: %i\n", searchTemplateLength);
	(*bytesIndex) += sizeof(char);
	uintptr_t startAddress = (unsigned int) (readRealShort(&codes[(*bytesIndex)]) * 0x10000);
	log_printf("Start Address: %p\n", (void *) (int *) startAddress);
	(*bytesIndex) += sizeof(short);
	uintptr_t endAddress = (unsigned int) (readRealShort(&codes[(*bytesIndex)]) * 0x10000);
	log_printf("End Address: %p\n", (void *) (int *) endAddress);
	(*bytesIndex) += sizeof(short);
	const unsigned char *searchTemplate = &codes[(*bytesIndex)];
	bool templateFound = false;
	uintptr_t foundAddress = searchForTemplate(searchTemplate, searchTemplateLength, startAddress,
											   endAddress, 1, targetMatchIndex, &templateFound);
	log_printf("Template Found: %i\n", templateFound);

	// Code type header plus search template lines
	unsigned char addressLoaderCodeLine[CODE_LINE_BYTES] = {CODE_TYPE_LOAD_POINTER_DIRECTLY, 0x00, 0x00, 0x00,
															0x00, 0x00, 0x00, 0x00};

	// Find the amount of bytes taken in total by the search template line(s)
	int occupiedSearchTemplateLength = searchTemplateLength;
	int remainder = searchTemplateLength % CODE_LINE_BYTES;
	if (remainder != 0) {
		occupiedSearchTemplateLength += CODE_LINE_BYTES - remainder;
	}

	if (templateFound) {
		log_printf("Found Address: %p\n", (void *) (int *) foundAddress);

		// Get the byte order right
		foundAddress = isBigEndian() ? foundAddress : htonl((uint32_t) foundAddress);
	}

	// Modify the code list by pasting the load pointer statement over the search code type
	*(unsigned int *) (addressLoaderCodeLine + sizeof(int)) = (unsigned int) foundAddress;
	memcpy((void *) codes, addressLoaderCodeLine, CODE_LINE_BYTES);
	memcpy((void *) searchTemplate, searchTemplate + CODE_LINE_BYTES, (size_t) (*length));
	(*length) -= occupiedSearchTemplateLength;

	(*bytesIndex) = 0;
}

void parseRegularCode(unsigned char *codes, const enum CodeType *codeType,
					  enum Pointer *pointer, enum ValueSize *valueSize,
					  int *bytesIndex, unsigned char **address,
					  unsigned char **value, int *valueBytes) {
	unsigned char pointerAndValueSize = codes[(*bytesIndex)];
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
		if (*codeType == CODE_TYPE_RAM_WRITE) {
			(*address) -= sizeof(short);
			log_printf("Offset: %p\n", (void *) (long) readRealShort((*address)));
		} else {
			log_printf("Offset: %p\n", (void *) (long) readRealInteger((*address)));
		}
		log_print("Checking loaded pointer...\n");
		if (loadedPointer != ILLEGAL_POINTER) {
			log_print("Applying loaded pointer...\n");
			if (*codeType == CODE_TYPE_RAM_WRITE) {
				unsigned char *addressPointer = (unsigned char *) (readRealShort(*address) + loadedPointer);
				*address = (unsigned char *) &addressPointer;
			} else {
				*address += loadedPointer;
			}
		} else {
			log_print("Invalid pointer, not executing...\n");
		}
		if (realMemoryAccessesAreEnabled) {
			log_printf("Address: %p\n", (void *) (long) readRealInteger((*address)));
		}
	} else {
		log_printf("Address: %p\n", (void *) (long) readRealInteger((*address)));
	}

	switch (*codeType) {
		case CODE_TYPE_LOAD_POINTER:
			(*value) = &codes[(*bytesIndex)];
			log_printf("Range start: %p\n", (void *) (long) readRealInteger(*value));
			break;

		default:
			if ((*codeType == CODE_TYPE_RAM_WRITE) && (*pointer == POINTER_POINTER)) {
				(*value) = &codes[(*bytesIndex - sizeof(int))];
			} else {
				(*value) = &codes[(*bytesIndex)];
			}
			log_printf("Value: %p {Length: %i}\n", (void *) (long) readRealInteger(*value), *valueBytes);
			break;
	}

	switch (*codeType) {
		case CODE_TYPE_STRING_WRITE:;
			int increment = roundUp(*valueBytes, CODE_LINE_BYTES);
			(*bytesIndex) += increment;
			break;

		default:
			if (!(*codeType == CODE_TYPE_RAM_WRITE && *pointer == POINTER_POINTER)) {
				(*bytesIndex) += sizeof(int) * 2;
			}
			break;
	}
}

bool isLoadedPointerValid(const enum Pointer *pointer) {
	return ((*pointer) == POINTER_POINTER && loadedPointer != ILLEGAL_POINTER) || (*pointer) == POINTER_NO_POINTER;
}

#define CODE_LINE_POINTER_MAXIMUM_INCREMENT INT_MAX

int getCodeLinePointerIncrement(unsigned char *haystack, const unsigned char *needle, int maximumDistance) {
	unsigned char *currentLocation = haystack;
	size_t needleSize = sizeof(needle);

	while (currentLocation - haystack < maximumDistance) {
		for (unsigned int needleIndex = 0; needleIndex < needleSize; needleIndex++) {
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

void runCodeHandlerInternal(unsigned char *codes, int codesLength) {
	// Keep running the code handler till all codes are handled
	while (codesLength > 0) {
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
				parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &codesLength,
									 (enum ComparisonType) codeType);
				break;

			case CODE_TYPE_IF_NOT_EQUAL:
				log_printf("### If not equal ###\n");
				parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &codesLength,
									 (enum ComparisonType) codeType);
				break;

			case CODE_TYPE_IF_GREATER:
				log_printf("### If greater than ###\n");
				parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &codesLength,
									 (enum ComparisonType) codeType);
				break;

			case CODE_TYPE_IF_LESS:
				log_printf("### If less than ###\n");
				parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &codesLength,
									 (enum ComparisonType) codeType);
				break;

			case CODE_TYPE_IF_GREATER_THAN_OR_EQUAL:
				log_printf("### If greater than or equal ###\n");
				parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &codesLength,
									 (enum ComparisonType) codeType);
				break;

			case CODE_TYPE_IF_LESS_THAN_OR_EQUAL:
				log_printf("### If less than or equal ###\n");
				parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &codesLength,
									 (enum ComparisonType) codeType);
				break;

			case CODE_TYPE_AND:
				log_printf("### Apply AND ###\n");
				parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &codesLength,
									 (enum ComparisonType) codeType);
				break;

			case CODE_TYPE_OR:
				log_printf("### Apply OR ###\n");
				parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &codesLength,
									 (enum ComparisonType) codeType);
				break;

			case CODE_TYPE_IF_VALUE_BETWEEN:
				log_printf("### If Value Between ###\n");
				parseConditionalCode(codes, &pointer, &valueSize, &bytesIndex, &address, &value, &valueBytes, &codesLength,
									 (enum ComparisonType) codeType);
				break;

			case CODE_TYPE_ADD_TIME_DEPENDENCE:
				log_printf("### Add Time Dependence ###\n");
				bytesIndex += sizeof(int) - sizeof(char);
				value = &codes[bytesIndex];
				timeDependenceDelay = readRealInteger(value);
				log_printf("[ADD_TIME_DEPENDENCE] %i\n", timeDependenceDelay);

				if (timeDependenceDelay != 0 && codeHandlerExecutionsCount % timeDependenceDelay != 0) {
					branchToAfterTerminator(codes, &codesLength, &bytesIndex, resetTimerCodeLine);
				} else {
					log_printf("Not branching: Executions count: %i\n", codeHandlerExecutionsCount);
				}

				bytesIndex += sizeof(int);
				break;

			case CODE_TYPE_RESET_TIMER:
				log_printf("### Reset Timer ###\n");
				bytesIndex += sizeof(resetTimerCodeLine) - 1;
				applyTimerReset();
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
					applyIntegerRegisterOperation(integerDestinationRegisterPointer,
												  integerSecondOperandRegisterPointer,
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

			case CODE_TYPE_LOAD_POINTER_DIRECTLY:
				log_printf("### Loading Pointer Directly ###\n");
				bytesIndex += sizeof(int) - sizeof(char);
				loadedPointer = readRealInteger(&codes[bytesIndex]);
				log_printf("Loaded Pointer: %p\n", (void *) loadedPointer);
				bytesIndex += sizeof(int);
				break;

			case CODE_TYPE_EXECUTE_ASSEMBLY:
				log_printf("### Execute Assembly ###\n");
				bytesIndex++;
				unsigned int assemblyLines = readRealShort(&codes[bytesIndex]);
				unsigned int assemblyBytesCount = assemblyLines * CODE_LINE_BYTES;
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
				applyTerminator();
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
				applySearchTemplateCodeType(codes, &codesLength, &bytesIndex);

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
				bytesIndex = handleProcedureCall(codes, bytesIndex);

				break;

			default:;
#define ERROR_BUFFER_SIZE 100
				char errorMessageBuffer[ERROR_BUFFER_SIZE];
				snprintf(errorMessageBuffer, ERROR_BUFFER_SIZE, "Unhandled code type: %i\n", codeType);
				OSFatal(errorMessageBuffer);
				break;
		}

		// Those have been consumed
		codesLength -= bytesIndex;

		// Is not supposed to happen
		if (codesLength < 0) {
			char errorMessageBuffer[ERROR_BUFFER_SIZE];
			snprintf(errorMessageBuffer, ERROR_BUFFER_SIZE, "Negative length: %i", codesLength);
			OSFatal(errorMessageBuffer);
		}

		if (codesLength % CODE_LINE_BYTES != 0) {
			char errorMessageBuffer[ERROR_BUFFER_SIZE];
			snprintf(errorMessageBuffer, ERROR_BUFFER_SIZE, "Illegal code length: %i", codesLength);
			OSFatal(errorMessageBuffer);
		}

		printf("\n");
		codes = codes + bytesIndex;
	}

	// One full code handler execution is done
	codeHandlerExecutionsCount++;
}

void initialize() {
	applyTerminator();
	applyTimerReset();
	initializeGeckoRegisters();
}

void runCodeHandler(unsigned char *codes, int length) {
	initialize();
	runCodeHandlerInternal(codes, length);
}