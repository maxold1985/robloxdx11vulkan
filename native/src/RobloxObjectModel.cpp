#include "RobloxObjectModel.h"
#include "RobloxScheduler.h"
#include "RobloxTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lua.h"
#include "lualib.h"

namespace
{
	constexpr const char* INSTANCE_METATABLE =
		"Roblox.Instance";

	constexpr const char* SIGNAL_METATABLE =
		"Roblox.RBXScriptSignal";

	constexpr const char* CONNECTION_METATABLE =
		"Roblox.RBXScriptConnection";

	struct ConnectionState;

	struct AttributeValue
	{
		enum class Type
		{
			Nil,
			Boolean,
			Number,
			String
		};

		Type type = Type::Nil;
		bool booleanValue = false;
		double numberValue = 0.0;
		std::string stringValue;
	};

	struct InstanceObject
	{
		std::string className = "Instance";
		std::string name = "Instance";

		InstanceObject* parent = nullptr;
		std::vector<InstanceObject*> children;

		bool destroyed = false;
		bool archivable = true;

		bool anchored = false;
		bool canCollide = true;
		double transparency = 0.0;

		RobloxTypes::Vector3Value position;
		RobloxTypes::Vector3Value size{4.0, 1.0, 2.0};
		RobloxTypes::Vector3Value assemblyLinearVelocity;

		RobloxTypes::Color3Value color{
			0.6392156862745098,
			0.6352941176470588,
			0.6470588235294118
		};

		std::string material = "Plastic";

		InstanceObject* networkOwner = nullptr;
		InstanceObject* cameraSubject = nullptr;

		long long userId = 0;

		int luaRef = LUA_NOREF;

		std::unordered_map<
			std::string,
			AttributeValue
		> attributes;

		std::vector<
			std::unique_ptr<ConnectionState>
		> ownedConnections;

		std::unordered_map<
			std::string,
			std::vector<ConnectionState*>
		> signals;
	};

	struct ConnectionState
	{
		int callbackRef = LUA_NOREF;
		bool connected = true;
		bool once = false;
	};

	struct ChildWaiter
	{
		InstanceObject* parent = nullptr;
		std::string childName;
		lua_State* thread = nullptr;

		bool hasDeadline = false;
		double deadline = 0.0;
	};

	struct RuntimeContext
	{
		std::vector<
			std::unique_ptr<InstanceObject>
		> objects;

		std::unordered_map<
			std::string,
			InstanceObject*
		> services;

		InstanceObject* dataModel = nullptr;
		InstanceObject* workspace = nullptr;
		InstanceObject* players = nullptr;
		InstanceObject* localPlayer = nullptr;
		InstanceObject* runService = nullptr;
		InstanceObject* userInputService = nullptr;
		InstanceObject* currentCamera = nullptr;

		double gravity = 196.2;
		double time = 0.0;

		std::vector<ChildWaiter> childWaiters;
	};

	struct InstanceUserdata
	{
		InstanceObject* object = nullptr;
	};

	struct SignalUserdata
	{
		InstanceObject* object = nullptr;
		std::string eventName;
	};

	struct ConnectionUserdata
	{
		ConnectionState* connection = nullptr;
	};

	std::unordered_map<
		lua_State*,
		std::unique_ptr<RuntimeContext>
	> contexts;

	RuntimeContext& context(lua_State* L)
	{
		lua_State* mainThread = lua_mainthread(L);

		auto iterator =
			contexts.find(mainThread);

		if (iterator == contexts.end())
		{
			luaL_error(
				L,
				"Roblox object model is not installed"
			);
		}

		return *iterator->second;
	}

	InstanceObject* createObject(
		RuntimeContext& runtime,
		std::string className,
		std::string name = {}
	)
	{
		auto object =
			std::make_unique<InstanceObject>();

		object->className =
			std::move(className);

		object->name =
			name.empty()
				? object->className
				: std::move(name);

		InstanceObject* raw = object.get();

		runtime.objects.push_back(
			std::move(object)
		);

		return raw;
	}

	void pushInstance(
		lua_State* L,
		InstanceObject* object
	)
	{
		if (!object)
		{
			lua_pushnil(L);
			return;
		}

		if (object->luaRef != LUA_NOREF)
		{
			lua_getref(L, object->luaRef);
			return;
		}

		auto* userdata =
			static_cast<InstanceUserdata*>(
				lua_newuserdata(
					L,
					sizeof(InstanceUserdata)
				)
			);

		userdata->object = object;

		luaL_getmetatable(
			L,
			INSTANCE_METATABLE
		);

		lua_setmetatable(L, -2);

		object->luaRef =
			lua_ref(L, -1);
	}

	InstanceObject* checkInstance(
		lua_State* L,
		int index
	)
	{
		auto* userdata =
			static_cast<InstanceUserdata*>(
				luaL_checkudata(
					L,
					index,
					INSTANCE_METATABLE
				)
			);

		if (!userdata->object)
			luaL_error(L, "invalid Instance");

		return userdata->object;
	}

	bool isBasePartClass(
		const std::string& className
	)
	{
		return
			className == "Part" ||
			className == "MeshPart" ||
			className == "WedgePart" ||
			className == "CornerWedgePart" ||
			className == "TrussPart" ||
			className == "UnionOperation";
	}

	bool classIsA(
		const std::string& className,
		const std::string& requested
	)
	{
		if (className == requested)
			return true;

		if (requested == "Instance")
			return true;

		if (
			isBasePartClass(className) &&
			(
				requested == "BasePart" ||
				requested == "PVInstance"
			)
		)
		{
			return true;
		}

		if (
			(
				className == "Model" ||
				className == "Workspace"
			) &&
			requested == "PVInstance"
		)
		{
			return true;
		}

		if (
			className == "DataModel" &&
			requested == "ServiceProvider"
		)
		{
			return true;
		}

		return false;
	}

	bool isAncestorOf(
		const InstanceObject* ancestor,
		const InstanceObject* object
	)
	{
		for (
			const InstanceObject* current =
				object ? object->parent : nullptr;
			current != nullptr;
			current = current->parent
		)
		{
			if (current == ancestor)
				return true;
		}

		return false;
	}

	bool isSelfOrDescendantOf(
		const InstanceObject* object,
		const InstanceObject* ancestor
	)
	{
		return
			object == ancestor ||
			isAncestorOf(ancestor, object);
	}

	InstanceObject* findFirstChild(
		InstanceObject* object,
		const std::string& name,
		bool recursive
	)
	{
		for (
			InstanceObject* child :
			object->children
		)
		{
			if (
				!child->destroyed &&
				child->name == name
			)
			{
				return child;
			}
		}

		if (recursive)
		{
			for (
				InstanceObject* child :
				object->children
			)
			{
				InstanceObject* found =
					findFirstChild(
						child,
						name,
						true
					);

				if (found)
					return found;
			}
		}

		return nullptr;
	}

	void collectDescendants(
		InstanceObject* object,
		std::vector<InstanceObject*>& result
	)
	{
		for (
			InstanceObject* child :
			object->children
		)
		{
			if (child->destroyed)
				continue;

			result.push_back(child);

			collectDescendants(
				child,
				result
			);
		}
	}

	std::string fullName(
		InstanceObject* object
	)
	{
		std::vector<std::string> names;

		for (
			InstanceObject* current = object;
			current != nullptr;
			current = current->parent
		)
		{
			names.push_back(current->name);
		}

		std::string result;

		for (
			auto iterator = names.rbegin();
			iterator != names.rend();
			++iterator
		)
		{
			if (!result.empty())
				result += ".";

			result += *iterator;
		}

		return result;
	}

	void disconnect(
		lua_State* L,
		ConnectionState* connection
	)
	{
		if (
			!connection ||
			!connection->connected
		)
		{
			return;
		}

		connection->connected = false;

		if (
			connection->callbackRef !=
			LUA_NOREF
		)
		{
			connection->callbackRef =
				lua_unref(
					L,
					connection->callbackRef
				);
		}
	}

	void purgeDisconnected(
		InstanceObject* object,
		const std::string& eventName
	)
	{
		auto iterator =
			object->signals.find(eventName);

		if (iterator == object->signals.end())
			return;

		auto& connections =
			iterator->second;

		connections.erase(
			std::remove_if(
				connections.begin(),
				connections.end(),
				[](
					ConnectionState* connection
				)
				{
					return
						connection == nullptr ||
						!connection->connected;
				}
			),
			connections.end()
		);
	}

