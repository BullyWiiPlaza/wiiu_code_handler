#include "endian.h"
#include <cstdint>

// Detect whether the system is using big endian byte order
int is_big_endian() {
	union {
		uint32_t integer;
		char character[4];
	} dummy_union = {0x01000000};

	return dummy_union.character[0];
}

unsigned int swap_unsigned_int(unsigned int value) {
	value = ((value << 8u) & 0xFF00FF00u) | ((value >> 8u) & 0xFF00FFu);
	return (value << 16u) | (value >> 16u);
}

unsigned int get_big_endian(unsigned int value) {
	if (!is_big_endian()) {
		return swap_unsigned_int(value);
	}

	return value;
}

/*unsigned long long int swap_int64(unsigned long long int val) {
	val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
	val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
	return (val << 32) | ((val >> 32) & 0xFFFFFFFFULL);
}*/

unsigned int read_real_integer(const unsigned char *value) {
	return get_big_endian(*(unsigned int *) value);
}

/*unsigned long long int getLong(const unsigned char *value) {
	unsigned long longValue = *(unsigned long *) value;

	if (!is_big_endian()) {
		longValue = swap_int64(longValue);
	}

	return longValue;
}*/

unsigned short read_real_short(const unsigned char *value) {
	unsigned int read = get_big_endian(*(unsigned int *) (value - (sizeof(int) - sizeof(short))));
	read &= 0x0000FFFF;

	return (unsigned short) read;
}

unsigned char read_real_byte(const unsigned char *value) {
	unsigned int read = get_big_endian(*(unsigned int *) (value - sizeof(int) - sizeof(char)));
	read &= 0x000000FF;

	return (unsigned char) read;
}

/* Used to get a big endian character pointer to an arbitrary int, short, char */
unsigned char *get_character_pointer(unsigned int *value, int value_size) {
	if (!is_big_endian()) {
		*value = swap_unsigned_int(*value);
	}

	auto character_pointer = (unsigned char *) value;
	character_pointer += sizeof(int) - value_size;

	return character_pointer;
}