/* Optional WinUAE remote debugger. Guest output capture also accepts trap threads. */
#ifndef WINUAE_GDB_SERVER_H
#define WINUAE_GDB_SERVER_H

#define DEBUG_FEATURE_GDBSERVER (1 << 2)

#ifdef DEBUGGER
namespace winuae_gdb { class Target; }
winuae_gdb::Target& debug_gdb_target();
void gdb_server_poll();
void gdb_server_input_frame();
void gdb_server_guest_output(const char*);
void gdb_server_close();
void gdb_server_reset();
void gdb_server_begin_restore();
void gdb_server_restore_complete(bool);
bool gdb_server_connected();
bool gdb_server_halted();
bool gdb_server_stop(const char* reason);
#else
static inline void gdb_server_poll() {}
static inline void gdb_server_input_frame() {}
static inline void gdb_server_guest_output(const char*) {}
static inline void gdb_server_close() {}
static inline void gdb_server_reset() {}
static inline void gdb_server_begin_restore() {}
static inline void gdb_server_restore_complete(bool) {}
static inline bool gdb_server_connected() { return false; }
static inline bool gdb_server_halted() { return false; }
static inline bool gdb_server_stop(const char*) { return false; }
#endif
#endif
