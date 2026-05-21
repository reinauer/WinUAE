#include "sysconfig.h"
#include "sysdeps.h"

#ifdef WITH_MIDI

#include <vector>

#if defined(WINUAE_UNIX_WITH_COREMIDI)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#elif defined(WINUAE_UNIX_WITH_ALSA_MIDI)
#include <alsa/asoundlib.h>
#endif

#include "options.h"
#include "midi.h"

extern int serdev;

BOOL midi_ready = FALSE;

struct unix_midi_output_device {
    int devid;
    TCHAR name[256];
#if defined(WINUAE_UNIX_WITH_COREMIDI)
    MIDIEndpointRef endpoint;
#elif defined(WINUAE_UNIX_WITH_ALSA_MIDI)
    int client;
    int port;
#endif
};

static std::vector<unix_midi_output_device> midi_outputs;
static bool midi_outputs_enumerated;
static MidiOutStatus out_status;
static std::vector<uae_u8> sysex_buffer;

#if defined(WINUAE_UNIX_WITH_COREMIDI)
static MIDIClientRef midi_client;
static MIDIPortRef midi_out_port;
static MIDIEndpointRef midi_out_endpoint;
#elif defined(WINUAE_UNIX_WITH_ALSA_MIDI)
static snd_seq_t *alsa_seq;
static int alsa_out_port = -1;
static snd_midi_event_t *alsa_encoder;
#endif

static const uae_u8 plen[128] = {
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,2,1,0,0,0,0,0,0,0,0,0,0,0,0
};

static void midi_reset_parser(void)
{
    memset(&out_status, 0, sizeof out_status);
    sysex_buffer.clear();
}

#if defined(WINUAE_UNIX_WITH_COREMIDI)
static bool coremidi_object_string(MIDIObjectRef object, CFStringRef property, TCHAR *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return false;
    }
    out[0] = 0;
    CFStringRef str = NULL;
    if (MIDIObjectGetStringProperty(object, property, &str) != noErr || !str) {
        return false;
    }
    bool ok = CFStringGetCString(str, out, out_size, kCFStringEncodingUTF8);
    CFRelease(str);
    return ok && out[0];
}
#endif

static void enumerate_midi_outputs(void)
{
    midi_outputs.clear();
    midi_outputs_enumerated = true;
#if defined(WINUAE_UNIX_WITH_COREMIDI)
    const ItemCount count = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < count; i++) {
        MIDIEndpointRef endpoint = MIDIGetDestination(i);
        if (!endpoint) {
            continue;
        }
        unix_midi_output_device dev;
        memset(&dev, 0, sizeof dev);
        dev.devid = (int)i;
        dev.endpoint = endpoint;
        if (!coremidi_object_string(endpoint, kMIDIPropertyDisplayName, dev.name, sizeof dev.name / sizeof(TCHAR))
            && !coremidi_object_string(endpoint, kMIDIPropertyName, dev.name, sizeof dev.name / sizeof(TCHAR))) {
            _sntprintf(dev.name, sizeof dev.name / sizeof(TCHAR), _T("CoreMIDI destination %d"), (int)i + 1);
        }
        midi_outputs.push_back(dev);
    }
#elif defined(WINUAE_UNIX_WITH_ALSA_MIDI)
    snd_seq_t *seq = NULL;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        return;
    }
    snd_seq_client_info_t *client_info;
    snd_seq_port_info_t *port_info;
    snd_seq_client_info_alloca(&client_info);
    snd_seq_port_info_alloca(&port_info);
    snd_seq_client_info_set_client(client_info, -1);
    int devid = 0;
    while (snd_seq_query_next_client(seq, client_info) >= 0) {
        int client = snd_seq_client_info_get_client(client_info);
        snd_seq_port_info_set_client(port_info, client);
        snd_seq_port_info_set_port(port_info, -1);
        while (snd_seq_query_next_port(seq, port_info) >= 0) {
            unsigned int caps = snd_seq_port_info_get_capability(port_info);
            if ((caps & SND_SEQ_PORT_CAP_WRITE) == 0 || (caps & SND_SEQ_PORT_CAP_SUBS_WRITE) == 0) {
                continue;
            }
            unix_midi_output_device dev;
            memset(&dev, 0, sizeof dev);
            dev.devid = devid++;
            dev.client = client;
            dev.port = snd_seq_port_info_get_port(port_info);
            const char *client_name = snd_seq_client_info_get_name(client_info);
            const char *port_name = snd_seq_port_info_get_name(port_info);
            _sntprintf(dev.name, sizeof dev.name / sizeof(TCHAR), _T("%d:%d %s%s%s"),
                dev.client, dev.port,
                client_name ? client_name : "ALSA",
                port_name && port_name[0] ? " " : "",
                port_name ? port_name : "");
            midi_outputs.push_back(dev);
        }
    }
    snd_seq_close(seq);
