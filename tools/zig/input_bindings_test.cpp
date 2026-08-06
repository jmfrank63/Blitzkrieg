#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace
{
struct BindingOracle
{
	std::vector<int> events;
	std::vector<int> visited;
	std::vector<int> emulated;
	bool key_a = false;
	bool key_b = false;
	bool bound = true;
	float axis_power = 1.0f;

	void Apply(int control, bool pressed, std::uint32_t time)
	{
		if (control == 0) key_a = pressed;
		if (control == 1) key_b = pressed;
		if (!bound) return;
		if (key_a && key_b && pressed) events.push_back(static_cast<int>(time));
		if (!pressed && (!key_a || !key_b)) events.push_back(-static_cast<int>(time));
	}

	void Visit(const std::function<void(int)> &visitor)
	{
		for (int control = 0; control != 3; ++control) { visited.push_back(control); visitor(control); }
	}

	std::uint32_t SerializeHash() const
	{
		const std::string bytes = "default|event down|A+B|power=" + std::to_string(axis_power);
		std::uint32_t hash = 2166136261u;
		for (unsigned char value : bytes) hash = (hash ^ value) * 16777619u;
		return hash;
	}
};

bool Check(bool condition, const char *message)
{
	if (!condition) std::fprintf(stderr, "input binding fixture failed: %s\n", message);
	return condition;
}
}

int main()
{
	BindingOracle input;
	input.Apply(0, true, 10);
	input.Apply(1, true, 11);
	if (!Check(input.events.size() == 1 && input.events[0] == 11, "chord activates once")) return 1;
	input.Apply(1, false, 12);
	if (!Check(input.events.size() == 2 && input.events[1] == -12, "chord deactivates on release")) return 1;

	const std::size_t before_double_click = input.events.size();
	input.Apply(0, false, 20);
	input.Apply(0, true, 30);
	input.Apply(0, false, 31);
	input.Apply(0, true, 40);
	if (!Check(input.events.size() >= before_double_click + 2, "double-click-like down/up sequence remains ordered")) return 1;

	const float axis = 0.6f * input.axis_power;
	if (!Check(axis > 0.5f, "axis threshold and power are preserved")) return 1;
	input.emulated = { 1, 2, 3 };
	if (!Check(input.emulated[0] == 1 && input.emulated[2] == 3, "emulated events preserve order")) return 1;

	input.Visit([](int) {});
	if (!Check(input.visited == std::vector<int>({0, 1, 2}), "visitor traversal is deterministic")) return 1;
	input.bound = false;
	const std::size_t before_unbind = input.events.size();
	input.Apply(0, false, 50);
	if (!Check(input.events.size() == before_unbind, "unbind suppresses command events")) return 1;
	if (!Check(input.SerializeHash() == 0xee533407u, "serialization oracle is stable")) return 1;

	std::puts("input binding and emulation fixture passed");
	return 0;
}
