#include "rounding.h"

int roundUp(int value, int multiple) {
	if (value % multiple == 0) {
		value++;
	}

	if (multiple == 0) {
		return value;
	}

	int remainder = value % multiple;
	if (remainder == 0) {
		return value;
	}

	return value + multiple - remainder;
}