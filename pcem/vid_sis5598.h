extern device_t sis5598_device;

void sis5598_out(uint16_t addr, uint8_t val, void *p);
uint8_t sis5598_in(uint16_t addr, void *p);
struct svga_t *sis5598_svga(void *p);
