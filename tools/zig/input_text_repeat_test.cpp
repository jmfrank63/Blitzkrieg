#include "../../Sources/src/Input/InputCodes.h"
#include "../../Sources/src/Platform/Event.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
constexpr std::uint64_t kRepeatDelayMs = 500;
constexpr std::uint64_t kRepeatPeriodMs = 30;
constexpr std::size_t kTextCapacity = 16;

struct KeyObservation
{
	std::uint32_t code;
	std::uint64_t timestamp;
	bool repeat;
};

struct TextObservation
{
	std::uint16_t code_unit;
	std::uint64_t timestamp;
};

// Fixture oracle only: the legacy input object keeps these queues private.
// This models the normalized event-boundary semantics without a platform API.
struct InputTextRepeatOracle
{
	std::array<bool, 256> pressed{};
	std::vector<KeyObservation> key_events;
	std::vector<TextObservation> text_events;

	bool text_enabled = true;
	std::uint32_t repeating_key = 0;
	std::uint64_t next_repeat = 0;

	void Consume(const NPlatform::PlatformEvent &event)
	{
		Advance(event.timestamp);
		switch (event.type)
		{
		case NPlatform::EventType::keyDown:
		{
			const std::uint32_t code = NInput::SDLScancodeToLegacy(static_cast<std::uint32_t>(event.scancode));
			if (code == 0 || code >= pressed.size()) break;
			pressed[code] = true;
			key_events.push_back({code, event.timestamp, false});
			repeating_key = code;
			next_repeat = event.timestamp + kRepeatDelayMs;
			break;
		}
		case NPlatform::EventType::keyUp:
		{
			const std::uint32_t code = NInput::SDLScancodeToLegacy(static_cast<std::uint32_t>(event.scancode));
			if (code == 0 || code >= pressed.size()) break;
			pressed[code] = false;
			if (repeating_key == code) repeating_key = 0;
			key_events.push_back({code, event.timestamp, false});
			break;
		}
		case NPlatform::EventType::textInput:
			if (text_enabled)
			{
				std::uint16_t decoded[kTextCapacity] = {};
				const std::size_t count = NInput::DecodeUtf8(event.text, decoded, kTextCapacity);
				for (std::size_t i = 0; i != count; ++i)
					text_events.push_back({decoded[i], event.timestamp});
			}
			break;
		case NPlatform::EventType::focusLost:
			pressed.fill(false);
			repeating_key = 0;
			break;
		default:
			break;
		}
	}

	void Advance(const std::uint64_t timestamp)
	{
		if (repeating_key == 0 || !pressed[repeating_key]) return;
		while (timestamp >= next_repeat)
		{
			key_events.push_back({repeating_key, next_repeat, true});
			next_repeat += kRepeatPeriodMs;
		}
	}
};

NPlatform::PlatformEvent KeyEvent(const NPlatform::EventType type, const std::uint64_t timestamp)
{
	NPlatform::PlatformEvent event = {};
	event.type = type;
	event.timestamp = timestamp;
	event.scancode = 4; // SDL scancode A, mapped to the legacy A control.
	return event;
}

NPlatform::PlatformEvent TextEvent(const char *text, const std::uint64_t timestamp)
{
	NPlatform::PlatformEvent event = {};
	event.type = NPlatform::EventType::textInput;
	event.timestamp = timestamp;
	std::snprintf(event.text, sizeof(event.text), "%s", text);
	return event;
}

bool Check(const bool condition, const char *message)
{
	if (!condition) std::fprintf(stderr, "input text/repeat fixture failed: %s\n", message);
	return condition;
}

bool TestDecodeUtf8()
{
	const char text[] = "A\xC3\xA9\xF0\x9F\x92\xA5";
	std::uint16_t decoded[kTextCapacity] = {};
	const std::size_t count = NInput::DecodeUtf8(text, decoded, kTextCapacity);
	if (!Check(count == 4, "BMP and supplementary UTF-8 code units")) return false;
	if (!Check(decoded[0] == 'A' && decoded[1] == 0x00e9, "BMP characters decode directly")) return false;
	return Check(decoded[2] == 0xd83d && decoded[3] == 0xdca5, "supplementary character decodes to a surrogate pair");
}

bool TestTextAndKeyStreamsAreSeparate()
{
	InputTextRepeatOracle input;
	input.Consume(KeyEvent(NPlatform::EventType::keyDown, 100));
	input.Consume(TextEvent("a", 101));
	if (!Check(input.key_events.size() == 1, "text input does not duplicate the key event")) return false;
	if (!Check(input.key_events[0].code == NInput::CodeForName("A"), "key event keeps its legacy control code")) return false;
	if (!Check(input.text_events.size() == 1 && input.text_events[0].code_unit == 'a',
	           "text input is delivered through the text stream")) return false;
	return Check(!input.key_events[0].repeat, "physical key event is not marked as a repeat");
}

bool TestRepeatDelayAndRate()
{
	InputTextRepeatOracle input;
	input.Consume(KeyEvent(NPlatform::EventType::keyDown, 0));
	input.Advance(kRepeatDelayMs - 1);
	if (!Check(input.key_events.size() == 1, "repeat waits for the explicit delay")) return false;
	input.Advance(kRepeatDelayMs);
	if (!Check(input.key_events.size() == 2 && input.key_events.back().timestamp == kRepeatDelayMs,
	           "first repeat occurs at the delay boundary")) return false;
	input.Advance(kRepeatDelayMs + kRepeatPeriodMs - 1);
	if (!Check(input.key_events.size() == 2, "repeat waits for the explicit rate period")) return false;
	input.Advance(kRepeatDelayMs + kRepeatPeriodMs);
	if (!Check(input.key_events.size() == 3 && input.key_events.back().timestamp == 530,
	           "second repeat occurs at one rate period")) return false;
	return Check(input.key_events.back().repeat, "generated repeats are distinguished from the physical key event");
}

bool TestDisabledTextMode()
{
	InputTextRepeatOracle input;
	input.text_enabled = false;
	input.Consume(TextEvent("disabled", 20));
	input.Consume(KeyEvent(NPlatform::EventType::keyDown, 21));
	if (!Check(input.text_events.empty(), "disabled text mode drops text input")) return false;
	return Check(input.key_events.size() == 1 && input.pressed[NInput::CodeForName("A")],
	             "disabled text mode still accepts key events");
}

bool TestFocusLossClearsPressedState()
{
	InputTextRepeatOracle input;
	input.Consume(KeyEvent(NPlatform::EventType::keyDown, 0));
	input.Advance(kRepeatDelayMs);
	const std::size_t repeats_before_focus_loss = input.key_events.size();
	NPlatform::PlatformEvent focus_loss = {};
	focus_loss.type = NPlatform::EventType::focusLost;
	focus_loss.timestamp = kRepeatDelayMs + 1;
	input.Consume(focus_loss);
	input.Advance(10'000);
	if (!Check(!input.pressed[NInput::CodeForName("A")], "focus loss clears the pressed key state")) return false;
	return Check(input.key_events.size() == repeats_before_focus_loss, "focus loss stops future repeats");
}
}

int main()
{
	if (!TestDecodeUtf8()) return 1;
	if (!TestTextAndKeyStreamsAreSeparate()) return 1;
	if (!TestRepeatDelayAndRate()) return 1;
	if (!TestDisabledTextMode()) return 1;
	if (!TestFocusLossClearsPressedState()) return 1;
	std::puts("input text, repeat, and focus fixture passed");
	return 0;
}
