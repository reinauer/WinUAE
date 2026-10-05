/* Bounded, remote-owned guest input. No host window or device dependency. */
#ifndef WINUAE_GDB_INPUT_H
#define WINUAE_GDB_INPUT_H
#include <array>
#include <cstdint>
#include <string>

namespace winuae_gdb {
struct InputEvent {
	uint32_t kind, port, code, value;
	bool valid() const {
		if (kind == 0) return port == 0 && code < 0x68 && value <= 1;
		if (kind == 1) return port < 2 && code < 2 &&
			(value <= 127 || value >= 0xffffff81u);
		return kind == 2 && port < 2 && code < 7 && value <= 1;
	}
};
class InputSink {
public:
	virtual ~InputSink() = default;
	virtual void input(const InputEvent&) = 0;
};
class GuestInput {
	InputSink& sink;
	std::array<bool, 104> keys{};
	std::array<std::array<bool, 7>, 2> ports{};
public:
	explicit GuestInput(InputSink& sink) : sink(sink) {}
	bool event(const InputEvent& e) {
		if (!e.valid()) return false;
		if (e.kind != 1) {
			bool& held = e.kind == 0 ? keys[e.code] : ports[e.port][e.code];
			if (held == (e.value != 0)) return true;
			held = e.value != 0;
		}
		sink.input(e);
		return true;
	}
	void release() {
		for (unsigned i = 0; i < keys.size(); ++i) if (keys[i]) event({0, 0, i, 0});
		for (unsigned p = 0; p < ports.size(); ++p)
			for (unsigned i = 0; i < ports[p].size(); ++i) if (ports[p][i]) event({2, p, i, 0});
	}
	std::string status() const {
		std::string out = "{\"held\":[";
		bool first = true;
		auto add = [&](unsigned kind, unsigned port, unsigned code) {
			if (!first) out += ",";
			first = false;
			out += "{\"kind\":" + std::to_string(kind) + ",\"port\":" + std::to_string(port) +
				",\"code\":" + std::to_string(code) + "}";
		};
		for (unsigned i = 0; i < keys.size(); ++i) if (keys[i]) add(0, 0, i);
		for (unsigned p = 0; p < ports.size(); ++p)
			for (unsigned i = 0; i < ports[p].size(); ++i) if (ports[p][i]) add(2, p, i);
		return out + "]}";
	}
};
}
#endif
