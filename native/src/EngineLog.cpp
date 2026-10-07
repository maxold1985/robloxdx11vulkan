#include "EngineLog.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>
#include <utility>

namespace
{
	constexpr std::size_t MAX_HISTORY =
		2000;

	std::mutex logMutex;

	std::array<
		bool,
		static_cast<std::size_t>(
			EngineLog::Component::Count
		)
	> enabled = {
		true,
		true,
		true,
		true,
		true,
		true,
		true,
		true,
		true
	};

	std::deque<
		EngineLog::Entry
	> history;

	EngineLog::Sink sink;

	std::uint64_t nextSequence = 1;

	std::size_t indexOf(
		EngineLog::Component component
	)
	{
		return static_cast<std::size_t>(
			component
		);
	}
}

const char* EngineLog::name(
	Component component
)
{
	switch (component)
	{
	case Component::Luau:
		return "Luau";

	case Component::ObjectModel:
		return "ObjectModel";

	case Component::Scheduler:
		return "Scheduler";

	case Component::Renderer:
		return "Renderer";

	case Component::Physics:
		return "Physics";

	case Component::Input:
		return "Input";

	case Component::RemoteEvent:
		return "RemoteEvent";

	case Component::Raycast:
		return "Raycast";

	case Component::Network:
		return "Network";

	default:
		return "Unknown";
	}
}

std::size_t EngineLog::componentCount()
{
	return static_cast<std::size_t>(
		Component::Count
	);
}

EngineLog::Component EngineLog::componentAt(
	std::size_t index
)
{
	if (index >= componentCount())
		return Component::Luau;

	return static_cast<Component>(
		index
	);
}

bool EngineLog::isEnabled(
	Component component
)
{
	std::lock_guard<std::mutex> lock(
		logMutex
	);

	return enabled[indexOf(component)];
}

void EngineLog::setEnabled(
	Component component,
	bool value
)
{
	std::lock_guard<std::mutex> lock(
		logMutex
	);

	enabled[indexOf(component)] =
		value;
}

bool EngineLog::toggle(
	Component component
)
{
	std::lock_guard<std::mutex> lock(
		logMutex
	);

	bool& value =
		enabled[indexOf(component)];

	value = !value;
	return value;
}

void EngineLog::setSink(
	Sink newSink
)
{
	std::lock_guard<std::mutex> lock(
		logMutex
	);

	sink = std::move(newSink);
}

void EngineLog::clear()
{
	std::lock_guard<std::mutex> lock(
		logMutex
	);

	history.clear();
}

std::vector<EngineLog::Entry>
EngineLog::snapshot()
{
	std::lock_guard<std::mutex> lock(
		logMutex
	);

	return std::vector<Entry>(
		history.begin(),
		history.end()
	);
}

void EngineLog::write(
	Component component,
	const std::string& text
)
{
	Sink activeSink;
	Entry entry;

	{
		std::lock_guard<std::mutex> lock(
			logMutex
		);

		if (!enabled[indexOf(component)])
			return;

		entry.sequence =
			nextSequence++;

		entry.component =
			component;

		entry.text = text;

		history.push_back(entry);

		while (
			history.size() >
			MAX_HISTORY
		)
		{
			history.pop_front();
		}

		activeSink = sink;
	}

	std::fprintf(
		stderr,
		"[%s] %s\n",
		name(component),
		text.c_str()
	);

	if (activeSink)
		activeSink(entry);
}

void EngineLog::writef(
	Component component,
	const char* format,
	...
)
{
	if (!format)
		return;

	char buffer[2048] = {};

	va_list arguments;
	va_start(arguments, format);

	std::vsnprintf(
		buffer,
		sizeof(buffer),
		format,
		arguments
	);

	va_end(arguments);

	buffer[
		sizeof(buffer) - 1
	] = '\0';

	write(
		component,
		buffer
	);
}