#endif
}

static void ensure_midi_outputs(void)
{
    if (!midi_outputs_enumerated) {
        enumerate_midi_outputs();
    }
}

static unix_midi_output_device *find_midi_output(int devid)
{
    ensure_midi_outputs();
    if (midi_outputs.empty()) {
        return NULL;
    }
    if (devid == -1) {
        return &midi_outputs[0];
    }
    for (size_t i = 0; i < midi_outputs.size(); i++) {
        if (midi_outputs[i].devid == devid) {
            return &midi_outputs[i];
        }
    }
    return NULL;
}

int unix_midi_output_device_count(void)
{
    ensure_midi_outputs();
    return midi_outputs.empty() ? 0 : (int)midi_outputs.size() + 1;
}

int unix_midi_output_device_id(int index)
{
    ensure_midi_outputs();
    if (index == 0 && !midi_outputs.empty()) {
        return -1;
    }
    index--;
    if (index < 0 || index >= (int)midi_outputs.size()) {
        return -2;
    }
    return midi_outputs[index].devid;
}

const TCHAR *unix_midi_output_device_display_name(int index)
{
    static TCHAR name[320];
    ensure_midi_outputs();
    if (index == 0 && !midi_outputs.empty()) {
        return _T("Default MIDI-Out Device");
    }
    index--;
    if (index < 0 || index >= (int)midi_outputs.size()) {
        return _T("");
    }
    _sntprintf(name, sizeof name / sizeof(TCHAR), _T("%s"), midi_outputs[index].name);
    return name;
}

const TCHAR *unix_midi_output_device_config_name_for_id(int devid)
{
    if (devid < -1) {
        return _T("none");
    }
    if (devid == -1) {
        return _T("default");
    }
    unix_midi_output_device *dev = find_midi_output(devid);
    return dev ? dev->name : _T("default");
}

int unix_midi_output_device_id_from_config_name(const TCHAR *name)
{
    if (!name || !name[0] || !_tcsicmp(name, _T("none"))) {
        return -2;
    }
    if (!_tcsicmp(name, _T("default"))) {
        return -1;
    }
    ensure_midi_outputs();
    for (size_t i = 0; i < midi_outputs.size(); i++) {
        if (!_tcsicmp(name, midi_outputs[i].name)) {
            return midi_outputs[i].devid;
        }
    }
    return -2;
}

static bool send_midi_bytes(const uae_u8 *data, int len)
{
    if (!data || len <= 0 || !midi_ready) {
        return false;
    }
#if defined(WINUAE_UNIX_WITH_COREMIDI)
    std::vector<Byte> packet_storage(sizeof(MIDIPacketList) + len + 128);
    MIDIPacketList *packet_list = (MIDIPacketList*)packet_storage.data();
    MIDIPacket *packet = MIDIPacketListInit(packet_list);
    packet = MIDIPacketListAdd(packet_list, packet_storage.size(), packet, 0, len, data);
    if (!packet) {
        return false;
    }
    return MIDISend(midi_out_port, midi_out_endpoint, packet_list) == noErr;
#elif defined(WINUAE_UNIX_WITH_ALSA_MIDI)
    bool sent = false;
    for (int i = 0; i < len; i++) {
        snd_seq_event_t ev;
        snd_seq_ev_clear(&ev);
        int ret = snd_midi_event_encode_byte(alsa_encoder, data[i], &ev);
        if (ret > 0) {
            snd_seq_ev_set_source(&ev, alsa_out_port);
            snd_seq_ev_set_subs(&ev);
            snd_seq_ev_set_direct(&ev);
            if (snd_seq_event_output_direct(alsa_seq, &ev) >= 0) {
                sent = true;
            }
        }
    }
    return sent;
#else
    return false;
#endif
}

