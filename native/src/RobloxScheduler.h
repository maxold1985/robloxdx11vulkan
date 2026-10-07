#pragma once

struct lua_State;

namespace RobloxScheduler
{
	void install(lua_State* L);
	void shutdown(lua_State* L);

	void step(
		lua_State* L,
		double deltaTime
	);

	void suspend(
		lua_State* L
	);

	bool resume(
		lua_State* L,
		lua_State* thread,
		int argumentCount
	);

	bool isManaged(
		lua_State* L,
		lua_State* thread
	);
}