	template <typename PushArguments>
	void fireEvent(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName,
		int argumentCount,
		PushArguments pushArguments
	)
	{
		if (!object || object->destroyed)
			return;

		auto iterator =
			object->signals.find(eventName);

		if (iterator == object->signals.end())
			return;

		const std::vector<
			ConnectionState*
		> snapshot = iterator->second;

		for (
			ConnectionState* connection :
			snapshot
		)
		{
			if (
				!connection ||
				!connection->connected
			)
			{
				continue;
			}

			lua_getref(
				L,
				connection->callbackRef
			);

			pushArguments();

			const int status =
				lua_pcall(
					L,
					argumentCount,
					0,
					0
				);

			if (status != LUA_OK)
			{
				const char* message =
					lua_tostring(L, -1);

				std::fprintf(
					stderr,
					"[RBXScriptSignal:%s] %s\n",
					eventName.c_str(),
					message
						? message
						: "callback error"
				);

				lua_pop(L, 1);
			}

			if (connection->once)
				disconnect(L, connection);
		}

		purgeDisconnected(
			object,
			eventName
		);
	}

	void fireEvent0(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName
	)
	{
		fireEvent(
			L,
			object,
			eventName,
			0,
			[]() {}
		);
	}

	void fireEventNumber(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName,
		double value
	)
	{
		fireEvent(
			L,
			object,
			eventName,
			1,
			[L, value]()
			{
				lua_pushnumber(L, value);
			}
		);
	}

	void fireEventInstance(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName,
		InstanceObject* argument
	)
	{
		fireEvent(
			L,
			object,
			eventName,
			1,
			[L, argument]()
			{
				pushInstance(L, argument);
			}
		);
	}

	void fireEventInstancePair(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName,
		InstanceObject* first,
		InstanceObject* second
	)
	{
		fireEvent(
			L,
			object,
			eventName,
			2,
			[L, first, second]()
			{
				pushInstance(L, first);
				pushInstance(L, second);
			}
		);
	}

	void fireEventString(
		lua_State* L,
		InstanceObject* object,
		const std::string& eventName,
		const std::string& value
	)
	{
		fireEvent(
			L,
			object,
			eventName,
			1,
			[L, &value]()
			{
				lua_pushlstring(
					L,
					value.data(),
					value.size()
				);
			}
		);
	}

	void firePropertyChanged(
		lua_State* L,
		InstanceObject* object,
		const std::string& property
	)
	{
		fireEventString(
			L,
			object,
			"Changed",
			property
		);

		fireEvent0(
			L,
			object,
			"PropertyChanged:" + property
		);
	}

	void fireAttributeChanged(
		lua_State* L,
		InstanceObject* object,
		const std::string& attribute
	)
	{
		fireEventString(
			L,
			object,
			"AttributeChanged",
			attribute
		);

		fireEvent0(
			L,
			object,
			"AttributeChanged:" + attribute
		);
	}

	void fireAncestryChangedRecursive(
		lua_State* L,
		InstanceObject* object
	)
	{
		fireEventInstancePair(
			L,
			object,
			"AncestryChanged",
			object,
			object->parent
		);

		for (
			InstanceObject* child :
			object->children
		)
		{
			fireAncestryChangedRecursive(
				L,
				child
			);
		}
	}

	void removeChild(
		InstanceObject* parent,
		InstanceObject* child
	)
	{
		if (!parent)
			return;

		auto& children = parent->children;

		children.erase(
			std::remove(
				children.begin(),
				children.end(),
				child
			),
			children.end()
		);
	}

	void setParent(
		lua_State* L,
		InstanceObject* object,
		InstanceObject* newParent
	)
	{
		if (object->destroyed)
		{
			luaL_error(
				L,
				"The Parent property of %s is locked",
				object->name.c_str()
			);
		}

		if (
			newParent &&
			newParent->destroyed
		)
		{
			luaL_error(
				L,
				"Cannot parent %s to a destroyed Instance",
				object->name.c_str()
			);
		}

		if (newParent == object)
		{
			luaL_error(
				L,
				"Attempt to set Instance as its own Parent"
			);
		}

		if (
			newParent &&
			isAncestorOf(object, newParent)
		)
		{
			luaL_error(
				L,
				"Attempt to set Parent would create a cyclic Instance tree"
			);
		}

		if (object->parent == newParent)
			return;

		InstanceObject* oldParent =
			object->parent;

		if (oldParent)
			removeChild(oldParent, object);

		object->parent = newParent;

		if (newParent)
		{
			newParent->children.push_back(
				object
			);
		}

		if (oldParent)
		{
			fireEventInstance(
				L,
				oldParent,
				"ChildRemoved",
				object
			);
		}

		if (newParent)
		{
			fireEventInstance(
				L,
				newParent,
				"ChildAdded",
				object
			);
		}

		firePropertyChanged(
			L,
			object,
			"Parent"
		);

		fireAncestryChangedRecursive(
			L,
			object
		);
	}

	void destroyObject(
		lua_State* L,
		InstanceObject* object
	)
	{
		if (!object || object->destroyed)
			return;

		fireEvent0(
			L,
			object,
			"Destroying"
		);

		const std::vector<
			InstanceObject*
		> children = object->children;

		for (
			InstanceObject* child :
			children
		)
		{
			destroyObject(L, child);
		}

		setParent(L, object, nullptr);

		for (
			auto& connection :
			object->ownedConnections
		)
		{
			disconnect(
				L,
				connection.get()
			);
		}

		object->signals.clear();
		object->destroyed = true;
	}

	double assemblyMass(
		const InstanceObject* object
	)
	{
		const double volume =
			object->size.x *
			object->size.y *
			object->size.z;

		return std::max(
			volume,
			0.001
		);
	}

	void pushSignal(
		lua_State* L,
		InstanceObject* object,
		std::string eventName
	)
	{
		auto destructor =
			[](
				lua_State*,
				void* raw
			)
			{
				static_cast<
					SignalUserdata*
				>(raw)->~SignalUserdata();
			};

		void* raw =
			lua_newuserdatadtor(
				L,
				sizeof(SignalUserdata),
				destructor
			);

		new (raw) SignalUserdata{
			object,
			std::move(eventName)
		};

		luaL_getmetatable(
			L,
			SIGNAL_METATABLE
		);

		lua_setmetatable(L, -2);
	}

	void pushAttributeValue(
		lua_State* L,
		const AttributeValue& value
	)
	{
		switch (value.type)
		{
		case AttributeValue::Type::Boolean:
			lua_pushboolean(
				L,
				value.booleanValue
			);
			break;

		case AttributeValue::Type::Number:
			lua_pushnumber(
				L,
				value.numberValue
			);
			break;

		case AttributeValue::Type::String:
			lua_pushlstring(
				L,
				value.stringValue.data(),
				value.stringValue.size()
			);
			break;

		default:
			lua_pushnil(L);
			break;
		}
	}

	AttributeValue readAttributeValue(
		lua_State* L,
		int index
	)
	{
		AttributeValue result;

		switch (lua_type(L, index))
		{
		case LUA_TNIL:
			result.type =
				AttributeValue::Type::Nil;
			break;

		case LUA_TBOOLEAN:
			result.type =
				AttributeValue::Type::Boolean;

			result.booleanValue =
				lua_toboolean(L, index) != 0;
			break;

		case LUA_TNUMBER:
		case LUA_TINTEGER:
			result.type =
				AttributeValue::Type::Number;

			result.numberValue =
				lua_tonumber(L, index);
			break;

		case LUA_TSTRING:
		{
			result.type =
				AttributeValue::Type::String;

			size_t length = 0;

			const char* text =
				lua_tolstring(
					L,
					index,
					&length
				);

			result.stringValue.assign(
				text ? text : "",
				length
			);
			break;
		}

		default:
			luaL_error(
				L,
				"attribute type %s is not implemented yet",
				lua_typename(
					L,
					lua_type(L, index)
				)
			);
		}

		return result;
	}

	int instanceDestroy(lua_State* L)
	{
		destroyObject(
			L,
			checkInstance(L, 1)
		);

		return 0;
	}

	int instanceGetChildren(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		lua_createtable(
			L,
			static_cast<int>(
				object->children.size()
			),
			0
		);

		int index = 1;

		for (
			InstanceObject* child :
			object->children
		)
		{
			if (child->destroyed)
				continue;

			pushInstance(L, child);

			lua_rawseti(
				L,
				-2,
				index++
			);
		}

		return 1;
	}

