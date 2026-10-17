#ifndef UAE_SUNPCI_H
#define UAE_SUNPCI_H

#include "uae/types.h"

extern const struct pci_board sunpci1_pci_board;

bool sunpci1_start(void);
bool sunpci_x86_active(void);
uint32_t sunpci_x86_io(uint16_t port, int dir, uint32_t val, int size);

#endif /* UAE_SUNPCI_H */
