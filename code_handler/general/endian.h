#pragma once

int isBigEndian();

// unsigned long long int getLong(const unsigned char *value);

unsigned int readRealInteger(const unsigned char *value);

unsigned short readRealShort(const unsigned char *value);

unsigned char readRealByte(const unsigned char *value);

unsigned char *getCharacterPointer(unsigned int *value, int valueSize);