	int instanceGetDescendants(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		std::vector<
			InstanceObject*
		> descendants;

		collectDescendants(
			object,
			descendants
		);

		lua_createtable(
			L,
			static_cast<int>(
				descendants.size()
			),
			0
		);

		for (
			size_t index = 0;
			index < descendants.size();
			++index
		)
		{
			pushInstance(
				L,
				descendants[index]
			);

			lua_rawseti(
				L,
				-2,
				static_cast<int>(
					index + 1
				)
			);
		}

		return 1;
	}

	int instanceFindFirstChild(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string name =
			luaL_checkstring(L, 2);

		const bool recursive =
			lua_isnoneornil(L, 3)
				? false
				: lua_toboolean(L, 3) != 0;

		pushInstance(
			L,
			findFirstChild(
				object,
				name,
				recursive
			)
		);

		return 1;
	}

	int instanceWaitForChild(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string name =
			luaL_checkstring(L, 2);

		InstanceObject* child =
			findFirstChild(
				object,
				name,
				false
			);

		if (child)
		{
			pushInstance(L, child);
			return 1;
		}

		if (!lua_isyieldable(L))
		{
			luaL_error(
				L,
				"WaitForChild('%s') cannot yield from this context",
				name.c_str()
			);
		}

		RuntimeContext& runtime =
			context(L);

		ChildWaiter waiter;
		waiter.parent = object;
		waiter.childName = name;
		waiter.thread = L;

		if (!lua_isnoneornil(L, 3))
		{
			const double timeout =
				luaL_checknumber(L, 3);

			if (timeout <= 0.0)
			{
				lua_pushnil(L);
				return 1;
			}

			waiter.hasDeadline = true;
			waiter.deadline =
				runtime.time + timeout;
		}

		RobloxScheduler::suspend(L);

		runtime.childWaiters.push_back(
			std::move(waiter)
		);

		return lua_yield(L, 0);
	}

	int instanceFindFirstChildOfClass(
		lua_State* L
	)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string className =
			luaL_checkstring(L, 2);

		for (
			InstanceObject* child :
			object->children
		)
		{
			if (
				!child->destroyed &&
				child->className == className
			)
			{
				pushInstance(L, child);
				return 1;
			}
		}

