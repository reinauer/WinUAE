/*
* UAE - The Un*x Amiga Emulator
*
* SiS 5597/5598 integrated VGA, driven by the SunPCi I card (sunpci.cpp)
*/

#ifndef WINUAE_VID_SIS5598_H
#define WINUAE_VID_SIS5598_H

#include <stdint.h>

struct svga_t;

void sis5598_out(uint16_t addr, uint8_t val, void *p);
uint8_t sis5598_in(uint16_t addr, void *p);
struct svga_t *sis5598_svga(void *p);

#endif /* WINUAE_VID_SIS5598_H */
