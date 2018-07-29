#pragma once

int is_big_endian();

// unsigned long long int getLong(const unsigned char *value);

unsigned int readRealInteger(const unsigned char *value);

unsigned short read_real_short(const unsigned char *value);

unsigned char readRealByte(const unsigned char *value);

unsigned char *getCharacterPointer(unsigned int *value, int valueSize);