#pragma once

int is_big_endian();

// unsigned long long int getLong(const unsigned char *value);

unsigned int read_real_integer(const unsigned char *value);

unsigned short read_real_short(const unsigned char *value);

unsigned char read_real_byte(const unsigned char *value);

unsigned char *get_character_pointer(unsigned int *value, int value_size);