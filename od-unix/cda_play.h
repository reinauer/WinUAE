#ifndef WINUAE_OD_UNIX_CDA_PLAY_H
#define WINUAE_OD_UNIX_CDA_PLAY_H

#include "sysconfig.h"
#include "sysdeps.h"
#include "audio.h"
#include "blkdev.h"

#define CDDA_BUFFERS 14

extern volatile bool cd_audio_mode_changed;

class cda_audio
{
public:
    uae_u8 *buffers[2];

    cda_audio(int num_sectors, int sectorsize, int)
    {
        int size = num_sectors * sectorsize;
        buffers[0] = (uae_u8*)calloc(1, size);
        buffers[1] = (uae_u8*)calloc(1, size);
    }

    ~cda_audio()
    {
        free(buffers[0]);
        free(buffers[1]);
    }

    void setvolume(int, int) {}
};

struct cda_play;
typedef int (*cda_play_read_block)(struct cda_play *, int, uae_u8 *, int, int, int);

struct cda_play
{
    int unitnum;
    int cdda_volume[2];
    int cd_last_pos;
    int cdda_play;
    int cdda_play_finished;
    int cdda_scan;
    int cdda_paused;
    int cdda_start, cdda_end;
    play_subchannel_callback cdda_subfunc;
    play_status_callback cdda_statusfunc;
    int cdda_play_state;
    int cdda_delay, cdda_delay_frames;
    cda_audio *cda;
    volatile int cda_bufon[2];
    struct cd_audio_state cas;
    bool subcodevalid;
    uae_sem_t sub_sem, sub_sem2;
    uae_u8 subcode[SUB_CHANNEL_SIZE * CDDA_BUFFERS];
    uae_u8 subcodebuf[SUB_CHANNEL_SIZE];
    struct device_info *di;
    cda_play_read_block read_block;
};

static inline void ciw_cdda_play(void*) {}
static inline void ciw_cdda_stop(struct cda_play*) {}
static inline int ciw_cdda_setstate(struct cda_play*, int, int) { return 0; }

#endif /* WINUAE_OD_UNIX_CDA_PLAY_H */
