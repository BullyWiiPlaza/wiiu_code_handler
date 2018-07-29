#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "code_handler_enumerations.h"

extern bool real_memory_accesses_are_enabled;

unsigned long call_address(uintptr_t real_address, const unsigned int *arguments);

uintptr_t search_for_template(const unsigned char *search_template, unsigned int length,
							  uintptr_t address, uintptr_t end_address,
							  int stepSize, unsigned short target_match_index,
							  bool *template_found);

void skip_write_memory(unsigned char *address, unsigned char *value,
					   int value_length, unsigned char *step_size,
					   unsigned char *increment, unsigned char *iterations_count);

void write_string(unsigned char *address, unsigned char *value, int value_length);

void write_value(unsigned char *address, const unsigned char *value, int value_length);

bool compare_value(unsigned char *address, unsigned char *value, const enum ValueSize *value_size,
				   unsigned char *upper_value, int value_length, enum ComparisonType comparison_type);

void execute_assembly(unsigned char *instructions, unsigned int size);

void apply_corrupter(unsigned char *beginning, unsigned char *end, unsigned char *search_value,
					 unsigned char *replacement_value);

unsigned int read_real_value(enum ValueSize value_size, unsigned char *pointer_to_address);

uintptr_t loadPointer(unsigned char *address, unsigned char *value);