		lua_pushnil(L);
		return 1;
	}

	int instanceFindFirstChildWhichIsA(
		lua_State* L
	)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string className =
			luaL_checkstring(L, 2);

		const bool recursive =
			lua_isnoneornil(L, 3)
				? false
				: lua_toboolean(L, 3) != 0;

		std::vector<
			InstanceObject*
		> candidates;

		if (recursive)
		{
			collectDescendants(
				object,
				candidates
			);
		}
		else
		{
			candidates = object->children;
		}

		for (
			InstanceObject* child :
			candidates
		)
		{
			if (
				!child->destroyed &&
				classIsA(
					child->className,
					className
				)
			)
			{
				pushInstance(L, child);
				return 1;
			}
		}

		lua_pushnil(L);
		return 1;
	}

	int instanceIsA(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string className =
			luaL_checkstring(L, 2);

		lua_pushboolean(
			L,
			classIsA(
				object->className,
				className
			)
		);

		return 1;
	}

	int instanceIsDescendantOf(
		lua_State* L
	)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		InstanceObject* ancestor =
			checkInstance(L, 2);

		lua_pushboolean(
			L,
			isAncestorOf(
				ancestor,
				object
			)
		);

		return 1;
	}

	int instanceIsAncestorOf(
		lua_State* L
	)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		InstanceObject* descendant =
			checkInstance(L, 2);

		lua_pushboolean(
			L,
			isAncestorOf(
				object,
				descendant
			)
		);

		return 1;
	}

	int instanceGetFullName(lua_State* L)
	{
		const std::string value =
			fullName(
				checkInstance(L, 1)
			);

		lua_pushlstring(
			L,
			value.data(),
			value.size()
		);

		return 1;
	}

	int instanceClearAllChildren(
		lua_State* L
	)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::vector<
			InstanceObject*
		> children = object->children;

		for (
			InstanceObject* child :
			children
		)
		{
			destroyObject(L, child);
		}

		return 0;
	}

	int instanceGetPropertyChangedSignal(
		lua_State* L
	)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string property =
			luaL_checkstring(L, 2);

		pushSignal(
			L,
			object,
			"PropertyChanged:" +
				property
		);

		return 1;
	}

	int instanceSetAttribute(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string name =
			luaL_checkstring(L, 2);

		AttributeValue value =
			readAttributeValue(L, 3);

		if (
			value.type ==
			AttributeValue::Type::Nil
		)
		{
			object->attributes.erase(name);
		}
		else
		{
			object->attributes[name] =
				std::move(value);
		}

		fireAttributeChanged(
			L,
			object,
			name
		);

		return 0;
	}

	int instanceGetAttribute(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string name =
			luaL_checkstring(L, 2);

		auto iterator =
			object->attributes.find(name);

		if (
			iterator ==
			object->attributes.end()
		)
		{
			lua_pushnil(L);
			return 1;
		}

		pushAttributeValue(
			L,
			iterator->second
		);

		return 1;
	}

	int instanceGetAttributeChangedSignal(
		lua_State* L
	)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string name =
			luaL_checkstring(L, 2);

		pushSignal(
			L,
			object,
			"AttributeChanged:" + name
		);

		return 1;
	}

	int dataModelGetService(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		if (
			object->className !=
			"DataModel"
		)
		{
			luaL_error(
				L,
				"GetService is only valid on DataModel"
			);
		}

		const std::string serviceName =
			luaL_checkstring(L, 2);

		RuntimeContext& runtime =
			context(L);

		auto iterator =
			runtime.services.find(
				serviceName
			);

		if (
			iterator ==
			runtime.services.end()
		)
		{
			luaL_error(
				L,
				"%s is not an available service in this runtime",
				serviceName.c_str()
			);
		}

		pushInstance(
			L,
			iterator->second
		);

		return 1;
	}

	int playersGetPlayers(lua_State* L)
	{
		InstanceObject* players =
			checkInstance(L, 1);

		lua_newtable(L);

		int index = 1;

		for (
			InstanceObject* child :
			players->children
		)
		{
			if (
				child->destroyed ||
				child->className != "Player"
			)
			{
				continue;
			}

			pushInstance(L, child);

			lua_rawseti(
				L,
				-2,
				index++
			);
		}

		return 1;
	}

	void fireRemoteServer(
		lua_State* L,
		InstanceObject* remote,
		InstanceObject* player,
		int firstArgument,
		int lastArgument
	)
	{
		const int payloadCount =
			lastArgument >= firstArgument
				? (
					lastArgument -
					firstArgument +
					1
				)
				: 0;

		fireEvent(
			L,
			remote,
			"OnServerEvent",
			payloadCount + 1,
			[
				L,
				player,
				firstArgument,
				lastArgument
			]()
			{
				pushInstance(L, player);

				for (
					int index =
						firstArgument;
					index <=
						lastArgument;
					++index
				)
				{
					lua_pushvalue(
						L,
						index
					);
				}
			}
		);
	}

	void fireRemoteClient(
		lua_State* L,
		InstanceObject* remote,
		int firstArgument,
		int lastArgument
	)
	{
		const int payloadCount =
			lastArgument >= firstArgument
				? (
					lastArgument -
					firstArgument +
					1
				)
				: 0;

		fireEvent(
			L,
			remote,
			"OnClientEvent",
			payloadCount,
			[
				L,
				firstArgument,
				lastArgument
			]()
			{
				for (
					int index =
						firstArgument;
					index <=
						lastArgument;
					++index
				)
				{
					lua_pushvalue(
						L,
						index
					);
				}
			}
		);
	}

	int remoteEventFireServer(lua_State* L)
	{
		InstanceObject* remote =
			checkInstance(L, 1);

		RuntimeContext& runtime =
			context(L);

		const int top = lua_gettop(L);

		fireRemoteServer(
			L,
			remote,
			runtime.localPlayer,
			2,
			top
		);

		return 0;
	}

	int remoteEventFireClient(lua_State* L)
	{
		InstanceObject* remote =
			checkInstance(L, 1);

		InstanceObject* player =
			checkInstance(L, 2);

		RuntimeContext& runtime =
			context(L);

		if (player == runtime.localPlayer)
		{
			fireRemoteClient(
				L,
				remote,
				3,
				lua_gettop(L)
			);
		}

		return 0;
	}

	int remoteEventFireAllClients(
		lua_State* L
	)
	{
		InstanceObject* remote =
			checkInstance(L, 1);

		fireRemoteClient(
			L,
			remote,
			2,
			lua_gettop(L)
		);

		return 0;
	}

	int basePartSetNetworkOwner(
		lua_State* L
	)
	{
		InstanceObject* part =
			checkInstance(L, 1);

		InstanceObject* player = nullptr;

		if (!lua_isnoneornil(L, 2))
		{
			player =
				checkInstance(L, 2);

			if (
				player->className !=
				"Player"
			)
			{
				luaL_error(
					L,
					"SetNetworkOwner expects a Player or nil"
				);
			}
		}

		part->networkOwner = player;
		return 0;
	}

	int basePartSetNetworkOwnershipAuto(
		lua_State* L
	)
	{
		InstanceObject* part =
			checkInstance(L, 1);

		part->networkOwner = nullptr;
		return 0;
	}

	int basePartApplyImpulse(lua_State* L)
	{
		InstanceObject* part =
			checkInstance(L, 1);

		const auto impulse =
			RobloxTypes::checkVector3(
				L,
				2
			);

		if (part->anchored)
			return 0;

		const double mass =
			assemblyMass(part);

		part->assemblyLinearVelocity.x +=
			impulse.x / mass;

		part->assemblyLinearVelocity.y +=
			impulse.y / mass;

		part->assemblyLinearVelocity.z +=
			impulse.z / mass;

		firePropertyChanged(
			L,
			part,
			"AssemblyLinearVelocity"
		);

		return 0;
	}

	struct RaycastFilter
	{
		bool includeOnly = false;
		bool respectCanCollide = false;

		std::vector<
			InstanceObject*
		> includeRoots;

		std::vector<
			InstanceObject*
		> excludeRoots;
	};

	void appendFilterInstances(
		lua_State* L,
		int tableIndex,
		std::vector<InstanceObject*>& output
	)
	{
		const int absoluteIndex =
			lua_absindex(L, tableIndex);

		const int count =
			lua_objlen(
				L,
				absoluteIndex
			);

		for (
			int index = 1;
			index <= count;
			++index
		)
		{
			lua_rawgeti(
				L,
				absoluteIndex,
				index
			);

			if (
				RobloxObjectModel::isInstance(
					L,
					-1
				)
			)
			{
				output.push_back(
					checkInstance(L, -1)
				);
			}

			lua_pop(L, 1);
		}
	}

	RaycastFilter readRaycastFilter(
		lua_State* L,
		int index
	)
	{
		RaycastFilter filter;

		if (lua_isnoneornil(L, index))
			return filter;

		luaL_checktype(
			L,
			index,
			LUA_TTABLE
		);

		if (
			!RobloxTypes::hasType(
				L,
				index,
				"RaycastParams"
			)
		)
		{
			luaL_typeerror(
				L,
				index,
				"RaycastParams"
			);
		}

		const int absoluteIndex =
			lua_absindex(L, index);

		lua_getfield(
			L,
			absoluteIndex,
			"RespectCanCollide"
		);

		filter.respectCanCollide =
			lua_toboolean(L, -1) != 0;

		lua_pop(L, 1);

		lua_getfield(
			L,
			absoluteIndex,
			"ExcludeInstances"
		);

		if (lua_istable(L, -1))
		{
			appendFilterInstances(
				L,
				-1,
				filter.excludeRoots
			);
		}

		lua_pop(L, 1);

		lua_getfield(
			L,
			absoluteIndex,
			"IncludeInstances"
		);

		if (lua_istable(L, -1))
		{
			filter.includeOnly = true;

			appendFilterInstances(
				L,
				-1,
				filter.includeRoots
			);
		}

		lua_pop(L, 1);

		lua_getfield(
			L,
			absoluteIndex,
			"FilterDescendantsInstances"
		);

		std::vector<InstanceObject*> legacy;

		if (lua_istable(L, -1))
		{
			appendFilterInstances(
				L,
				-1,
				legacy
			);
		}

		lua_pop(L, 1);

		lua_getfield(
			L,
			absoluteIndex,
			"FilterType"
		);

		if (
			RobloxTypes::isEnumItem(
				L,
				-1
			)
		)
		{
			const std::string type =
				RobloxTypes::checkEnumItem(
					L,
					-1,
					"RaycastFilterType"
				);

			if (type == "Include")
			{
				filter.includeOnly = true;

				filter.includeRoots.insert(
					filter.includeRoots.end(),
					legacy.begin(),
					legacy.end()
				);
			}
			else
			{
				filter.excludeRoots.insert(
					filter.excludeRoots.end(),
					legacy.begin(),
					legacy.end()
				);
			}
		}

		lua_pop(L, 1);

		return filter;
	}

	bool matchesAnyRoot(
		InstanceObject* object,
		const std::vector<
			InstanceObject*
		>& roots
	)
	{
		for (
			InstanceObject* root :
			roots
		)
		{
			if (
				root &&
				isSelfOrDescendantOf(
					object,
					root
				)
			)
			{
				return true;
			}
		}

		return false;
	}

	bool rayCandidateAllowed(
		InstanceObject* object,
		const RaycastFilter& filter
	)
	{
		if (
			matchesAnyRoot(
				object,
				filter.excludeRoots
			)
		)
		{
			return false;
		}

		if (
			filter.includeOnly &&
			!matchesAnyRoot(
				object,
				filter.includeRoots
			)
		)
		{
			return false;
		}

		if (
			filter.respectCanCollide &&
			!object->canCollide
		)
		{
			return false;
		}

		return true;
	}

	bool intersectAxisAlignedBox(
		const RobloxTypes::Vector3Value& origin,
		const RobloxTypes::Vector3Value& direction,
		const InstanceObject* part,
		double& hitT,
		RobloxTypes::Vector3Value& normal
	)
	{
		const double halfX =
			part->size.x * 0.5;

		const double halfY =
			part->size.y * 0.5;

		const double halfZ =
			part->size.z * 0.5;

		const double minimum[3] = {
			part->position.x - halfX,
			part->position.y - halfY,
			part->position.z - halfZ
		};

		const double maximum[3] = {
			part->position.x + halfX,
			part->position.y + halfY,
			part->position.z + halfZ
		};

		const double o[3] = {
			origin.x,
			origin.y,
			origin.z
		};

		const double d[3] = {
			direction.x,
			direction.y,
			direction.z
		};

		double tMin = 0.0;
		double tMax = 1.0;
		int hitAxis = -1;
		double hitSign = 0.0;

		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::abs(d[axis]) < 1e-12)
			{
				if (
					o[axis] < minimum[axis] ||
					o[axis] > maximum[axis]
				)
				{
					return false;
				}

				continue;
			}

			double t1 =
				(
					minimum[axis] -
					o[axis]
				) / d[axis];

			double t2 =
				(
					maximum[axis] -
					o[axis]
				) / d[axis];

			double enteringSign =
				d[axis] > 0.0
					? -1.0
					: 1.0;

			if (t1 > t2)
			{
				std::swap(t1, t2);
			}

			if (t1 > tMin)
			{
				tMin = t1;
				hitAxis = axis;
				hitSign =
					enteringSign;
			}

			tMax =
				std::min(
					tMax,
					t2
				);

			if (tMin > tMax)
				return false;
		}

		if (
			tMin < 0.0 ||
			tMin > 1.0
		)
		{
			return false;
		}

		hitT = tMin;
		normal = {};

		if (hitAxis == 0)
			normal.x = hitSign;
		else if (hitAxis == 1)
			normal.y = hitSign;
		else if (hitAxis == 2)
			normal.z = hitSign;

		return true;
	}

	void pushRaycastResult(
		lua_State* L,
		InstanceObject* hit,
		const RobloxTypes::Vector3Value& position,
		const RobloxTypes::Vector3Value& normal,
		double distance
	)
	{
		lua_createtable(L, 0, 7);

		lua_pushstring(L, "RaycastResult");
		lua_setfield(L, -2, "__type");

		pushInstance(L, hit);
		lua_setfield(L, -2, "Instance");

		RobloxTypes::pushVector3(
			L,
			position
		);
		lua_setfield(L, -2, "Position");

		RobloxTypes::pushVector3(
			L,
			normal
		);
		lua_setfield(L, -2, "Normal");

		lua_pushnumber(L, distance);
		lua_setfield(L, -2, "Distance");

		RobloxTypes::pushEnumItem(
			L,
			"Material",
			hit->material.c_str()
		);
		lua_setfield(L, -2, "Material");
	}

	int workspaceRaycast(lua_State* L)
	{
		InstanceObject* workspace =
			checkInstance(L, 1);

		if (
			workspace->className !=
			"Workspace"
		)
		{
			luaL_error(
				L,
				"Raycast is only available on Workspace"
			);
		}

		const auto origin =
			RobloxTypes::checkVector3(
				L,
				2
			);

		const auto direction =
			RobloxTypes::checkVector3(
				L,
				3
			);

		const double directionLength =
			std::sqrt(
				direction.x * direction.x +
				direction.y * direction.y +
				direction.z * direction.z
			);

		if (directionLength == 0.0)
		{
			lua_pushnil(L);
			return 1;
		}

		const RaycastFilter filter =
			readRaycastFilter(L, 4);

		RuntimeContext& runtime =
			context(L);

		InstanceObject* closest = nullptr;

		double closestT =
			std::numeric_limits<double>::infinity();

		RobloxTypes::Vector3Value closestNormal;

		for (
			const auto& owned :
			runtime.objects
		)
		{
			InstanceObject* candidate =
				owned.get();

			if (
				candidate->destroyed ||
				!isBasePartClass(
					candidate->className
				) ||
				!isSelfOrDescendantOf(
					candidate,
					runtime.workspace
				) ||
				!rayCandidateAllowed(
					candidate,
					filter
				)
			)
			{
				continue;
			}

			double hitT = 0.0;
			RobloxTypes::Vector3Value normal;

			if (
				intersectAxisAlignedBox(
					origin,
					direction,
					candidate,
					hitT,
					normal
				) &&
				hitT < closestT
			)
			{
				closest = candidate;
				closestT = hitT;
				closestNormal = normal;
			}
		}

		if (!closest)
		{
			lua_pushnil(L);
			return 1;
		}

		const RobloxTypes::Vector3Value position{
			origin.x +
				direction.x * closestT,
			origin.y +
				direction.y * closestT,
			origin.z +
				direction.z * closestT
		};

		pushRaycastResult(
			L,
			closest,
			position,
			closestNormal,
			directionLength * closestT
		);

		return 1;
	}

	int instanceIndex(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string key =
			luaL_checkstring(L, 2);

		if (key == "Name")
		{
			lua_pushlstring(
				L,
				object->name.data(),
				object->name.size()
			);
			return 1;
		}

		if (key == "ClassName")
		{
			lua_pushlstring(
				L,
				object->className.data(),
				object->className.size()
			);
			return 1;
		}

		if (key == "Parent")
		{
			pushInstance(
				L,
				object->parent
			);
			return 1;
		}

		if (key == "Archivable")
		{
			lua_pushboolean(
				L,
				object->archivable
			);
			return 1;
		}

		if (
			object->className == "Player" &&
			key == "UserId"
		)
		{
			// Roblox exposes Player.UserId to Luau as number.
			// Keep integer storage natively, but push a Luau number
			// so normal Roblox operations such as string concatenation work.
			lua_pushnumber(
				L,
				static_cast<double>(
					object->userId
				)
			);
			return 1;
		}

		RuntimeContext& runtime =
			context(L);

		if (
			object == runtime.players &&
			key == "LocalPlayer"
		)
		{
			pushInstance(
				L,
				runtime.localPlayer
			);
			return 1;
		}

		if (
			object == runtime.workspace &&
			key == "CurrentCamera"
		)
		{
			pushInstance(
				L,
				runtime.currentCamera
			);
			return 1;
		}

		if (
			object == runtime.workspace &&
			key == "Gravity"
		)
		{
			lua_pushnumber(
				L,
				runtime.gravity
			);
			return 1;
		}

		if (
			object->className == "Camera" &&
			key == "CameraSubject"
		)
		{
			pushInstance(
				L,
				object->cameraSubject
			);
			return 1;
		}

		if (isBasePartClass(object->className))
		{
			if (key == "Anchored")
			{
				lua_pushboolean(
					L,
					object->anchored
				);
				return 1;
			}

			if (key == "CanCollide")
			{
				lua_pushboolean(
					L,
					object->canCollide
				);
				return 1;
			}

			if (key == "Transparency")
			{
				lua_pushnumber(
					L,
					object->transparency
				);
				return 1;
			}

			if (key == "Position")
			{
				RobloxTypes::pushVector3(
					L,
					object->position
				);
				return 1;
			}

			if (key == "Size")
			{
				RobloxTypes::pushVector3(
					L,
					object->size
				);
				return 1;
			}

			if (
				key ==
				"AssemblyLinearVelocity"
			)
			{
				RobloxTypes::pushVector3(
					L,
					object->assemblyLinearVelocity
				);
				return 1;
			}

			if (key == "AssemblyMass")
			{
				lua_pushnumber(
					L,
					assemblyMass(object)
				);
				return 1;
			}

			if (key == "Color")
			{
				RobloxTypes::pushColor3(
					L,
					object->color
				);
				return 1;
			}

			if (key == "Material")
			{
				RobloxTypes::pushEnumItem(
					L,
					"Material",
					object->material.c_str()
				);
				return 1;
			}

			if (
				key ==
				"SetNetworkOwner"
			)
			{
				lua_pushcfunction(
					L,
					basePartSetNetworkOwner,
					"BasePart.SetNetworkOwner"
				);
				return 1;
			}

			if (
				key ==
				"SetNetworkOwnershipAuto"
			)
			{
				lua_pushcfunction(
					L,
					basePartSetNetworkOwnershipAuto,
					"BasePart.SetNetworkOwnershipAuto"
				);
				return 1;
			}

			if (key == "ApplyImpulse")
			{
				lua_pushcfunction(
					L,
					basePartApplyImpulse,
					"BasePart.ApplyImpulse"
				);
				return 1;
			}
		}

		if (
			object->className == "RemoteEvent"
		)
		{
			if (key == "FireServer")
			{
				lua_pushcfunction(
					L,
					remoteEventFireServer,
					"RemoteEvent.FireServer"
				);
				return 1;
			}

			if (key == "FireClient")
			{
				lua_pushcfunction(
					L,
					remoteEventFireClient,
					"RemoteEvent.FireClient"
				);
				return 1;
			}

			if (key == "FireAllClients")
			{
				lua_pushcfunction(
					L,
					remoteEventFireAllClients,
					"RemoteEvent.FireAllClients"
				);
				return 1;
			}
		}

		if (
			object == runtime.players &&
			key == "GetPlayers"
		)
		{
			lua_pushcfunction(
				L,
				playersGetPlayers,
				"Players.GetPlayers"
			);
			return 1;
		}

		if (
			object == runtime.workspace &&
			key == "Raycast"
		)
		{
			lua_pushcfunction(
				L,
				workspaceRaycast,
				"Workspace.Raycast"
			);
			return 1;
		}

		if (key == "Destroy")
		{
			lua_pushcfunction(
				L,
				instanceDestroy,
				"Instance.Destroy"
			);
			return 1;
		}

		if (key == "GetChildren")
		{
			lua_pushcfunction(
				L,
				instanceGetChildren,
				"Instance.GetChildren"
			);
			return 1;
		}

		if (key == "GetDescendants")
		{
			lua_pushcfunction(
				L,
				instanceGetDescendants,
				"Instance.GetDescendants"
			);
			return 1;
		}

		if (key == "FindFirstChild")
		{
			lua_pushcfunction(
				L,
				instanceFindFirstChild,
				"Instance.FindFirstChild"
			);
			return 1;
		}

		if (key == "WaitForChild")
		{
			lua_pushcfunction(
				L,
				instanceWaitForChild,
				"Instance.WaitForChild"
			);
			return 1;
		}

		if (key == "FindFirstChildOfClass")
		{
			lua_pushcfunction(
				L,
				instanceFindFirstChildOfClass,
				"Instance.FindFirstChildOfClass"
			);
			return 1;
		}

		if (
			key ==
			"FindFirstChildWhichIsA"
		)
		{
			lua_pushcfunction(
				L,
				instanceFindFirstChildWhichIsA,
				"Instance.FindFirstChildWhichIsA"
			);
			return 1;
		}

		if (key == "IsA")
		{
			lua_pushcfunction(
				L,
				instanceIsA,
				"Instance.IsA"
			);
			return 1;
		}

		if (key == "IsDescendantOf")
		{
			lua_pushcfunction(
				L,
				instanceIsDescendantOf,
				"Instance.IsDescendantOf"
			);
			return 1;
		}

		if (key == "IsAncestorOf")
		{
			lua_pushcfunction(
				L,
				instanceIsAncestorOf,
				"Instance.IsAncestorOf"
			);
			return 1;
		}

		if (key == "GetFullName")
		{
			lua_pushcfunction(
				L,
				instanceGetFullName,
				"Instance.GetFullName"
			);
			return 1;
		}

		if (key == "ClearAllChildren")
		{
			lua_pushcfunction(
				L,
				instanceClearAllChildren,
				"Instance.ClearAllChildren"
			);
			return 1;
		}

		if (
			key ==
			"GetPropertyChangedSignal"
		)
		{
			lua_pushcfunction(
				L,
				instanceGetPropertyChangedSignal,
				"Instance.GetPropertyChangedSignal"
			);
			return 1;
		}

		if (key == "SetAttribute")
		{
			lua_pushcfunction(
				L,
				instanceSetAttribute,
				"Instance.SetAttribute"
			);
			return 1;
		}

		if (key == "GetAttribute")
		{
			lua_pushcfunction(
				L,
				instanceGetAttribute,
				"Instance.GetAttribute"
			);
			return 1;
		}

		if (
			key ==
			"GetAttributeChangedSignal"
		)
		{
			lua_pushcfunction(
				L,
				instanceGetAttributeChangedSignal,
				"Instance.GetAttributeChangedSignal"
			);
			return 1;
		}

		if (
			object->className == "DataModel" &&
			key == "GetService"
		)
		{
			lua_pushcfunction(
				L,
				dataModelGetService,
				"DataModel.GetService"
			);
			return 1;
		}

		const bool commonSignal =
			key == "ChildAdded" ||
			key == "ChildRemoved" ||
			key == "AncestryChanged" ||
			key == "Destroying" ||
			key == "Changed" ||
			key == "AttributeChanged";

		const bool playerSignal =
			object == runtime.players &&
			(
				key == "PlayerAdded" ||
				key == "PlayerRemoving"
			);

		const bool remoteSignal =
			object->className ==
				"RemoteEvent" &&
			(
				key == "OnServerEvent" ||
				key == "OnClientEvent"
			);

		const bool inputSignal =
			object ==
				runtime.userInputService &&
			(
				key == "InputBegan" ||
				key == "InputEnded" ||
				key == "InputChanged"
			);

		const bool runSignal =
			object ==
				runtime.runService &&
			(
				key == "Heartbeat" ||
				key == "PreSimulation" ||
				key == "PostSimulation" ||
				key == "PreRender" ||
				key == "RenderStepped"
			);

		if (
			commonSignal ||
			playerSignal ||
			remoteSignal ||
			inputSignal ||
			runSignal
		)
		{
			pushSignal(
				L,
				object,
				key
			);

			return 1;
		}

		if (
			InstanceObject* child =
				findFirstChild(
					object,
					key,
					false
				)
		)
		{
			pushInstance(L, child);
			return 1;
		}

		luaL_error(
			L,
			"%s is not a valid member of %s \"%s\"",
			key.c_str(),
			object->className.c_str(),
			object->name.c_str()
		);

		return 0;
	}

	int instanceNewIndex(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		const std::string key =
			luaL_checkstring(L, 2);

		if (object->destroyed)
		{
			luaL_error(
				L,
				"Cannot modify destroyed Instance %s",
				object->name.c_str()
			);
		}

		if (key == "Name")
		{
			const std::string value =
				luaL_checkstring(L, 3);

			if (object->name != value)
			{
				object->name = value;

				firePropertyChanged(
					L,
					object,
					"Name"
				);
			}

			return 0;
		}

		if (key == "Parent")
		{
			InstanceObject* parent =
				nullptr;

			if (!lua_isnil(L, 3))
			{
				parent =
					checkInstance(L, 3);
			}

			setParent(
				L,
				object,
				parent
			);

			return 0;
		}

		if (key == "Archivable")
		{
			const bool value =
				lua_toboolean(L, 3) != 0;

			if (
				object->archivable !=
				value
			)
			{
				object->archivable =
					value;

				firePropertyChanged(
					L,
					object,
					"Archivable"
				);
			}

			return 0;
		}

		if (key == "ClassName")
		{
			luaL_error(
				L,
				"ClassName is a read-only property"
			);
		}

		RuntimeContext& runtime =
			context(L);

		if (
			object == runtime.workspace &&
			key == "Gravity"
		)
		{
			runtime.gravity =
				luaL_checknumber(L, 3);

			firePropertyChanged(
				L,
				object,
				"Gravity"
			);

			return 0;
		}

		if (
			object->className == "Camera" &&
			key == "CameraSubject"
		)
		{
			InstanceObject* subject =
				nullptr;

			if (!lua_isnil(L, 3))
			{
				subject =
					checkInstance(L, 3);
			}

			object->cameraSubject =
				subject;

			firePropertyChanged(
				L,
				object,
				"CameraSubject"
			);

			return 0;
		}

		if (isBasePartClass(object->className))
		{
			if (key == "Anchored")
			{
				const bool value =
					lua_toboolean(
						L,
						3
					) != 0;

				if (
					object->anchored !=
					value
				)
				{
					object->anchored =
						value;

					firePropertyChanged(
						L,
						object,
						"Anchored"
					);
				}

				return 0;
			}

			if (key == "CanCollide")
			{
				const bool value =
					lua_toboolean(
						L,
						3
					) != 0;

				if (
					object->canCollide !=
					value
				)
				{
					object->canCollide =
						value;

					firePropertyChanged(
						L,
						object,
						"CanCollide"
					);
				}

				return 0;
			}

			if (key == "Transparency")
			{
				const double value =
					luaL_checknumber(
						L,
						3
					);

				if (
					value < 0.0 ||
					value > 1.0
				)
				{
					luaL_error(
						L,
						"Transparency must be between 0 and 1"
					);
				}

				object->transparency =
					value;

				firePropertyChanged(
					L,
					object,
					"Transparency"
				);

				return 0;
			}

			if (key == "Position")
			{
				object->position =
					RobloxTypes::checkVector3(
						L,
						3
					);

				firePropertyChanged(
					L,
					object,
					"Position"
				);

				return 0;
			}

			if (key == "Size")
			{
				const auto value =
					RobloxTypes::checkVector3(
						L,
						3
					);

				if (
					value.x <= 0.0 ||
					value.y <= 0.0 ||
					value.z <= 0.0
				)
				{
					luaL_error(
						L,
						"Part Size components must be greater than zero"
					);
				}

				object->size = value;

				firePropertyChanged(
					L,
					object,
					"Size"
				);

				return 0;
			}

			if (
				key ==
				"AssemblyLinearVelocity"
			)
			{
				object->assemblyLinearVelocity =
					RobloxTypes::checkVector3(
						L,
						3
					);

				firePropertyChanged(
					L,
					object,
					"AssemblyLinearVelocity"
				);

				return 0;
			}

			if (key == "AssemblyMass")
			{
				luaL_error(
					L,
					"AssemblyMass is a read-only property"
				);
			}

			if (key == "Color")
			{
				object->color =
					RobloxTypes::checkColor3(
						L,
						3
					);

				firePropertyChanged(
					L,
					object,
					"Color"
				);

				return 0;
			}

			if (key == "Material")
			{
				object->material =
					RobloxTypes::checkEnumItem(
						L,
						3,
						"Material"
					);

				firePropertyChanged(
					L,
					object,
					"Material"
				);

				return 0;
			}
		}

		luaL_error(
			L,
			"%s is not a writable property of %s",
			key.c_str(),
			object->className.c_str()
		);

		return 0;
	}

	int instanceToString(lua_State* L)
	{
		InstanceObject* object =
			checkInstance(L, 1);

		lua_pushlstring(
			L,
			object->name.data(),
			object->name.size()
		);

		return 1;
	}

	int instanceNew(lua_State* L)
	{
		const int argumentCount =
			lua_gettop(L);

		const std::string className =
			luaL_checkstring(L, 1);

		RuntimeContext& runtime =
			context(L);

		InstanceObject* parent = nullptr;

		if (
			argumentCount >= 2 &&
			!lua_isnil(L, 2)
		)
		{
			parent =
				checkInstance(L, 2);
		}

		InstanceObject* object =
			createObject(
				runtime,
				className
			);

		if (parent)
		{
			setParent(
				L,
				object,
				parent
			);
		}

		pushInstance(L, object);
		return 1;
	}

	SignalUserdata* checkSignal(
		lua_State* L,
		int index
	)
	{
		return static_cast<
			SignalUserdata*
		>(
			luaL_checkudata(
				L,
				index,
				SIGNAL_METATABLE
			)
		);
	}

	ConnectionUserdata* checkConnection(
		lua_State* L,
		int index
	)
	{
		return static_cast<
			ConnectionUserdata*
		>(
			luaL_checkudata(
				L,
				index,
				CONNECTION_METATABLE
			)
		);
	}

	void pushConnection(
		lua_State* L,
		ConnectionState* connection
	)
	{
		auto* userdata =
			static_cast<
				ConnectionUserdata*
			>(
				lua_newuserdata(
					L,
					sizeof(
						ConnectionUserdata
					)
				)
			);

		userdata->connection =
			connection;

		luaL_getmetatable(
			L,
			CONNECTION_METATABLE
		);

		lua_setmetatable(L, -2);
	}

	int signalConnectInternal(
		lua_State* L,
		bool once
	)
	{
		SignalUserdata* signal =
			checkSignal(L, 1);

		luaL_checktype(
			L,
			2,
			LUA_TFUNCTION
		);

		if (
			!signal->object ||
			signal->object->destroyed
		)
		{
			luaL_error(
				L,
				"Cannot connect to destroyed Instance signal"
			);
		}

		auto connection =
			std::make_unique<
				ConnectionState
			>();

		connection->callbackRef =
			lua_ref(L, 2);

		connection->connected = true;
		connection->once = once;

		ConnectionState* raw =
			connection.get();

		signal->object
			->ownedConnections
			.push_back(
				std::move(connection)
			);

		signal->object
			->signals[
				signal->eventName
			]
			.push_back(raw);

		pushConnection(L, raw);
		return 1;
	}

	int signalConnect(lua_State* L)
	{
		return signalConnectInternal(
			L,
			false
		);
	}

	int signalOnce(lua_State* L)
	{
		return signalConnectInternal(
			L,
			true
		);
	}

	int signalWaitResume(lua_State* L)
	{
		lua_State* waiter =
			static_cast<lua_State*>(
				lua_tolightuserdata(
					L,
					lua_upvalueindex(1)
				)
			);

		if (
			!waiter ||
			!RobloxScheduler::isManaged(
				L,
				waiter
			)
		)
		{
			return 0;
		}

		const int argumentCount =
			lua_gettop(L);

		for (
			int index = 1;
			index <= argumentCount;
			++index
		)
		{
			lua_xpush(
				L,
				waiter,
				index
			);
		}

		RobloxScheduler::resume(
			L,
			waiter,
			argumentCount
		);

		return 0;
	}

	int signalWait(lua_State* L)
	{
		SignalUserdata* signal =
			checkSignal(L, 1);

		if (!lua_isyieldable(L))
		{
			luaL_error(
				L,
				"RBXScriptSignal:Wait cannot yield from this context"
			);
		}

		if (
			!signal->object ||
			signal->object->destroyed
		)
		{
			luaL_error(
				L,
				"Cannot wait on a destroyed Instance signal"
			);
		}

		RobloxScheduler::suspend(L);

		lua_pushlightuserdata(
			L,
			L
		);

		lua_pushcclosure(
			L,
			signalWaitResume,
			"RBXScriptSignal.WaitResume",
			1
		);

		auto connection =
			std::make_unique<
				ConnectionState
			>();

		connection->callbackRef =
			lua_ref(L, -1);

		lua_pop(L, 1);

		connection->connected = true;
		connection->once = true;

		ConnectionState* raw =
			connection.get();

		signal->object
			->ownedConnections
			.push_back(
				std::move(connection)
			);

		signal->object
			->signals[
				signal->eventName
			]
			.push_back(raw);

		return lua_yield(L, 0);
	}

	int signalIndex(lua_State* L)
	{
		checkSignal(L, 1);

		const std::string key =
			luaL_checkstring(L, 2);

		if (key == "Connect")
		{
			lua_pushcfunction(
				L,
				signalConnect,
				"RBXScriptSignal.Connect"
			);

			return 1;
		}

		if (key == "Once")
		{
			lua_pushcfunction(
				L,
				signalOnce,
				"RBXScriptSignal.Once"
			);

			return 1;
		}

		if (key == "Wait")
		{
			lua_pushcfunction(
				L,
				signalWait,
				"RBXScriptSignal.Wait"
			);

			return 1;
		}

		luaL_error(
			L,
			"%s is not a valid member of RBXScriptSignal",
			key.c_str()
		);

		return 0;
	}

	int signalToString(lua_State* L)
	{
		SignalUserdata* signal =
			checkSignal(L, 1);

		lua_pushfstring(
			L,
			"Signal %s",
			signal->eventName.c_str()
		);

		return 1;
	}

	int connectionDisconnect(lua_State* L)
	{
		ConnectionUserdata* userdata =
			checkConnection(L, 1);

		disconnect(
			L,
			userdata->connection
		);

		return 0;
	}

	int connectionIndex(lua_State* L)
	{
		ConnectionUserdata* userdata =
			checkConnection(L, 1);

		const std::string key =
			luaL_checkstring(L, 2);

		if (key == "Connected")
		{
			lua_pushboolean(
				L,
				userdata->connection &&
				userdata->connection
					->connected
			);

			return 1;
		}

		if (key == "Disconnect")
		{
			lua_pushcfunction(
				L,
				connectionDisconnect,
				"RBXScriptConnection.Disconnect"
			);

			return 1;
		}

		luaL_error(
			L,
			"%s is not a valid member of RBXScriptConnection",
			key.c_str()
		);

		return 0;
	}

	int connectionToString(lua_State* L)
	{
		checkConnection(L, 1);

		lua_pushstring(
			L,
			"Connection"
		);

		return 1;
	}

	void installMetatables(lua_State* L)
	{
		luaL_newmetatable(
			L,
			INSTANCE_METATABLE
		);

		lua_pushstring(L, "Instance");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(
			L,
			instanceIndex,
			"Instance.__index"
		);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(
			L,
			instanceNewIndex,
			"Instance.__newindex"
		);
		lua_setfield(
			L,
			-2,
			"__newindex"
		);

		lua_pushcfunction(
			L,
			instanceToString,
			"Instance.__tostring"
		);
		lua_setfield(
			L,
			-2,
			"__tostring"
		);

		lua_pop(L, 1);

		luaL_newmetatable(
			L,
			SIGNAL_METATABLE
		);

		lua_pushstring(
			L,
			"RBXScriptSignal"
		);
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(
			L,
			signalIndex,
			"RBXScriptSignal.__index"
		);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(
			L,
			signalToString,
			"RBXScriptSignal.__tostring"
		);
		lua_setfield(
			L,
			-2,
			"__tostring"
		);

		lua_pop(L, 1);

		luaL_newmetatable(
			L,
			CONNECTION_METATABLE
		);

		lua_pushstring(
			L,
			"RBXScriptConnection"
		);
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(
			L,
			connectionIndex,
			"RBXScriptConnection.__index"
		);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(
			L,
			connectionToString,
			"RBXScriptConnection.__tostring"
		);
		lua_setfield(
			L,
			-2,
			"__tostring"
		);

		lua_pop(L, 1);
	}

	InstanceObject* addService(
		RuntimeContext& runtime,
		const std::string& className,
		const std::string& name
	)
	{
		InstanceObject* service =
			createObject(
				runtime,
				className,
				name
			);

		service->parent =
			runtime.dataModel;

		runtime.dataModel
			->children
			.push_back(service);

		runtime.services[name] =
			service;

		if (name == "Workspace")
			runtime.workspace = service;

		if (name == "Players")
			runtime.players = service;

		if (name == "RunService")
			runtime.runService = service;

		if (
			name ==
			"UserInputService"
		)
		{
			runtime.userInputService =
				service;
		}

		return service;
	}

	void pushInputObject(
		lua_State* L,
		const std::string& keyCodeName
	)
	{
		lua_createtable(L, 0, 3);

		lua_pushstring(
			L,
			"InputObject"
		);
		lua_setfield(L, -2, "__type");

		RobloxTypes::pushEnumItem(
			L,
			"KeyCode",
			keyCodeName.c_str()
		);
		lua_setfield(L, -2, "KeyCode");
	}

	void processChildWaiters(
		lua_State* L,
		RuntimeContext& runtime
	)
	{
		if (runtime.childWaiters.empty())
			return;

		std::vector<ChildWaiter> pending;
		pending.swap(
			runtime.childWaiters
		);

		for (ChildWaiter& waiter : pending)
		{
			if (!waiter.thread)
				continue;

			InstanceObject* child =
				waiter.parent
					? findFirstChild(
						waiter.parent,
						waiter.childName,
						false
					)
					: nullptr;

			if (child)
			{
				pushInstance(
					waiter.thread,
					child
				);

				RobloxScheduler::resume(
					L,
					waiter.thread,
					1
				);

				continue;
			}

			if (
				waiter.hasDeadline &&
				runtime.time >=
					waiter.deadline
			)
			{
				lua_pushnil(
					waiter.thread
				);

				RobloxScheduler::resume(
					L,
					waiter.thread,
					1
				);

				continue;
			}

			runtime.childWaiters.push_back(
				std::move(waiter)
			);
		}
	}
}

