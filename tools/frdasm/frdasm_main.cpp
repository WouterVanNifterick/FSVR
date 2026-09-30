#include "emu.h"
#include "frdasm.h"
#include <fstream>
#include <iostream>
#include <cstdlib>
// frdasm ROM BASE START END  -> linear listing to stdout
int main(int argc, char **argv) {
	if (argc < 5) { fprintf(stderr, "usage: frdasm rom base start end\n"); return 1; }
	std::ifstream f(argv[1], std::ios::binary);
	std::vector<u8> rom((std::istreambuf_iterator<char>(f)), {});
	u32 base = strtoul(argv[2], 0, 0), start = strtoul(argv[3], 0, 0), end = strtoul(argv[4], 0, 0);
	util::disasm_interface::data_buffer db{&rom, base};
	fr_disassembler d;
	for (u32 pc = start; pc < end;) {
		std::ostringstream s;
		offs_t r = d.disassemble(s, pc, db, db);
		u32 len = r & util::disasm_interface::LENGTHMASK;
		printf("%08x  ", pc);
		for (u32 i = 0; i < 6; i++) printf(i < len ? "%02x" : "  ", i < len ? db.r8(pc + i) : 0);
		printf("  %s\n", s.str().c_str());
		pc += len ? len : 2;
	}
}
