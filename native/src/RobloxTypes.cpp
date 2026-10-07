#include "RobloxTypes.h"

#include <cmath>
#include <cstdio>

extern "C"
{
#include "lua.h"
#include "lualib.h"
}

namespace
{
	constexpr const char* VECTOR3_METATABLE = "Roblox.Vector3";

	void pushVector3Raw(lua_State* L, double x, double y, double z)
	{
		lua_createtable(L, 0, 3);

		lua_pushnumber(L, x);
		lua_setfield(L, -2, "X");

		lua_pushnumber(L, y);
		lua_setfield(L, -2, "Y");

		lua_pushnumber(L, z);
		lua_setfield(L, -2, "Z");

		luaL_getmetatable(L, VECTOR3_METATABLE);
		lua_setmetatable(L, -2);
	}

	RobloxTypes::Vector3Value readVector3(lua_State* L, int index)
	{
		luaL_checktype(L, index, LUA_TTABLE);

		if (!RobloxTypes::isVector3(L, index))
		{
			luaL_typeerror(L, index, "Vector3");
		}

		RobloxTypes::Vector3Value value;

		lua_getfield(L, index, "X");
		value.x = luaL_checknumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, index, "Y");
		value.y = luaL_checknumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, index, "Z");
		value.z = luaL_checknumber(L, -1);
		lua_pop(L, 1);

		return value;
	}

	int vector3New(lua_State* L)
	{
		pushVector3Raw(
			L,
			luaL_optnumber(L, 1, 0.0),
			luaL_optnumber(L, 2, 0.0),
			luaL_optnumber(L, 3, 0.0)
		);

		return 1;
	}

	int vector3Add(lua_State* L)
	{
		const auto a = readVector3(L, 1);
		const auto b = readVector3(L, 2);

		pushVector3Raw(L, a.x + b.x, a.y + b.y, a.z + b.z);
		return 1;
	}

	int vector3Sub(lua_State* L)
	{
		const auto a = readVector3(L, 1);
		const auto b = readVector3(L, 2);

		pushVector3Raw(L, a.x - b.x, a.y - b.y, a.z - b.z);
		return 1;
	}

	int vector3Mul(lua_State* L)
	{
		if (RobloxTypes::isVector3(L, 1) && lua_isnumber(L, 2))
		{
			const auto a = readVector3(L, 1);
			const double scalar = lua_tonumber(L, 2);
			pushVector3Raw(L, a.x * scalar, a.y * scalar, a.z * scalar);
			return 1;
		}

		if (lua_isnumber(L, 1) && RobloxTypes::isVector3(L, 2))
		{
			const double scalar = lua_tonumber(L, 1);
			const auto a = readVector3(L, 2);
			pushVector3Raw(L, a.x * scalar, a.y * scalar, a.z * scalar);
			return 1;
		}

		luaL_error(L, "Vector3 multiplication requires a Vector3 and a number");
		return 0;
	}

	int vector3Div(lua_State* L)
	{
		const auto a = readVector3(L, 1);
		const double scalar = luaL_checknumber(L, 2);

		if (scalar == 0.0)
		{
			luaL_error(L, "attempt to divide Vector3 by zero");
		}

		pushVector3Raw(L, a.x / scalar, a.y / scalar, a.z / scalar);
		return 1;
	}

	int vector3Unm(lua_State* L)
	{
		const auto a = readVector3(L, 1);
		pushVector3Raw(L, -a.x, -a.y, -a.z);
		return 1;
	}

	int vector3Eq(lua_State* L)
	{
		const auto a = readVector3(L, 1);
		const auto b = readVector3(L, 2);

		lua_pushboolean(
			L,
			a.x == b.x &&
			a.y == b.y &&
			a.z == b.z
		);

		return 1;
	}

	int vector3ToString(lua_State* L)
	{
		const auto value = readVector3(L, 1);

		char buffer[160];
		std::snprintf(
			buffer,
			sizeof(buffer),
			"%g, %g, %g",
			value.x,
			value.y,
			value.z
		);

		lua_pushstring(L, buffer);
		return 1;
	}

	int vector3Index(lua_State* L)
	{
		const auto value = readVector3(L, 1);
		const char* key = luaL_checkstring(L, 2);

		if (std::strcmp(key, "Magnitude") == 0)
		{
			lua_pushnumber(
				L,
				std::sqrt(
					value.x * value.x +
					value.y * value.y +
					value.z * value.z
				)
			);

			return 1;
		}

		if (std::strcmp(key, "Unit") == 0)
		{
			const double magnitude = std::sqrt(
				value.x * value.x +
				value.y * value.y +
				value.z * value.z
			);

			if (magnitude == 0.0)
			{
				pushVector3Raw(L, 0.0, 0.0, 0.0);
			}
			else
			{
				pushVector3Raw(
					L,
					value.x / magnitude,
					value.y / magnitude,
					value.z / magnitude
				);
			}

			return 1;
		}

		lua_pushnil(L);
		return 1;
	}
}

void RobloxTypes::install(lua_State* L)
{
	luaL_newmetatable(L, VECTOR3_METATABLE);

	lua_pushstring(L, "Vector3");
	lua_setfield(L, -2, "__type");

	lua_pushcfunction(L, vector3Add, "Vector3.__add");
	lua_setfield(L, -2, "__add");

	lua_pushcfunction(L, vector3Sub, "Vector3.__sub");
	lua_setfield(L, -2, "__sub");

	lua_pushcfunction(L, vector3Mul, "Vector3.__mul");
	lua_setfield(L, -2, "__mul");

	lua_pushcfunction(L, vector3Div, "Vector3.__div");
	lua_setfield(L, -2, "__div");

	lua_pushcfunction(L, vector3Unm, "Vector3.__unm");
	lua_setfield(L, -2, "__unm");

	lua_pushcfunction(L, vector3Eq, "Vector3.__eq");
	lua_setfield(L, -2, "__eq");

	lua_pushcfunction(L, vector3ToString, "Vector3.__tostring");
	lua_setfield(L, -2, "__tostring");

	lua_pushcfunction(L, vector3Index, "Vector3.__index");
	lua_setfield(L, -2, "__index");

	lua_pop(L, 1);

	lua_newtable(L);

	lua_pushcfunction(L, vector3New, "Vector3.new");
	lua_setfield(L, -2, "new");

	pushVector3Raw(L, 0.0, 0.0, 0.0);
	lua_setfield(L, -2, "zero");

	pushVector3Raw(L, 1.0, 1.0, 1.0);
	lua_setfield(L, -2, "one");

	lua_setglobal(L, "Vector3");
}

void RobloxTypes::pushVector3(lua_State* L, const Vector3Value& value)
{
	pushVector3Raw(L, value.x, value.y, value.z);
}

RobloxTypes::Vector3Value RobloxTypes::checkVector3(lua_State* L, int index)
{
	return readVector3(L, index);
}

bool RobloxTypes::isVector3(lua_State* L, int index)
{
	if (!lua_istable(L, index))
		return false;

	if (!lua_getmetatable(L, index))
		return false;

	luaL_getmetatable(L, VECTOR3_METATABLE);
	const bool matches = lua_rawequal(L, -1, -2) != 0;
	lua_pop(L, 2);

	return matches;
}
