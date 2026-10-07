#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace EngineLog
{
	enum class Component : std::size_t
	{
		Luau = 0,
		ObjectModel,
		Scheduler,
		Renderer,
		Physics,
		Input,
		RemoteEvent,
		Raycast,
		Network,
		Count
	};

	struct Entry
	{
		std::uint64_t sequence = 0;
		Component component =
			Component::Luau;
		std::string text;
	};

	using Sink =
		std::function<
			void(const Entry&)
		>;

	const char* name(
		Component component
	);

	std::size_t componentCount();

	Component componentAt(
		std::size_t index
	);

	bool isEnabled(
		Component component
	);

	void setEnabled(
		Component component,
		bool enabled
	);

	bool toggle(
		Component component
	);

	void setSink(
		Sink sink
	);

	void clear();

	std::vector<Entry> snapshot();

	void write(
		Component component,
		const std::string& text
	);

	void writef(
		Component component,
		const char* format,
		...
	);
}
