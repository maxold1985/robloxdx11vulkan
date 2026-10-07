#pragma once

#include <string>

struct lua_State;

class LuauRuntime
{
public:
	LuauRuntime();
	~LuauRuntime();

	LuauRuntime(
		const LuauRuntime&
	) = delete;

	LuauRuntime& operator=(
		const LuauRuntime&
	) = delete;

	bool execute(
		const std::string& source,
		const std::string& chunkName
	);

	bool executeFile(
		const std::string& path
	);

	void step(double deltaTime);

	void emitKey(
		const std::string& keyCodeName,
		bool pressed
	);

private:
	lua_State* state_;
};
