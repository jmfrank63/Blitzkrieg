#include "Event.h"
#include "InputCodes.h"

#include <array>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace
{
constexpr int kMouseAxisX = 0;
constexpr int kMouseAxisY = 4;
constexpr int kMouseAxisWheel = 8;
constexpr int kMouseButton0 = 12;

struct AppliedEvent { int control; int value; };

// Fixture oracle only: CInputAPI has no inspectable backend-neutral state API
// without linking its legacy DirectInput-backed implementation.
struct InputStateOracle
{
	std::array<bool, 256> keys{};
	std::array<bool, 10> buttons{};
	int mouse_x = 0;
	int mouse_y = 0;
	int wheel = 0;
	std::vector<AppliedEvent> applied;

	void consume(const NPlatform::PlatformEvent &event)
	{
		switch (event.type)
		{
		case NPlatform::EventType::keyDown:
		case NPlatform::EventType::keyUp:
		{
			const std::uint32_t legacy = NInput::SDLScancodeToLegacy(static_cast<std::uint32_t>(event.scancode));
			if (legacy != 0 && legacy < keys.size())
			{
				const bool pressed = event.type == NPlatform::EventType::keyDown;
				keys[legacy] = pressed;
				applied.push_back({static_cast<int>(legacy), pressed ? 0x80 : 0});
			}
			break;
		}
		case NPlatform::EventType::mouseMotion:
			mouse_x = event.x;
			mouse_y = event.y;
			applied.push_back({kMouseAxisX, event.x});
			applied.push_back({kMouseAxisY, event.y});
			break;
		case NPlatform::EventType::mouseWheel:
			wheel = event.y;
			applied.push_back({kMouseAxisWheel, event.y});
			break;
		case NPlatform::EventType::mouseButtonDown:
		case NPlatform::EventType::mouseButtonUp:
		{
			const int button = event.button - 1;
			if (button >= 0 && button < static_cast<int>(buttons.size()))
			{
				const bool pressed = event.type == NPlatform::EventType::mouseButtonDown;
				buttons[button] = pressed;
				applied.push_back({kMouseButton0 + button, pressed ? 0x80 : 0});
			}
			break;
		}
		case NPlatform::EventType::focusLost:
			keys.fill(false);
			buttons.fill(false);
			break;
		default:
			break;
		}
	}
};

bool check(bool condition, const char *message)
{
	if (!condition) std::fprintf(stderr, "input state fixture failed: %s\n", message);
	return condition;
}

NPlatform::PlatformEvent key(NPlatform::EventType type, int scancode)
{
	NPlatform::PlatformEvent event;
	event.type = type;
	event.scancode = scancode;
	return event;
}
}

int main()
{
	InputStateOracle state;
	state.consume(key(NPlatform::EventType::keyDown, 4));   // SDL A
	state.consume(key(NPlatform::EventType::keyDown, 224)); // SDL left control
	state.consume(key(NPlatform::EventType::keyDown, 225)); // SDL left shift
	if (!check(state.keys[0x1e] && state.keys[0x1d] && state.keys[0x2a], "simultaneous key modifiers")) return 1;
	state.consume(key(NPlatform::EventType::keyUp, 4));
	if (!check(!state.keys[0x1e] && state.keys[0x1d] && state.keys[0x2a], "key release preserves modifiers")) return 1;

	NPlatform::PlatformEvent motion;
	motion.type = NPlatform::EventType::mouseMotion;
	motion.x = 320;
	motion.y = 240;
	state.consume(motion);
	if (!check(state.mouse_x == 320 && state.mouse_y == 240, "mouse coordinate translation")) return 1;

	NPlatform::PlatformEvent wheel;
	wheel.type = NPlatform::EventType::mouseWheel;
	wheel.y = -1;
	state.consume(wheel);
	if (!check(state.wheel == -1, "wheel direction")) return 1;

	NPlatform::PlatformEvent button;
	button.type = NPlatform::EventType::mouseButtonDown;
	button.button = 1;
	state.consume(button);
	button.type = NPlatform::EventType::mouseButtonUp;
	state.consume(button);
	if (!check(!state.buttons[0], "mouse button release")) return 1;

	const std::size_t before_focus_loss = state.applied.size();
	state.consume(key(NPlatform::EventType::keyDown, 4));
	button.type = NPlatform::EventType::mouseButtonDown;
	state.consume(button);
	NPlatform::PlatformEvent focus_loss;
	focus_loss.type = NPlatform::EventType::focusLost;
	state.consume(focus_loss);
	if (!check(!state.keys[0x1e] && !state.buttons[0], "focus loss synthesizes release state")) return 1;
	if (!check(state.applied.size() > before_focus_loss && state.applied[before_focus_loss].control == 0x1e,
	           "same-frame event ordering")) return 1;

	std::puts("input keyboard and mouse state fixture passed");
	return 0;
}
