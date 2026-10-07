#include "RobloxTypes.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

extern "C"
{
#include "lua.h"
#include "lualib.h"
}

namespace
{
	constexpr const char* VECTOR3_METATABLE = "Roblox.Vector3";
	constexpr const char* COLOR3_METATABLE = "Roblox.Color3";

	void setTypeField(lua_State* L, const char* typeName)
	{
		lua_pushstring(L, typeName);
		lua_setfield(L, -2, "__type");
	}

	bool readDirectType(
		lua_State* L,
		int index,
		const char* typeName
	)
	{
		if (!lua_istable(L, index))
			return false;

		lua_getfield(L, index, "__type");

		const char* value =
			lua_isstring(L, -1)
				? lua_tostring(L, -1)
				: nullptr;

		const bool matches =
			value != nullptr &&
			std::strcmp(value, typeName) == 0;

		lua_pop(L, 1);
		return matches;
	}

	void pushVector3Raw(
		lua_State* L,
		double x,
		double y,
		double z
	)
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

	RobloxTypes::Vector3Value readVector3(
		lua_State* L,
		int index
	)
	{
		luaL_checktype(L, index, LUA_TTABLE);

		if (!RobloxTypes::isVector3(L, index))
			luaL_typeerror(L, index, "Vector3");

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

	void pushColor3Raw(
		lua_State* L,
		double r,
		double g,
		double b
	)
	{
		lua_createtable(L, 0, 3);

		lua_pushnumber(L, r);
		lua_setfield(L, -2, "R");

		lua_pushnumber(L, g);
		lua_setfield(L, -2, "G");

		lua_pushnumber(L, b);
		lua_setfield(L, -2, "B");

		luaL_getmetatable(L, COLOR3_METATABLE);
		lua_setmetatable(L, -2);
	}

	RobloxTypes::Color3Value readColor3(
		lua_State* L,
		int index
	)
	{
		luaL_checktype(L, index, LUA_TTABLE);

		if (!RobloxTypes::isColor3(L, index))
			luaL_typeerror(L, index, "Color3");

		RobloxTypes::Color3Value value;

		lua_getfield(L, index, "R");
		value.r = luaL_checknumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, index, "G");
		value.g = luaL_checknumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, index, "B");
		value.b = luaL_checknumber(L, -1);
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

		pushVector3Raw(
			L,
			a.x + b.x,
			a.y + b.y,
			a.z + b.z
		);

