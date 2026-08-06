#include "../../Sources/src/Platform/Event.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace
{
constexpr int kAxisRange = 10000;
constexpr int kDeadZone = 8000;

struct Device
{
	int runtime_id;
	std::string name;
	std::map<int, int> controls;
};

struct ControllerOracle
{
	std::map<int, int> host_to_runtime;
	std::map<int, Device> devices;
	int next_runtime_id = 3;
	std::vector<int> released;

	static int AxisValue(int raw, int axis)
	{
		if (axis >= 4) return std::max(0, std::min(32767, raw)) * kAxisRange / 32767;
		const int clamped = std::max(-32768, std::min(32767, raw));
		const int magnitude = std::abs(clamped);
		if (magnitude <= kDeadZone) return 0;
		const int scaled = (magnitude - kDeadZone) * kAxisRange / (32767 - kDeadZone);
		return clamped < 0 ? -std::min(kAxisRange, scaled) : std::min(kAxisRange, scaled);
	}

	void Consume(const NPlatform::PlatformEvent &event)
	{
		switch (event.type)
		{
		case NPlatform::EventType::controllerAdded:
			if (host_to_runtime.find(event.deviceId) == host_to_runtime.end())
			{
				const int runtime = next_runtime_id++;
				Device device{runtime, event.text[0] ? event.text : "Gamepad", {}};
				devices[runtime] = device;
				host_to_runtime[event.deviceId] = runtime;
			}
			break;
		case NPlatform::EventType::controllerRemoved:
		{
			auto host = host_to_runtime.find(event.deviceId);
			if (host == host_to_runtime.end()) break;
			auto device = devices.find(host->second);
			if (device != devices.end())
			{
				for (const auto &control : device->second.controls)
					if (control.second != 0) released.push_back(control.first);
				devices.erase(device);
			}
			host_to_runtime.erase(host);
			break;
		}
		case NPlatform::EventType::controllerButtonDown:
		case NPlatform::EventType::controllerButtonUp:
		{
			auto host = host_to_runtime.find(event.deviceId);
			if (host == host_to_runtime.end()) break;
			auto device = devices.find(host->second);
			if (device == devices.end() || event.control < 0 || event.control >= 16) break;
			device->second.controls[0x100 + event.control] = event.type == NPlatform::EventType::controllerButtonDown ? 0x80 : 0;
			break;
		}
		case NPlatform::EventType::controllerAxis:
		{
			auto host = host_to_runtime.find(event.deviceId);
			if (host == host_to_runtime.end()) break;
			auto device = devices.find(host->second);
			if (device == devices.end() || event.control < 0 || event.control >= 6) break;
			device->second.controls[event.control * 4] = AxisValue(event.value, event.control);
			break;
		}
		default: break;
		}
	}
};

bool Check(bool condition, const char *message)
{
	if (!condition) std::fprintf(stderr, "input controller fixture failed: %s\n", message);
	return condition;
}

NPlatform::PlatformEvent Added(int host, const char *name)
{
	NPlatform::PlatformEvent event = {};
	event.type = NPlatform::EventType::controllerAdded;
	event.deviceId = host;
	std::snprintf(event.text, sizeof(event.text), "%s", name);
	return event;
}
}

int main()
{
	ControllerOracle input;
	input.Consume(Added(11, "TwinPad"));
	input.Consume(Added(22, "TwinPad"));
	if (!Check(input.host_to_runtime[11] == 3 && input.host_to_runtime[22] == 4, "connect order assigns runtime IDs")) return 1;
	if (!Check(input.devices[3].name == input.devices[4].name, "duplicate names do not alter runtime identity")) return 1;

	NPlatform::PlatformEvent axis = {};
	axis.type = NPlatform::EventType::controllerAxis;
	axis.deviceId = 11;
	axis.control = 0;
	axis.value = 4000;
	input.Consume(axis);
	if (!Check(input.devices[3].controls[0] == 0, "stick dead zone is deterministic")) return 1;
	axis.value = -32768;
	input.Consume(axis);
	if (!Check(input.devices[3].controls[0] == -kAxisRange, "signed stick axis maps to legacy range")) return 1;
	axis.control = 4;
	axis.value = 32767;
	input.Consume(axis);
	if (!Check(input.devices[3].controls[16] == kAxisRange, "trigger maps to unsigned legacy range")) return 1;

	NPlatform::PlatformEvent button = {};
	button.type = NPlatform::EventType::controllerButtonDown;
	button.deviceId = 11;
	button.control = 0;
	input.Consume(button);
	if (!Check(input.devices[3].controls[0x100] == 0x80, "button down preserves activation value")) return 1;
	button.type = NPlatform::EventType::controllerRemoved;
	input.Consume(button);
	if (!Check(input.devices.find(3) == input.devices.end() && !input.released.empty(), "removal releases active controls")) return 1;

	axis.type = NPlatform::EventType::controllerAxis;
	axis.deviceId = 11;
	axis.control = 0;
	axis.value = 32767;
	input.Consume(axis);
	if (!Check(input.devices.find(3) == input.devices.end(), "stale events after removal are ignored")) return 1;
	input.Consume(Added(11, "TwinPad"));
	if (!Check(input.host_to_runtime[11] == 5, "reconnect receives a fresh runtime generation")) return 1;

	std::puts("input controller mapping fixture passed");
	return 0;
}
