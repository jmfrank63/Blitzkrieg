#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace
{
enum class ControlKind : std::uint8_t { button = 1, axis = 2 };

// These values intentionally mirror EInputBindActivationType without pulling
// the legacy object graph into this backend-neutral fixture.
enum class Activation : std::uint8_t
{
	event_down = 1,
	event_up = 2,
	slider_plus = 3,
	slider_minus = 4,
};

struct Binding
{
	std::string command;
	Activation activation;
	std::vector<std::string> controls;
};

struct Control
{
	std::string name;
	ControlKind kind;
	int threshold = 1;
	float power = 1.0f;
	int value = 0;
	bool active = false;
	std::int64_t last_release = -1;
	std::string double_click;
};

struct EmulatedEvent
{
	std::string control;
	int value;
	std::uint64_t timestamp;
	int parameter;
};

struct CommandRecord
{
	std::string command;
	Activation activation;
	std::uint64_t timestamp;
	int parameter;
	int value;
};

bool Contains(const std::vector<std::string> &controls, const std::string &name)
{
	return std::find(controls.begin(), controls.end(), name) != controls.end();
}

bool SameBinding(const Binding &left, const Binding &right)
{
	return left.command == right.command && left.activation == right.activation && left.controls == right.controls;
}

class RecordingVisitor
{
public:
	std::vector<std::string> entries;

	void VisitControl(const std::string &name) { entries.push_back("control:" + name); }
	void VisitCombo(const Binding &binding) { entries.push_back("combo:" + binding.command); }
	void VisitBind(const Binding &binding) { entries.push_back("bind:" + binding.command); }
	void VisitCommand(const Binding &binding) { entries.push_back("command:" + binding.command); }
};

class BindingFixture
{
	std::map<std::string, Control> controls;
	std::vector<Binding> bindings;
	std::map<std::size_t, bool> formed;
	std::queue<EmulatedEvent> emulated;
	std::vector<CommandRecord> commands;

	static constexpr std::int64_t double_click_window = 250;

	int EffectiveAxis(const Control &control) const
	{
		return static_cast<int>(std::lround(static_cast<float>(control.value) * control.power));
	}

	bool IsFormed(const Binding &binding) const
	{
		for (const std::string &name : binding.controls)
		{
			const auto control = controls.find(name);
			if (control == controls.end() || !control->second.active)
				return false;
		}
		return true;
	}

	int BindingValue(const Binding &binding) const
	{
		int value = 0;
		for (const std::string &name : binding.controls)
		{
			const Control &control = controls.at(name);
			if (control.kind == ControlKind::axis)
				value = std::max(value, EffectiveAxis(control));
		}
		return value;
	}

	void Notify(const std::size_t index, const std::string &changed, const std::uint64_t timestamp, const int parameter)
	{
		Binding &binding = bindings[index];
		const bool now_formed = IsFormed(binding);
		const bool was_formed = formed[index];
		formed[index] = now_formed;

		if (binding.activation == Activation::event_down && now_formed && !was_formed)
			commands.push_back({binding.command, binding.activation, timestamp, parameter, 1});
		else if (binding.activation == Activation::event_up && !now_formed && was_formed)
			commands.push_back({binding.command, binding.activation, timestamp, parameter, 0});
		else if ((binding.activation == Activation::slider_plus || binding.activation == Activation::slider_minus) &&
				Contains(binding.controls, changed) && (now_formed || was_formed))
		{
			const int value = now_formed ? BindingValue(binding) : 0;
			commands.push_back({binding.command, binding.activation, timestamp, parameter, value});
		}
	}

	void ApplyOne(const EmulatedEvent &event)
	{
		auto current = controls.find(event.control);
		if (current == controls.end())
			return;

		Control &control = current->second;
		const bool was_active = control.active;
		control.value = event.value;
		control.active = control.kind == ControlKind::button
			? event.value != 0
			: std::abs(event.value) >= control.threshold;

		if (control.active != was_active)
		{
			for (std::size_t i = 0; i != bindings.size(); ++i)
				if (Contains(bindings[i].controls, control.name))
					Notify(i, control.name, event.timestamp, event.parameter);
		}

		if (control.kind == ControlKind::button && event.value != 0 &&
				!control.double_click.empty() && control.last_release >= 0 &&
				event.timestamp - control.last_release <= double_click_window)
		{
			ApplyOne({control.double_click, 1, event.timestamp, event.parameter});
		}
		if (control.kind == ControlKind::button && event.value == 0)
		{
			if (!control.double_click.empty())
				ApplyOne({control.double_click, 0, event.timestamp, event.parameter});
			control.last_release = static_cast<std::int64_t>(event.timestamp);
		}
	}

public:
	void AddControl(const std::string &name, const ControlKind kind)
	{
		Control control;
		control.name = name;
		control.kind = kind;
		controls.insert({name, control});
	}

	void SetThreshold(const std::string &name, const int threshold)
	{
		controls.at(name).threshold = threshold;
	}

	void SetPower(const std::string &name, const float power)
	{
		controls.at(name).power = power;
	}

	bool AddDoubleClick(const std::string &name)
	{
		auto source = controls.find(name);
		if (source == controls.end() || source->second.kind != ControlKind::button)
			return false;
		const std::string generated = name + "_DBLCLK";
		AddControl(generated, ControlKind::button);
		source->second.double_click = generated;
		return true;
	}

	void Bind(const std::string &command, const Activation activation, std::vector<std::string> names)
	{
		bindings.push_back({command, activation, std::move(names)});
		formed[bindings.size() - 1] = false;
	}

	bool Unbind(const Binding &target)
	{
		const auto it = std::find_if(bindings.begin(), bindings.end(), [&](const Binding &candidate) {
			return SameBinding(candidate, target);
		});
		if (it == bindings.end())
			return false;
		const std::size_t removed = static_cast<std::size_t>(it - bindings.begin());
		bindings.erase(it);
		formed.clear();
		for (std::size_t i = 0; i != bindings.size(); ++i)
			formed[i] = false;
		(void)removed;
		return true;
	}

	void Emulate(const std::string &control, const int value, const std::uint64_t timestamp, const int parameter)
	{
		emulated.push({control, value, timestamp, parameter});
	}

	void FlushEmulated()
	{
		while (!emulated.empty())
		{
			const EmulatedEvent event = emulated.front();
			emulated.pop();
			ApplyOne(event);
		}
	}

	const std::vector<CommandRecord> &CommandSequence() const { return commands; }
	const std::map<std::string, Control> &Controls() const { return controls; }
	const std::vector<Binding> &Bindings() const { return bindings; }

	void Visit(RecordingVisitor &visitor) const
	{
		// The map and explicit insertion order make this walk stable across hosts.
		for (const auto &control : controls)
		{
			visitor.VisitControl(control.first);
			for (const Binding &binding : bindings)
				if (Contains(binding.controls, control.first))
				{
					visitor.VisitCombo(binding);
					visitor.VisitBind(binding);
					visitor.VisitCommand(binding);
				}
		}
	}
};

struct Config
{
	std::map<std::string, float> powers;
	std::vector<std::string> double_clicks;
	std::vector<std::string> system_commands;
	std::vector<Binding> bindings;
};

void PutU32(std::vector<std::uint8_t> &bytes, const std::uint32_t value)
{
	for (unsigned shift = 0; shift != 32; shift += 8)
		bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint32_t GetU32(const std::vector<std::uint8_t> &bytes, std::size_t &offset)
{
	std::uint32_t value = 0;
	for (unsigned shift = 0; shift != 32; shift += 8)
		value |= static_cast<std::uint32_t>(bytes.at(offset++)) << shift;
	return value;
}

void PutString(std::vector<std::uint8_t> &bytes, const std::string &value)
{
	PutU32(bytes, static_cast<std::uint32_t>(value.size()));
	bytes.insert(bytes.end(), value.begin(), value.end());
}

std::string GetString(const std::vector<std::uint8_t> &bytes, std::size_t &offset)
{
	const std::uint32_t size = GetU32(bytes, offset);
	const std::string value(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
		bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
	offset += size;
	return value;
}

std::vector<Binding> CanonicalBindings(const std::vector<Binding> &input)
{
	std::vector<Binding> result = input;
	for (Binding &binding : result)
		std::sort(binding.controls.begin(), binding.controls.end());
	std::sort(result.begin(), result.end(), [](const Binding &left, const Binding &right) {
		if (left.command != right.command) return left.command < right.command;
		if (left.activation != right.activation) return left.activation < right.activation;
		return left.controls < right.controls;
	});
	return result;
}

std::vector<std::uint8_t> Serialize(const Config &config)
{
	std::vector<std::uint8_t> bytes{'B', 'K', 'I', 'B', 1};
	PutU32(bytes, static_cast<std::uint32_t>(config.powers.size()));
	for (const auto &power : config.powers)
	{
		PutString(bytes, power.first);
		std::uint32_t bits = 0;
		static_assert(sizeof(bits) == sizeof(power.second), "float oracle requires IEEE-754 storage");
		std::memcpy(&bits, &power.second, sizeof(bits));
		PutU32(bytes, bits);
	}

	std::vector<std::string> double_clicks = config.double_clicks;
	std::sort(double_clicks.begin(), double_clicks.end());
	PutU32(bytes, static_cast<std::uint32_t>(double_clicks.size()));
	for (const std::string &name : double_clicks) PutString(bytes, name);

	std::vector<std::string> system_commands = config.system_commands;
	std::sort(system_commands.begin(), system_commands.end());
	PutU32(bytes, static_cast<std::uint32_t>(system_commands.size()));
	for (const std::string &name : system_commands) PutString(bytes, name);

	PutU32(bytes, 1); // one canonical "default" mapping section
	PutString(bytes, "default");
	const std::vector<Binding> bindings = CanonicalBindings(config.bindings);
	PutU32(bytes, static_cast<std::uint32_t>(bindings.size()));
	for (const Binding &binding : bindings)
	{
		PutString(bytes, binding.command);
		bytes.push_back(static_cast<std::uint8_t>(binding.activation));
		PutU32(bytes, static_cast<std::uint32_t>(binding.controls.size()));
		for (const std::string &control : binding.controls) PutString(bytes, control);
	}
	return bytes;
}

Config Deserialize(const std::vector<std::uint8_t> &bytes)
{
	Config config;
	std::size_t offset = 5;
	const std::uint32_t power_count = GetU32(bytes, offset);
	for (std::uint32_t i = 0; i != power_count; ++i)
	{
		const std::string name = GetString(bytes, offset);
		const std::uint32_t bits = GetU32(bytes, offset);
		float power = 0.0f;
		std::memcpy(&power, &bits, sizeof(power));
		config.powers[name] = power;
	}
	const std::uint32_t double_click_count = GetU32(bytes, offset);
	for (std::uint32_t i = 0; i != double_click_count; ++i) config.double_clicks.push_back(GetString(bytes, offset));
	const std::uint32_t system_count = GetU32(bytes, offset);
	for (std::uint32_t i = 0; i != system_count; ++i) config.system_commands.push_back(GetString(bytes, offset));
	const std::uint32_t section_count = GetU32(bytes, offset);
	for (std::uint32_t section = 0; section != section_count; ++section)
	{
		if (GetString(bytes, offset) != "default") return Config{};
		const std::uint32_t bind_count = GetU32(bytes, offset);
		for (std::uint32_t i = 0; i != bind_count; ++i)
		{
			Binding binding;
			binding.command = GetString(bytes, offset);
			binding.activation = static_cast<Activation>(bytes.at(offset++));
			const std::uint32_t control_count = GetU32(bytes, offset);
			for (std::uint32_t control = 0; control != control_count; ++control)
				binding.controls.push_back(GetString(bytes, offset));
			config.bindings.push_back(std::move(binding));
		}
	}
	if (offset != bytes.size() || bytes.size() < 5 || bytes[0] != 'B' || bytes[1] != 'K' || bytes[2] != 'I' || bytes[3] != 'B' || bytes[4] != 1)
		return Config{};
	return config;
}

bool SameConfig(const Config &left, const Config &right)
{
	const std::vector<Binding> left_bindings = CanonicalBindings(left.bindings);
	const std::vector<Binding> right_bindings = CanonicalBindings(right.bindings);
	if (left.powers != right.powers || left.double_clicks != right.double_clicks || left.system_commands != right.system_commands ||
		left_bindings.size() != right_bindings.size())
		return false;
	for (std::size_t i = 0; i != left_bindings.size(); ++i)
		if (!SameBinding(left_bindings[i], right_bindings[i])) return false;
	return true;
}

std::uint64_t Fnv1a64(const std::vector<std::uint8_t> &bytes)
{
	std::uint64_t hash = 14695981039346656037ull;
	for (const std::uint8_t byte : bytes)
	{
		hash ^= byte;
		hash *= 1099511628211ull;
	}
	return hash;
}

bool Check(const bool condition, const char *message)
{
	if (!condition) std::fprintf(stderr, "input bindings fixture failed: %s\n", message);
	return condition;
}

bool TestSingleChordAndDoubleClick()
{
	BindingFixture input;
	input.AddControl("A", ControlKind::button);
	input.AddControl("CTRL", ControlKind::button);
	input.Bind("jump", Activation::event_down, {"A"});
	input.Bind("jump_release", Activation::event_up, {"A"});
	input.Bind("dash", Activation::event_down, {"CTRL", "A"});
	input.Bind("dash_release", Activation::event_up, {"CTRL", "A"});
	input.Emulate("A", 0x80, 10, 10);
	input.Emulate("CTRL", 0x80, 20, 20);
	input.Emulate("A", 0, 30, 30);
	input.Emulate("CTRL", 0, 40, 40);
	input.FlushEmulated();
	const std::vector<CommandRecord> &sequence = input.CommandSequence();
	if (!Check(sequence.size() == 4, "single and chord activation count")) return false;
	if (!Check(sequence[0].command == "jump" && sequence[0].activation == Activation::event_down && sequence[0].timestamp == 10, "single down ordering")) return false;
	if (!Check(sequence[1].command == "dash" && sequence[1].timestamp == 20, "chord formed after second control")) return false;
	if (!Check(sequence[2].command == "jump_release" && sequence[2].timestamp == 30, "single release ordering")) return false;
	if (!Check(sequence[3].command == "dash_release" && sequence[3].timestamp == 30, "chord release ordering")) return false;

	BindingFixture double_click;
	double_click.AddControl("MOUSE_BUTTON0", ControlKind::button);
	if (!Check(double_click.AddDoubleClick("MOUSE_BUTTON0"), "double-click control generation")) return false;
	double_click.Bind("select", Activation::event_down, {"MOUSE_BUTTON0_DBLCLK"});
	double_click.Emulate("MOUSE_BUTTON0", 0x80, 100, 0);
	double_click.Emulate("MOUSE_BUTTON0", 0, 120, 0);
	double_click.Emulate("MOUSE_BUTTON0", 0x80, 200, 0);
	double_click.Emulate("MOUSE_BUTTON0", 0, 220, 0);
	double_click.FlushEmulated();
	return Check(double_click.CommandSequence().size() == 1 &&
		double_click.CommandSequence()[0].command == "select" &&
		double_click.CommandSequence()[0].timestamp == 200, "double-click-like sequence");
}

bool TestAxisThresholdAndPower()
{
	BindingFixture input;
	input.AddControl("MOUSE_AXIS_X", ControlKind::axis);
	input.SetThreshold("MOUSE_AXIS_X", 4000);
	input.SetPower("MOUSE_AXIS_X", 1.5f);
	input.Bind("pan", Activation::slider_plus, {"MOUSE_AXIS_X"});
	input.Emulate("MOUSE_AXIS_X", 3999, 10, 0);
	input.Emulate("MOUSE_AXIS_X", 6000, 20, 0);
	input.Emulate("MOUSE_AXIS_X", 3000, 30, 0);
	input.FlushEmulated();
	const auto &sequence = input.CommandSequence();
	if (!Check(sequence.size() == 2, "axis threshold suppresses sub-threshold values")) return false;
	if (!Check(sequence[0].value == 9000 && sequence[0].timestamp == 20, "axis power multiplier")) return false;
	return Check(sequence[1].value == 0 && sequence[1].timestamp == 30, "axis threshold release");
}

bool TestEmulatedOrdering()
{
	BindingFixture input;
	input.AddControl("A", ControlKind::button);
	input.AddControl("B", ControlKind::button);
	input.Bind("first", Activation::event_down, {"A"});
	input.Bind("second", Activation::event_down, {"B"});
	// Parameter order is deliberately 99 then 1; the queue remains FIFO.
	input.Emulate("A", 0x80, 50, 99);
	input.Emulate("B", 0x80, 40, 1);
	input.FlushEmulated();
	const auto &sequence = input.CommandSequence();
	if (!Check(sequence.size() == 2, "emulated event count")) return false;
	if (!Check(sequence[0].command == "first" && sequence[0].parameter == 99, "FIFO emulated event first")) return false;
	return Check(sequence[1].command == "second" && sequence[1].parameter == 1, "FIFO ignores legacy sequence sorting");
}

bool TestVisitorAndUnbind()
{
	BindingFixture input;
	input.AddControl("A", ControlKind::button);
	input.AddControl("CTRL", ControlKind::button);
	input.Bind("jump", Activation::event_down, {"A"});
	input.Bind("dash", Activation::event_down, {"CTRL", "A"});
	RecordingVisitor visitor;
	input.Visit(visitor);
	const std::vector<std::string> expected = {
		"control:A", "combo:jump", "bind:jump", "command:jump", "combo:dash", "bind:dash", "command:dash",
		"control:CTRL", "combo:dash", "bind:dash", "command:dash",
	};
	if (!Check(visitor.entries == expected, "visitor control/combo/bind/command traversal")) return false;

	if (!Check(input.Unbind({"jump", Activation::event_down, {"A"}}), "unbind existing binding")) return false;
	if (!Check(!input.Unbind({"jump", Activation::event_down, {"A"}}), "unbind is idempotently absent")) return false;
	input.Emulate("A", 0x80, 10, 0);
	input.FlushEmulated();
	return Check(input.CommandSequence().empty(), "unbound command is not emitted");
}

bool TestSerializationOracle(std::size_t &byte_count, std::uint64_t &hash)
{
	Config first;
	first.powers["MOUSE_AXIS_X"] = 1.5f;
	first.double_clicks = {"MOUSE_BUTTON0"};
	first.system_commands = {"system_pause"};
	first.bindings = {
		{"dash", Activation::event_down, {"CTRL", "A"}},
		{"pan", Activation::slider_plus, {"MOUSE_AXIS_X"}},
	};
	Config second;
	second.powers["MOUSE_AXIS_X"] = 1.5f;
	second.double_clicks = {"MOUSE_BUTTON0"};
	second.system_commands = {"system_pause"};
	second.bindings = {
		{"pan", Activation::slider_plus, {"MOUSE_AXIS_X"}},
		{"dash", Activation::event_down, {"A", "CTRL"}},
	};
	const std::vector<std::uint8_t> bytes = Serialize(first);
	const std::vector<std::uint8_t> equivalent = Serialize(second);
	if (!Check(bytes == equivalent, "canonical serialization is insertion-order independent")) return false;
	const Config round_trip = Deserialize(bytes);
	if (!Check(SameConfig(first, round_trip), "serialization round-trip")) return false;
	byte_count = bytes.size();
	hash = Fnv1a64(bytes);
	// This is the accepted byte/hash oracle for the canonical fixture above.
	constexpr std::size_t expected_bytes = 143;
	constexpr std::uint64_t expected_hash = 0xdc2cfaa2cd056d5aull;
	if (!Check(byte_count == expected_bytes && hash == expected_hash, "stable serialization byte/hash oracle")) return false;
	return true;
}
}

int main()
{
	if (!TestSingleChordAndDoubleClick()) return 1;
	if (!TestAxisThresholdAndPower()) return 1;
	if (!TestEmulatedOrdering()) return 1;
	if (!TestVisitorAndUnbind()) return 1;
	std::size_t byte_count = 0;
	std::uint64_t hash = 0;
	if (!TestSerializationOracle(byte_count, hash))
	{
		std::fprintf(stderr, "serialization observed bytes=%zu fnv1a64=%016llx\n", byte_count,
			static_cast<unsigned long long>(hash));
		return 1;
	}
	std::printf("input bindings fixture passed: commands=9 bytes=%zu fnv1a64=%016llx\n", byte_count,
		static_cast<unsigned long long>(hash));
	return 0;
}
