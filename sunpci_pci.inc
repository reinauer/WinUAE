/* SunPCi diagnostic fixtures, included by pci.cpp.
 * No x86 CPU or firmware execution. BAR sizes are test fixtures.
 */
struct sunpci_fixture {
	uae_u8 scratch[32];
};

static void sunpci_fixture_reset(struct pci_board_state *s)
{
	memset(s->userdata, 0, sizeof(struct sunpci_fixture));
}

static bool sunpci_fixture_init(struct pci_board_state *s, struct autoconfig_info*)
{
	s->userdata = xcalloc(struct sunpci_fixture, 1);
	write_log(_T("SunPCi: diagnostic fixture only; no firmware execution\n"));
	return true;
}

static void sunpci_fixture_free(struct pci_board_state *s)
{
	xfree(s->userdata);
	s->userdata = NULL;
}

static uae_u32 sunpci_fixture_read(struct pci_board_state *s, uaecptr addr, unsigned size)
{
	const struct sunpci_fixture *f = (const struct sunpci_fixture*)s->userdata;
	// Both CSR apertures alias the scratch registers. WinUAE performs the
	// host bridge's endian conversion; callbacks return PCI-native values.
	unsigned off = addr & 0xfff;
	uae_u32 value = 0;
	for (unsigned i = 0; i < size; i++) {
		unsigned p = off + i;
		uae_u32 b = (p >= 0xa8 && p < 0xc8) ? f->scratch[p - 0xa8] : 0xff;
		value |= b << (i * 8);
	}
	return value;
}

static void sunpci_fixture_write(struct pci_board_state *s, uaecptr addr, uae_u32 value, unsigned size)
{
	struct sunpci_fixture *f = (struct sunpci_fixture*)s->userdata;
	unsigned off = addr & 0xfff;
	for (unsigned i = 0; i < size; i++) {
		unsigned p = off + i;
		if (p >= 0xa8 && p < 0xc8)
			f->scratch[p - 0xa8] = (uae_u8)(value >> (i * 8));
	}
	// Deliberately retain GOGO/FLSH/MFGM rather than acknowledging a boot
	// that did not execute. Unsupported accesses do not mutate other state.
}

static uae_u32 REGPARAM2 sunpci_fixture_lget(struct pci_board_state *s, uaecptr a)
{ return sunpci_fixture_read(s, a, 4); }
static uae_u32 REGPARAM2 sunpci_fixture_wget(struct pci_board_state *s, uaecptr a)
{ return sunpci_fixture_read(s, a, 2); }
static uae_u32 REGPARAM2 sunpci_fixture_bget(struct pci_board_state *s, uaecptr a)
{ return sunpci_fixture_read(s, a, 1); }
static void REGPARAM2 sunpci_fixture_lput(struct pci_board_state *s, uaecptr a, uae_u32 v)
{ sunpci_fixture_write(s, a, v, 4); }
static void REGPARAM2 sunpci_fixture_wput(struct pci_board_state *s, uaecptr a, uae_u32 v)
{ sunpci_fixture_write(s, a, v, 2); }
static void REGPARAM2 sunpci_fixture_bput(struct pci_board_state *s, uaecptr a, uae_u32 v)
{ sunpci_fixture_write(s, a, v, 1); }

#define SUNPCI_FIXTURE_BANK { sunpci_fixture_lget, sunpci_fixture_wget, sunpci_fixture_bget, sunpci_fixture_lput, sunpci_fixture_wput, sunpci_fixture_bput }
#define SUNPCI_FIXTURE_CONFIG(id) { 0x108e, id, 0, 0, 0, 0x068000, 0, 0x108e, id, 0, 0, 0, { 4096, 256 | 1, 0, 0, 0, 0, 0 } }
static const struct pci_config sunpci_fixture_configs[] = {
	SUNPCI_FIXTURE_CONFIG(0x5043),
	SUNPCI_FIXTURE_CONFIG(0x7063),
	SUNPCI_FIXTURE_CONFIG(0x676a)
};
#define SUNPCI_FIXTURE_BOARD(name, n) { _T(name), &sunpci_fixture_configs[n], sunpci_fixture_init, sunpci_fixture_free, sunpci_fixture_reset, NULL, NULL, { SUNPCI_FIXTURE_BANK, SUNPCI_FIXTURE_BANK }, false, NULL, NULL, NULL }
static const struct pci_board sunpci_fixture_boards[] = {
	SUNPCI_FIXTURE_BOARD("SunPCi I diagnostic fixture", 0),
	SUNPCI_FIXTURE_BOARD("SunPCi II diagnostic fixture", 1),
	SUNPCI_FIXTURE_BOARD("SunPCi III diagnostic fixture", 2)
};
#undef SUNPCI_FIXTURE_BOARD
#undef SUNPCI_FIXTURE_CONFIG
#undef SUNPCI_FIXTURE_BANK
