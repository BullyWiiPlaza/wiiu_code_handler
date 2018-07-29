#include "bit_manipulations.h"

char getUpperNibble(unsigned char value) {
	return (char) ((value & 0xF0u) / 10);
}

char getLowerNibble(unsigned char value) {
	return (char) (value & 0x0Fu);
}