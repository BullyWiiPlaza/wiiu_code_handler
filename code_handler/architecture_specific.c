#include <asm/byteorder.h>
#include <netinet/in.h>
#include "architecture_specific.h"

__attribute__((__may_alias__))
void executeSystemCall(unsigned short value) {
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

	unsigned short realShort = readRealShort((unsigned char *) &value);
	if (!isBigEndian()) {
		realShort = htons(realShort);
	}
	memcpy(&assembly[2], &realShort, sizeof(short));

	log_printf("[SYSTEM_CALL] Executing system call value 0x%04x...\n", value);
	executeAssembly(assembly, sizeof(assembly));
}