// Isolated callback tests using WinUAE's actual PCI descriptor structures.
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <initializer_list>
struct addrbank;
#define REGPARAM
#define REGPARAM2
#define REGPARAM3
#include "pci_hw.h"
#define xcalloc(t, n) ((t*)calloc(n, sizeof(t)))
#define xfree(p) free(p)
#define write_log(...) ((void)0)
#include "../../sunpci_pci.inc"

int main()
{
	for (int gen = 0; gen < 3; gen++) {
		pci_board_state a = {}, b = {};
		const pci_board &board = sunpci_fixture_boards[gen];
		assert(board.config->vendor == 0x108e);
		assert(board.config->bars[0] == 4096);
		assert(board.config->bars[1] == (256 | 1));
		assert(board.config->device == (gen == 0 ? 0x5043 : gen == 1 ? 0x7063 : 0x676a));
		assert(board.init(&a, nullptr) && board.init(&b, nullptr));
		const pci_addrbank &mem = board.bars[0], &io = board.bars[1];
		for (unsigned reg = 0xa8; reg <= 0xc4; reg += 4) {
			assert(mem.lget(&a, reg) == 0);
			for (unsigned bit = 0; bit < 32; bit++) {
				uint32_t v = uint32_t(1) << bit;
				mem.lput(&a, reg, v);
				assert(io.lget(&a, reg) == v);
				io.lput(&a, reg, ~v);
				assert(mem.lget(&a, reg) == ~v);
			}
			assert(mem.lget(&b, reg) == 0);
		}
		mem.lput(&a, 0x100000c4, 0x12345678);
		assert(mem.bget(&a, 0xc4) == 0x78);
		assert(mem.bget(&a, 0xc7) == 0x12);
		assert(mem.wget(&a, 0xc5) == 0x3456);
		io.wput(&a, 0xc5, 0xabcd);
		assert(mem.lget(&a, 0xc4) == 0x12abcd78);
		mem.bput(&a, 0xc7, 0xef);
		assert(mem.lget(&a, 0xc4) == 0xefabcd78);
		assert(mem.lget(&a, 0xc7) == 0xffffffef);
		mem.lput(&a, 0xfff, 0);
		assert(mem.lget(&a, 0xfff) == 0xffffffff);
		assert(mem.lget(&a, 0xc4) == 0xefabcd78);
		mem.lput(&a, 0x98, 0);
		assert(mem.lget(&a, 0x98) == 0xffffffff);
		for (uint32_t cmd : {0x474f474fu, 0x464c5348u, 0x4d46474du}) {
			mem.lput(&a, 0xa8, cmd);
			assert(mem.lget(&a, 0xa8) == cmd);
		}
		board.reset(&a);
		for (unsigned reg = 0xa8; reg <= 0xc4; reg += 4)
			assert(mem.lget(&a, reg) == 0);
		board.free(&a); board.free(&b);
		assert(!a.userdata && !b.userdata);
	}
	puts("SunPCi fixture: identities, lanes, aliases, walking bits, bounds, isolation and reset passed");
}
