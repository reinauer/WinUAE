 /*
  * UAE - The Un*x Amiga Emulator
  *
  * SANAII compatible network driver emulation
  *
  * (c) 2007 Toni Wilen
  */

#ifndef UAE_SANA2_H
#define UAE_SANA2_H

#include "uae/types.h"

#ifndef MAX_TOTAL_NET_DEVICES
#define MAX_TOTAL_NET_DEVICES 30
#endif

uaecptr netdev_startup(TrapContext*, uaecptr resaddr);
void netdev_install(void);

extern int log_net;

#endif /* UAE_SANA2_H */
