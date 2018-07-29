// #include <netinet/in.h>
#include "architecture_specific.h"

unsigned short swap_unsigned_short(unsigned short value) {
	unsigned short first_operand = value << 8u;
	unsigned short second_operand = value >> 8u;
	return first_operand | second_operand;
}

__attribute__((__may_alias__))
void execute_system_call(unsigned short value) {
	/*asm volatile
	(
	"li 0, %0\n\t"
			"sc\n\t"
			"blr"
	:
	:"i"(value)
	:
	);*/

	unsigned char assembly[] = {
			0x38, 0x00, 0x00, 0x00, // li r0, 0xXXXX    # load syscall value->r0
			0x44, 0x00, 0x00, 0x02, // sc               # syscall instruction
			0x4E, 0x80, 0x00, 0x20  // blr               # return control to program
	};

	unsigned short real_short = read_real_short((unsigned char *) &value);
	if (!is_big_endian()) {
		real_short = swap_unsigned_short(real_short);
	}
	memcpy(&assembly[2], &real_short, sizeof(short));

	log_printf("[SYSTEM_CALL] Executing system call value 0x%04x...\n", value);
	execute_assembly(assembly, sizeof(assembly));
}