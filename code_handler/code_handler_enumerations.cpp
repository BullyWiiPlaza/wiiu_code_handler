#include "code_handler_enumerations.h"

int get_bytes(enum ValueSize value_size) {
	switch (value_size) {
		case VALUE_SIZE_EIGHT_BIT:
			return sizeof(char);

		case VALUE_SIZE_SIXTEEN_BIT:
			return sizeof(short);

		case VALUE_SIZE_THIRTY_TWO_BIT:
			return sizeof(int);
	}

	OSFatal("Unmatched value size");

	return -1;
}