void RobloxObjectModel::install(lua_State* L)
{
	lua_State* mainThread =
		lua_mainthread(L);

	if (
		contexts.find(mainThread) !=
		contexts.end()
	)
	{
		return;
	}

	installMetatables(L);

	auto runtime =
		std::make_unique<
			RuntimeContext
		>();

	runtime->dataModel =
		createObject(
			*runtime,
			"DataModel",
			"game"
		);

	addService(
		*runtime,
		"Workspace",
		"Workspace"
	);

	addService(
		*runtime,
		"RunService",
		"RunService"
	);

	addService(
		*runtime,
		"Players",
		"Players"
	);

	addService(
		*runtime,
		"ReplicatedStorage",
		"ReplicatedStorage"
	);

	addService(
		*runtime,
		"ServerScriptService",
		"ServerScriptService"
	);

	addService(
		*runtime,
		"UserInputService",
		"UserInputService"
	);

	addService(
		*runtime,
		"Lighting",
		"Lighting"
	);

	addService(
		*runtime,
		"SoundService",
		"SoundService"
	);

	addService(
		*runtime,
		"TweenService",
		"TweenService"
	);

	addService(
		*runtime,
		"HttpService",
		"HttpService"
	);

	addService(
		*runtime,
		"CollectionService",
		"CollectionService"
	);

	addService(
		*runtime,
		"PhysicsService",
		"PhysicsService"
	);

	addService(
		*runtime,
		"Debris",
		"Debris"
	);

	runtime->localPlayer =
		createObject(
			*runtime,
			"Player",
			"LocalPlayer"
		);

	runtime->localPlayer->userId = 1;

	runtime->localPlayer->parent =
		runtime->players;

	runtime->players
		->children
		.push_back(
			runtime->localPlayer
		);

	runtime->currentCamera =
		createObject(
			*runtime,
			"Camera",
			"Camera"
		);

	runtime->currentCamera->parent =
		runtime->workspace;

	runtime->workspace
		->children
		.push_back(
			runtime->currentCamera
		);

	RuntimeContext* rawRuntime =
		runtime.get();

	contexts.emplace(
		mainThread,
		std::move(runtime)
	);

	lua_newtable(L);

	lua_pushcfunction(
		L,
		instanceNew,
		"Instance.new"
	);

	lua_setfield(L, -2, "new");
	lua_setglobal(L, "Instance");

	pushInstance(
		L,
		rawRuntime->dataModel
	);
	lua_setglobal(L, "game");

	pushInstance(
		L,
		rawRuntime->workspace
	);
	lua_setglobal(L, "workspace");
}

