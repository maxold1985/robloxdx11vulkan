#include "RobloxApi.h"
#include "RobloxObjectModel.h"
#include "RobloxTypes.h"

#include <cstdio>
#include <string>

extern "C"
{
#include "lua.h"
#include "lualib.h"
}

namespace
{
	bool pushDeclaredType(
		lua_State* L,
		int index
	)
	{
		const int valueType =
			lua_type(L, index);

		if (valueType == LUA_TTABLE)
		{
			lua_getfield(
				L,
				index,
				"__type"
			);

			if (lua_isstring(L, -1))
				return true;

			lua_pop(L, 1);
		}

		if (
			valueType == LUA_TTABLE ||
			valueType == LUA_TUSERDATA
		)
		{
			if (lua_getmetatable(L, index))
			{
				lua_getfield(
					L,
					-1,
					"__type"
				);

				if (lua_isstring(L, -1))
				{
					lua_remove(L, -2);
					return true;
				}

				lua_pop(L, 2);
			}
		}

		return false;
	}

	int robloxTypeof(lua_State* L)
	{
		if (pushDeclaredType(L, 1))
			return 1;

		lua_pushstring(
			L,
			lua_typename(
				L,
				lua_type(L, 1)
			)
		);

		return 1;
	}

	int robloxWarn(lua_State* L)
	{
		const int count =
			lua_gettop(L);

		std::fputs("[warn] ", stderr);

		for (
			int index = 1;
			index <= count;
			++index
		)
		{
			size_t length = 0;

			const char* value =
				luaL_tolstring(
					L,
					index,
					&length
				);

			if (index > 1)
				std::fputc('\t', stderr);

			if (value)
			{
				std::fwrite(
					value,
					1,
					length,
					stderr
				);
			}

			lua_pop(L, 1);
		}

		std::fputc('\n', stderr);
		return 0;
	}
}

void RobloxApi::install(lua_State* L)
{
	RobloxTypes::install(L);
	RobloxObjectModel::install(L);

	lua_pushcfunction(
		L,
		robloxTypeof,
		"typeof"
	);
	lua_setglobal(L, "typeof");

	lua_pushcfunction(
		L,
		robloxWarn,
		"warn"
	);
	lua_setglobal(L, "warn");
}

void RobloxApi::shutdown(lua_State* L)
{
	RobloxObjectModel::shutdown(L);
}

void RobloxApi::step(
	lua_State* L,
	double deltaTime
)
{
	RobloxObjectModel::step(
		L,
		deltaTime
	);
}

void RobloxApi::emitKey(
	lua_State* L,
	const std::string& keyCodeName,
	bool pressed
)
{
	RobloxObjectModel::emitKey(
		L,
		keyCodeName,
		pressed
	);
}
