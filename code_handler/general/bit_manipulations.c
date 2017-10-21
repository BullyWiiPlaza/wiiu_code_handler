#include "bit_manipulations.h"

char getUpperNibble(char value) {
	return (char) ((value & 0xF0) / 10);
}

char getLowerNibble(char value) {
	return (char) (value & 0x0F);
}