void RobloxObjectModel::shutdown(
	lua_State* L
)
{
	lua_State* mainThread =
		lua_mainthread(L);

	auto iterator =
		contexts.find(mainThread);

	if (iterator == contexts.end())
		return;

	RuntimeContext& runtime =
		*iterator->second;

	for (
		auto& object :
		runtime.objects
	)
	{
		for (
			auto& connection :
			object->ownedConnections
		)
		{
			disconnect(
				L,
				connection.get()
			);
		}
	}

	for (
		auto& object :
		runtime.objects
	)
	{
		if (
			object->luaRef !=
			LUA_NOREF
		)
		{
			object->luaRef =
				lua_unref(
					L,
					object->luaRef
				);
		}
	}

	contexts.erase(iterator);
}

void RobloxObjectModel::step(
	lua_State* L,
	double deltaTime
)
{
	if (deltaTime < 0.0)
		deltaTime = 0.0;

	RuntimeContext& runtime =
		context(L);

	runtime.time += deltaTime;

	processChildWaiters(
		L,
		runtime
	);

	fireEventNumber(
		L,
		runtime.runService,
		"PreSimulation",
		deltaTime
	);

	for (
		const auto& owned :
		runtime.objects
	)
	{
		InstanceObject* object =
			owned.get();

		if (
			object->destroyed ||
			!isBasePartClass(
				object->className
			) ||
			object->anchored ||
			!isSelfOrDescendantOf(
				object,
				runtime.workspace
			)
		)
		{
			continue;
		}

		object
			->assemblyLinearVelocity
			.y -=
				runtime.gravity *
				deltaTime;

		object->position.x +=
			object
				->assemblyLinearVelocity
				.x *
			deltaTime;

		object->position.y +=
			object
				->assemblyLinearVelocity
				.y *
			deltaTime;

		object->position.z +=
			object
				->assemblyLinearVelocity
				.z *
			deltaTime;
	}

	fireEventNumber(
		L,
		runtime.runService,
		"PostSimulation",
		deltaTime
	);

	fireEventNumber(
		L,
		runtime.runService,
		"Heartbeat",
		deltaTime
	);

	fireEventNumber(
		L,
		runtime.runService,
		"PreRender",
		deltaTime
	);

	fireEventNumber(
		L,
		runtime.runService,
		"RenderStepped",
		deltaTime
	);
}

