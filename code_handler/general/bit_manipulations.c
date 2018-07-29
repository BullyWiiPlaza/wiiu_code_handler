#include "bit_manipulations.h"

char get_upper_nibble(unsigned char value) {
	return (char) ((value & 0xF0u) / 10);
}

char get_lower_nibble(unsigned char value) {
	return (char) (value & 0x0Fu);
}