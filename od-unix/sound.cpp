#include "sysconfig.h"
#include "sysdeps.h"

#include "sounddep/sound.h"
#include "gensound.h"

int active_sound_stereo;
uae_u16 paula_sndbuffer[DEFAULT_SOUND_MAXB];
uae_u16 *paula_sndbufpt = paula_sndbuffer;
int paula_sndbufsize = DEFAULT_SOUND_MAXB;

int setup_sound(void)
{
    sound_available = 0;
    return 0;
}

int init_sound(void) { return 0; }
void close_sound(void) {}
void finish_sound_buffer(void) { paula_sndbufpt = paula_sndbuffer; }
void restart_sound_buffer(void) { paula_sndbufpt = paula_sndbuffer; }
void pause_sound_buffer(void) {}
void resume_sound(void) {}
void pause_sound(void) {}
void reset_sound(void) {}
bool sound_paused(void) { return true; }
void sound_setadjust(float) {}
int enumerate_sound_devices(void) { return 0; }
void sound_mute(int) {}
void sound_volume(int) {}
void set_volume(int, int) {}
void master_sound_volume(int) {}