void RobloxObjectModel::emitKey(
	lua_State* L,
	const std::string& keyCodeName,
	bool pressed
)
{
	RuntimeContext& runtime =
		context(L);

	const std::string eventName =
		pressed
			? "InputBegan"
			: "InputEnded";

	fireEvent(
		L,
		runtime.userInputService,
		eventName,
		2,
		[
			L,
			&keyCodeName
		]()
		{
			pushInputObject(
				L,
				keyCodeName
			);

			lua_pushboolean(L, 0);
		}
	);
}

bool RobloxObjectModel::isInstance(
	lua_State* L,
	int index
)
{
	if (!lua_isuserdata(L, index))
		return false;

	if (!lua_getmetatable(L, index))
		return false;

	luaL_getmetatable(
		L,
		INSTANCE_METATABLE
	);

	const bool matches =
		lua_rawequal(
			L,
			-1,
			-2
		) != 0;

	lua_pop(L, 2);
	return matches;
}


std::vector<RobloxObjectModel::RenderPartSnapshot>
RobloxObjectModel::getRenderParts(
	lua_State* L
)
{
	RuntimeContext& runtime =
		context(L);

	std::vector<
		RenderPartSnapshot
	> result;

	for (
		const auto& owned :
		runtime.objects
	)
	{
		InstanceObject* object =
			owned.get();

		if (
			object->destroyed ||
			!isBasePartClass(
				object->className
			) ||
			!isSelfOrDescendantOf(
				object,
				runtime.workspace
			)
		)
		{
			continue;
		}

		RenderPartSnapshot part;

		part.name = object->name;

		part.positionX =
			static_cast<float>(
				object->position.x
			);

		part.positionY =
			static_cast<float>(
				object->position.y
			);

		part.positionZ =
			static_cast<float>(
				object->position.z
			);

		part.sizeX =
			static_cast<float>(
				object->size.x
			);

		part.sizeY =
			static_cast<float>(
				object->size.y
			);

		part.sizeZ =
			static_cast<float>(
				object->size.z
			);

		part.colorR =
			static_cast<float>(
				object->color.r
			);

		part.colorG =
			static_cast<float>(
				object->color.g
			);

		part.colorB =
			static_cast<float>(
				object->color.b
			);

		part.transparency =
			static_cast<float>(
				object->transparency
			);

		result.push_back(
			std::move(part)
		);
	}

	return result;
}

RobloxObjectModel::RenderCameraSnapshot
RobloxObjectModel::getRenderCamera(
	lua_State* L
)
{
	RuntimeContext& runtime =
		context(L);

	RenderCameraSnapshot camera;

	if (
		runtime.currentCamera &&
		runtime.currentCamera
			->cameraSubject &&
		!runtime.currentCamera
			->cameraSubject
			->destroyed &&
		isBasePartClass(
			runtime.currentCamera
				->cameraSubject
				->className
		)
	)
	{
		InstanceObject* subject =
			runtime.currentCamera
				->cameraSubject;

		camera.hasSubject = true;

		camera.targetX =
			static_cast<float>(
				subject->position.x
			);

		camera.targetY =
			static_cast<float>(
				subject->position.y
			);

		camera.targetZ =
			static_cast<float>(
				subject->position.z
			);
	}

	return camera;
}