int Midi_Parse(midi_direction_e direction, BYTE *dataptr)
{
    if (direction != midi_output || !dataptr) {
        return 0;
    }
    const uae_u8 data = (uae_u8)*dataptr;
    if (data >= 0x80) {
        if (out_status.sysex) {
            sysex_buffer.push_back(MIDI_EOX);
            send_midi_bytes(sysex_buffer.data(), (int)sysex_buffer.size());
            sysex_buffer.clear();
            out_status.sysex = 0;
            out_status.unknown = 1;
            if (data == MIDI_EOX) {
                return 0;
            }
        }
        out_status.status = data;
        out_status.length = plen[data & 0x7f];
        out_status.posn = 0;
        out_status.unknown = 0;
        if (data == MIDI_SYSX) {
            out_status.sysex = 1;
            sysex_buffer.clear();
            sysex_buffer.push_back(data);
            return 0;
        }
        if (out_status.length == 0) {
            send_midi_bytes(&data, 1);
        }
        return 0;
    }
    if (out_status.sysex) {
        if (sysex_buffer.size() < BUFFLEN) {
            sysex_buffer.push_back(data);
        }
        return 0;
    }
    if (out_status.unknown) {
        return 0;
    }
    if (++out_status.posn == 1) {
        out_status.byte1 = data;
    } else {
        out_status.byte2 = data;
    }
    if (out_status.posn >= out_status.length) {
        uae_u8 msg[3] = {
            (uae_u8)out_status.status,
            (uae_u8)out_status.byte1,
            (uae_u8)out_status.byte2
        };
        const int len = 1 + out_status.length;
        out_status.posn = 0;
        send_midi_bytes(msg, len);
    }
    return 0;
}

int Midi_Open(void)
{
    if (midi_ready) {
        return 1;
    }
    if (currprefs.win32_midioutdev < -1) {
        return 0;
    }
    unix_midi_output_device *dev = find_midi_output(currprefs.win32_midioutdev);
    if (!dev) {
        write_log(_T("MIDI OUT: no output device for id %d\n"), currprefs.win32_midioutdev);
        return 0;
    }
#if defined(WINUAE_UNIX_WITH_COREMIDI)
    if (MIDIClientCreate(CFSTR("WinUAE Unix MIDI"), NULL, NULL, &midi_client) != noErr) {
        write_log(_T("MIDI OUT: MIDIClientCreate failed\n"));
        return 0;
    }
    if (MIDIOutputPortCreate(midi_client, CFSTR("WinUAE Unix MIDI Out"), &midi_out_port) != noErr) {
        MIDIClientDispose(midi_client);
        midi_client = 0;
        write_log(_T("MIDI OUT: MIDIOutputPortCreate failed\n"));
        return 0;
    }
    midi_out_endpoint = dev->endpoint;
#elif defined(WINUAE_UNIX_WITH_ALSA_MIDI)
    if (snd_seq_open(&alsa_seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        write_log(_T("MIDI OUT: failed to open ALSA sequencer\n"));
        return 0;
    }
    snd_seq_set_client_name(alsa_seq, "WinUAE Unix MIDI");
    alsa_out_port = snd_seq_create_simple_port(alsa_seq, "WinUAE MIDI Out",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (alsa_out_port < 0 || snd_seq_connect_to(alsa_seq, alsa_out_port, dev->client, dev->port) < 0) {
        snd_seq_close(alsa_seq);
        alsa_seq = NULL;
        alsa_out_port = -1;
        write_log(_T("MIDI OUT: failed to connect ALSA port %s\n"), dev->name);
        return 0;
    }
    if (snd_midi_event_new(BUFFLEN, &alsa_encoder) < 0) {
        snd_seq_close(alsa_seq);
        alsa_seq = NULL;
        alsa_out_port = -1;
        write_log(_T("MIDI OUT: failed to create ALSA MIDI encoder\n"));
        return 0;
    }
    snd_midi_event_init(alsa_encoder);
#endif
    midi_reset_parser();
    midi_ready = TRUE;
    serdev = 1;
    write_log(_T("MIDI OUT: using %s\n"), dev->name);
    return 1;
}

void Midi_Close(void)
{
    if (!midi_ready) {
        return;
    }
#if defined(WINUAE_UNIX_WITH_COREMIDI)
    if (midi_out_port) {
        MIDIPortDispose(midi_out_port);
        midi_out_port = 0;
    }
    if (midi_client) {
        MIDIClientDispose(midi_client);
        midi_client = 0;
    }
    midi_out_endpoint = 0;
#elif defined(WINUAE_UNIX_WITH_ALSA_MIDI)
    if (alsa_encoder) {
        snd_midi_event_free(alsa_encoder);
        alsa_encoder = NULL;
    }
    if (alsa_seq) {
        snd_seq_close(alsa_seq);
        alsa_seq = NULL;
    }
    alsa_out_port = -1;
#endif
    midi_ready = FALSE;
    midi_reset_parser();
    write_log(_T("MIDI: closed.\n"));
}

void Midi_Reopen(void)
{
    if (midi_ready) {
        Midi_Close();
        Midi_Open();
    }
}

int ismidibyte(void)
{
    return 0;
}

LONG getmidibyte(void)
{
    return -1;
}

#endif