		return 1;
	}

	int vector3Sub(lua_State* L)
	{
		const auto a = readVector3(L, 1);
		const auto b = readVector3(L, 2);

		pushVector3Raw(
			L,
			a.x - b.x,
			a.y - b.y,
			a.z - b.z
		);

		return 1;
	}

	int vector3Mul(lua_State* L)
	{
		if (
			RobloxTypes::isVector3(L, 1) &&
			lua_isnumber(L, 2)
		)
		{
			const auto a = readVector3(L, 1);
			const double scalar = lua_tonumber(L, 2);

			pushVector3Raw(
				L,
				a.x * scalar,
				a.y * scalar,
				a.z * scalar
			);

			return 1;
		}

		if (
			lua_isnumber(L, 1) &&
			RobloxTypes::isVector3(L, 2)
		)
		{
			const double scalar = lua_tonumber(L, 1);
			const auto a = readVector3(L, 2);

			pushVector3Raw(
				L,
				a.x * scalar,
				a.y * scalar,
				a.z * scalar
			);

			return 1;
		}

		luaL_error(
			L,
			"Vector3 multiplication requires a Vector3 and a number"
		);

		return 0;
	}

	int vector3Div(lua_State* L)
	{
		const auto a = readVector3(L, 1);
		const double scalar = luaL_checknumber(L, 2);

		if (scalar == 0.0)
			luaL_error(L, "attempt to divide Vector3 by zero");

		pushVector3Raw(
			L,
			a.x / scalar,
			a.y / scalar,
			a.z / scalar
		);

		return 1;
	}

	int vector3Unm(lua_State* L)
	{
		const auto value = readVector3(L, 1);

		pushVector3Raw(
			L,
			-value.x,
			-value.y,
			-value.z
		);

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
			const double magnitude =
				std::sqrt(
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

	int color3New(lua_State* L)
	{
		pushColor3Raw(
			L,
			luaL_optnumber(L, 1, 0.0),
			luaL_optnumber(L, 2, 0.0),
			luaL_optnumber(L, 3, 0.0)
		);

		return 1;
	}

	int color3FromRGB(lua_State* L)
	{
		const double r = luaL_optnumber(L, 1, 0.0);
		const double g = luaL_optnumber(L, 2, 0.0);
		const double b = luaL_optnumber(L, 3, 0.0);

		pushColor3Raw(
			L,
			r / 255.0,
			g / 255.0,
			b / 255.0
		);

		return 1;
	}

	int color3Eq(lua_State* L)
	{
		const auto a = readColor3(L, 1);
		const auto b = readColor3(L, 2);

		lua_pushboolean(
			L,
			a.r == b.r &&
			a.g == b.g &&
			a.b == b.b
		);

		return 1;
	}

	int color3ToString(lua_State* L)
	{
		const auto value = readColor3(L, 1);

		char buffer[160];

		std::snprintf(
			buffer,
			sizeof(buffer),
			"%g, %g, %g",
			value.r,
			value.g,
			value.b
		);

		lua_pushstring(L, buffer);
		return 1;
	}

	void pushEnumItemRaw(
		lua_State* L,
		const char* enumType,
		const char* itemName,
		int value
	)
	{
		lua_createtable(L, 0, 5);

		setTypeField(L, "EnumItem");

		lua_pushstring(L, enumType);
		lua_setfield(L, -2, "EnumType");

		lua_pushstring(L, itemName);
		lua_setfield(L, -2, "Name");

		lua_pushinteger(L, value);
		lua_setfield(L, -2, "Value");

		std::string text = "Enum.";
		text += enumType;
		text += ".";
		text += itemName;

		lua_pushlstring(
			L,
			text.data(),
			text.size()
		);
		lua_setfield(L, -2, "__text");
	}

	void addEnumItem(
		lua_State* L,
		const char* enumType,
		const char* itemName,
		int value
	)
	{
		pushEnumItemRaw(
			L,
			enumType,
			itemName,
			value
		);

		lua_setfield(L, -2, itemName);
	}

	int raycastParamsNew(lua_State* L)
	{
		lua_createtable(L, 0, 8);
		setTypeField(L, "RaycastParams");

		RobloxTypes::pushEnumItem(
			L,
			"RaycastFilterType",
			"Exclude"
		);
		lua_setfield(L, -2, "FilterType");

		lua_newtable(L);
		lua_setfield(
			L,
			-2,
			"FilterDescendantsInstances"
		);

		lua_pushnil(L);
		lua_setfield(L, -2, "ExcludeInstances");

		lua_pushnil(L);
		lua_setfield(L, -2, "IncludeInstances");

		lua_pushboolean(L, 0);
		lua_setfield(L, -2, "IgnoreWater");

		lua_pushboolean(L, 0);
		lua_setfield(L, -2, "RespectCanCollide");

		lua_pushboolean(L, 0);
		lua_setfield(L, -2, "BruteForceAllSlow");

		lua_pushstring(L, "Default");
		lua_setfield(L, -2, "CollisionGroup");

		return 1;
	}

	void installEnums(lua_State* L)
	{
		lua_newtable(L);

		lua_newtable(L);
		addEnumItem(L, "KeyCode", "A", 97);
		addEnumItem(L, "KeyCode", "D", 100);
		addEnumItem(L, "KeyCode", "S", 115);
		addEnumItem(L, "KeyCode", "W", 119);
		addEnumItem(L, "KeyCode", "Space", 32);
		lua_setfield(L, -2, "KeyCode");

		lua_newtable(L);
		addEnumItem(
			L,
			"Material",
			"Plastic",
			256
		);
		addEnumItem(
			L,
			"Material",
			"SmoothPlastic",
			272
		);
		lua_setfield(L, -2, "Material");

		lua_newtable(L);
		addEnumItem(
			L,
			"RaycastFilterType",
			"Exclude",
			0
		);
		addEnumItem(
			L,
			"RaycastFilterType",
			"Include",
			1
		);
		lua_setfield(L, -2, "RaycastFilterType");

		lua_setglobal(L, "Enum");
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

	lua_pushcfunction(
		L,
		vector3ToString,
		"Vector3.__tostring"
	);
	lua_setfield(L, -2, "__tostring");

	lua_pushcfunction(
		L,
		vector3Index,
		"Vector3.__index"
	);
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

	luaL_newmetatable(L, COLOR3_METATABLE);

	lua_pushstring(L, "Color3");
	lua_setfield(L, -2, "__type");

	lua_pushcfunction(L, color3Eq, "Color3.__eq");
	lua_setfield(L, -2, "__eq");

	lua_pushcfunction(
		L,
		color3ToString,
		"Color3.__tostring"
	);
	lua_setfield(L, -2, "__tostring");

	lua_pop(L, 1);

	lua_newtable(L);

	lua_pushcfunction(L, color3New, "Color3.new");
	lua_setfield(L, -2, "new");

	lua_pushcfunction(
		L,
		color3FromRGB,
		"Color3.fromRGB"
	);
	lua_setfield(L, -2, "fromRGB");

	lua_setglobal(L, "Color3");

	installEnums(L);

	lua_newtable(L);

	lua_pushcfunction(
		L,
		raycastParamsNew,
		"RaycastParams.new"
	);
	lua_setfield(L, -2, "new");

	lua_setglobal(L, "RaycastParams");
}

void RobloxTypes::pushVector3(
	lua_State* L,
	const Vector3Value& value
)
{
	pushVector3Raw(
		L,
		value.x,
		value.y,
		value.z
	);
}

RobloxTypes::Vector3Value
RobloxTypes::checkVector3(
	lua_State* L,
	int index
)
{
	return readVector3(L, index);
}

bool RobloxTypes::isVector3(
	lua_State* L,
	int index
)
{
	if (!lua_istable(L, index))
		return false;

	if (!lua_getmetatable(L, index))
		return false;

	luaL_getmetatable(L, VECTOR3_METATABLE);

	const bool matches =
		lua_rawequal(L, -1, -2) != 0;

	lua_pop(L, 2);
	return matches;
}

void RobloxTypes::pushColor3(
	lua_State* L,
	const Color3Value& value
)
{
	pushColor3Raw(
		L,
		value.r,
		value.g,
		value.b
	);
}

RobloxTypes::Color3Value
RobloxTypes::checkColor3(
	lua_State* L,
	int index
)
{
	return readColor3(L, index);
}

bool RobloxTypes::isColor3(
	lua_State* L,
	int index
)
{
	if (!lua_istable(L, index))
		return false;

	if (!lua_getmetatable(L, index))
		return false;

	luaL_getmetatable(L, COLOR3_METATABLE);

	const bool matches =
		lua_rawequal(L, -1, -2) != 0;

	lua_pop(L, 2);
	return matches;
}

void RobloxTypes::pushEnumItem(
	lua_State* L,
	const char* enumType,
	const char* itemName
)
{
	lua_getglobal(L, "Enum");

	if (!lua_istable(L, -1))
		luaL_error(L, "Enum is not installed");

	lua_getfield(L, -1, enumType);
	lua_remove(L, -2);

	if (!lua_istable(L, -1))
	{
		luaL_error(
			L,
			"Enum.%s is not installed",
			enumType
		);
	}

	lua_getfield(L, -1, itemName);
	lua_remove(L, -2);

	if (lua_isnil(L, -1))
	{
		luaL_error(
			L,
			"Enum.%s.%s is not installed",
			enumType,
			itemName
		);
	}
}

std::string RobloxTypes::checkEnumItem(
	lua_State* L,
	int index,
	const char* expectedEnumType
)
{
	if (!isEnumItem(L, index))
		luaL_typeerror(L, index, "EnumItem");

	lua_getfield(L, index, "EnumType");

	const char* enumType =
		luaL_checkstring(L, -1);

	const bool matches =
		std::strcmp(
			enumType,
			expectedEnumType
		) == 0;

	lua_pop(L, 1);

	if (!matches)
	{
		luaL_error(
			L,
			"expected Enum.%s item",
			expectedEnumType
		);
	}

	lua_getfield(L, index, "Name");

	const char* name =
		luaL_checkstring(L, -1);

	const std::string result = name;
	lua_pop(L, 1);

	return result;
}

bool RobloxTypes::isEnumItem(
	lua_State* L,
	int index
)
{
	return hasType(L, index, "EnumItem");
}

bool RobloxTypes::hasType(
	lua_State* L,
	int index,
	const char* typeName
)
{
	if (readDirectType(L, index, typeName))
		return true;

	const int valueType = lua_type(L, index);

	if (
		valueType != LUA_TTABLE &&
		valueType != LUA_TUSERDATA
	)
	{
		return false;
	}

	if (!lua_getmetatable(L, index))
		return false;

	lua_getfield(L, -1, "__type");

	const char* value =
		lua_isstring(L, -1)
			? lua_tostring(L, -1)
			: nullptr;

	const bool matches =
		value != nullptr &&
		std::strcmp(value, typeName) == 0;

	lua_pop(L, 2);
	return matches;
}
