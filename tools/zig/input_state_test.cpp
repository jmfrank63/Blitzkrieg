#include "InputCodes.h"
#include "Event.h"

#include <array>
#include <cstdint>
#include <cstdio>

namespace
{
struct LegacyInputState
{
	std::array<bool, 256> keyboard{};
	std::array<bool, 10> mouse_buttons{};
	int mouse_x = 0;
	int mouse_y = 0;
	int wheel = 0;
	std::array<int, 32> order{};
	std::size_t order_count = 0;

	void record(int control)
	{
		if (order_count < order.size()) order[order_count++] = control;
	}

	void consume(const NPlatform::PlatformEvent &event)
	{
		switch (event.type)
		{
			case NPlatform::EventType::keyDown:
			case NPlatform::EventType::keyUp:
			{
				const std::uint32_t legacy = NInput::SDLScancodeToLegacy(static_cast<std::uint32_t>(event.scancode));
				if (legacy != 0 && legacy < keyboard.size())
				{
					keyboard[legacy] = event.type == NPlatform::EventType::keyDown;
					record(static_cast<int>(legacy));
				}
				break;
			}
			case NPlatform::EventType::mouseMotion:
				mouse_x += event.x;
				mouse_y += event.y;
				record(0);
				record(4);
				break;
			case NPlatform::EventType::mouseButtonDown:
			case NPlatform::EventType::mouseButtonUp:
				if (event.button >= 1 && event.button <= static_cast<int>(mouse_buttons.size()))
				{
					mouse_buttons[static_cast<std::size_t>(event.button - 1)] =
						event.type == NPlatform::EventType::mouseButtonDown;
					record(12 + event.button - 1);
				}
				break;
			case NPlatform::EventType::mouseWheel:
				wheel += event.y;
				record(8);
				break;
			case NPlatform::EventType::focusLost:
				keyboard.fill(false);
				mouse_buttons.fill(false);
				break;
			default:
				break;
		}
	}
};

bool check(bool condition, const char *message)
{
	if (!condition) std::fprintf(stderr, "input state test failed: %s\n", message);
	return condition;
}
}

int main()
{
	LegacyInputState state;

	NPlatform::PlatformEvent event{};
	event.type = NPlatform::EventType::keyDown;
	event.scancode = 4; // SDL A -> legacy DIK A (0x1e).
	state.consume(event);
	event.type = NPlatform::EventType::keyDown;
	event.scancode = 225; // SDL left shift -> legacy DIK left shift (0x2a).
	state.consume(event);
	if (!check(state.keyboard[0x1e] && state.keyboard[0x2a], "key press and simultaneous modifier")) return 1;

	event.type = NPlatform::EventType::keyUp;
	event.scancode = 4;
	state.consume(event);
	if (!check(!state.keyboard[0x1e] && state.keyboard[0x2a], "key release preserves modifier")) return 1;

	event.type = NPlatform::EventType::mouseMotion;
	event.x = 7;
	event.y = -3;
	state.consume(event);
	if (!check(state.mouse_x == 7 && state.mouse_y == -3, "relative pointer motion")) return 1;

	event.type = NPlatform::EventType::mouseButtonDown;
	event.button = 1;
	state.consume(event);
	event.type = NPlatform::EventType::mouseButtonUp;
	state.consume(event);
	if (!check(!state.mouse_buttons[0], "mouse button press and release")) return 1;

	event.type = NPlatform::EventType::mouseWheel;
	event.y = 120;
	state.consume(event);
	if (!check(state.wheel == 120, "wheel direction and value")) return 1;
	if (!check(state.order_count == 8 && state.order[0] == 0x1e && state.order[1] == 0x2a &&
			state.order[2] == 0x1e && state.order[3] == 0 && state.order[4] == 4 &&
			state.order[5] == 12 && state.order[6] == 12 && state.order[7] == 8,
			"same-frame event ordering")) return 1;

	event.type = NPlatform::EventType::focusLost;
	state.consume(event);
	if (!check(!state.keyboard[0x2a] && !state.mouse_buttons[0], "focus loss synthesizes releases")) return 1;

	std::puts("input state event contract passed");
	return 0;
}
