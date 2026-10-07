#include "RobloxScheduler.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <unordered_map>
#include <vector>

#include "lua.h"
#include "lualib.h"

namespace
{
	struct ThreadRecord
	{
		lua_State* thread = nullptr;
		int threadRef = LUA_NOREF;

		double wakeTime = 0.0;
		double waitStartedAt = 0.0;

		int initialArgumentCount = 0;

		unsigned long long minimumStep = 0;

		bool started = false;
		bool scheduled = false;
		bool resumeWithElapsed = false;
		bool cancelled = false;
	};

	struct SchedulerContext
	{
		double time = 0.0;
		unsigned long long stepCounter = 0;

		std::vector<
			std::unique_ptr<ThreadRecord>
		> records;

		std::unordered_map<
			lua_State*,
			ThreadRecord*
		> byThread;
	};

	std::unordered_map<
		lua_State*,
		std::unique_ptr<SchedulerContext>
	> contexts;

	SchedulerContext& context(lua_State* L)
	{
		lua_State* mainThread =
			lua_mainthread(L);

		auto iterator =
			contexts.find(mainThread);

		if (iterator == contexts.end())
		{
			luaL_error(
				L,
				"Roblox scheduler is not installed"
			);
		}

		return *iterator->second;
	}

	void releaseRecord(
		lua_State* mainThread,
		SchedulerContext& scheduler,
		ThreadRecord* record
	)
	{
		if (!record)
			return;

		scheduler.byThread.erase(
			record->thread
		);

		if (
			record->threadRef !=
			LUA_NOREF
		)
		{
			record->threadRef =
				lua_unref(
					mainThread,
					record->threadRef
				);
		}

		auto iterator =
			std::remove_if(
				scheduler.records.begin(),
				scheduler.records.end(),
				[
					record
				](
					const std::unique_ptr<
						ThreadRecord
					>& item
				)
				{
					return
						item.get() ==
						record;
				}
			);

		scheduler.records.erase(
			iterator,
			scheduler.records.end()
		);
	}

	ThreadRecord* findRecord(
		SchedulerContext& scheduler,
		lua_State* thread
	)
	{
		auto iterator =
			scheduler.byThread.find(
				thread
			);

		return
			iterator ==
				scheduler.byThread.end()
				? nullptr
				: iterator->second;
	}

	ThreadRecord* pinCurrentThread(
		lua_State* L,
		SchedulerContext& scheduler
	)
	{
		if (
			ThreadRecord* existing =
				findRecord(
					scheduler,
					L
				)
		)
		{
			return existing;
		}

		lua_pushthread(L);

		const int ref =
			lua_ref(L, -1);

		lua_pop(L, 1);

		auto record =
			std::make_unique<
				ThreadRecord
			>();

		record->thread = L;
		record->threadRef = ref;
		record->started = true;

		ThreadRecord* raw =
			record.get();

		scheduler.records.push_back(
			std::move(record)
		);

		scheduler.byThread[L] = raw;

		return raw;
	}

	ThreadRecord* createThread(
		lua_State* caller,
		int functionIndex,
		int firstArgumentIndex,
		int lastArgumentIndex
	)
	{
		SchedulerContext& scheduler =
			context(caller);

		lua_State* mainThread =
			lua_mainthread(caller);

		lua_State* thread =
			lua_newthread(mainThread);

		const int threadRef =
			lua_ref(
				mainThread,
				-1
			);

		lua_pop(mainThread, 1);

		lua_xpush(
			caller,
			thread,
			functionIndex
		);

		int argumentCount = 0;

		for (
			int index =
				firstArgumentIndex;
			index <=
				lastArgumentIndex;
			++index
		)
		{
			lua_xpush(
				caller,
				thread,
				index
			);

			++argumentCount;
		}

		auto record =
			std::make_unique<
				ThreadRecord
			>();

		record->thread = thread;
		record->threadRef = threadRef;
		record->initialArgumentCount =
			argumentCount;

		ThreadRecord* raw =
			record.get();

		scheduler.records.push_back(
			std::move(record)
		);

		scheduler.byThread[thread] = raw;

		return raw;
	}

	void printThreadError(
		lua_State* thread,
		const char* prefix
	)
	{
		const char* message =
			lua_tostring(
				thread,
				-1
			);

		std::fprintf(
			stderr,
			"%s%s\n",
			prefix,
			message
				? message
				: "unknown Luau error"
		);

		const char* trace =
			lua_debugtrace(thread);

		if (trace)
		{
			std::fprintf(
				stderr,
				"%s\n",
				trace
			);
		}
	}

	void resumeRecord(
		lua_State* mainThread,
		SchedulerContext& scheduler,
		ThreadRecord* record
	)
	{
		if (
			!record ||
			record->cancelled
		)
		{
			return;
		}

		record->scheduled = false;

		int argumentCount = 0;

		if (!record->started)
		{
			record->started = true;

			argumentCount =
				record
					->initialArgumentCount;

			record
				->initialArgumentCount = 0;
		}
		else if (
			record->resumeWithElapsed
		)
		{
			const double elapsed =
				scheduler.time -
				record->waitStartedAt;

			lua_pushnumber(
				record->thread,
				elapsed
			);

			argumentCount = 1;

			record->resumeWithElapsed =
				false;
		}

		const int status =
			lua_resume(
				record->thread,
				mainThread,
				argumentCount
			);

		if (status == LUA_OK)
		{
			releaseRecord(
				mainThread,
				scheduler,
				record
			);

			return;
		}

		if (status == LUA_YIELD)
		{
			if (!record->scheduled)
			{
				// Coroutine yielded manually instead of through
				// task.wait(). Keep it pinned; a future runtime
				// primitive may resume it.
			}

			return;
		}

		printThreadError(
			record->thread,
			"[task] "
		);

		releaseRecord(
			mainThread,
			scheduler,
			record
		);
	}

	int taskWait(lua_State* L)
	{
		if (!lua_isyieldable(L))
		{
			luaL_error(
				L,
				"task.wait cannot yield from this context"
			);
		}

		const double seconds =
			std::max(
				0.0,
				luaL_optnumber(
					L,
					1,
					0.0
				)
			);

		SchedulerContext& scheduler =
			context(L);

		ThreadRecord* record =
			pinCurrentThread(
				L,
				scheduler
			);

		record->waitStartedAt =
			scheduler.time;

		record->wakeTime =
			scheduler.time +
			seconds;

		record->minimumStep =
			scheduler.stepCounter + 1;

		record->scheduled = true;
		record->resumeWithElapsed = true;

		return lua_yield(L, 0);
	}

	int taskSpawn(lua_State* L)
	{
		luaL_checktype(
			L,
			1,
			LUA_TFUNCTION
		);

		ThreadRecord* record =
			createThread(
				L,
				1,
				2,
				lua_gettop(L)
			);

		SchedulerContext& scheduler =
			context(L);

		lua_State* mainThread =
			lua_mainthread(L);

		const int ref =
			record->threadRef;

		resumeRecord(
			mainThread,
			scheduler,
			record
		);

		if (ref != LUA_NOREF)
			lua_getref(L, ref);
		else
			lua_pushnil(L);

		return 1;
	}

	int taskDefer(lua_State* L)
	{
		luaL_checktype(
			L,
			1,
			LUA_TFUNCTION
		);

		ThreadRecord* record =
			createThread(
				L,
				1,
				2,
				lua_gettop(L)
			);

		SchedulerContext& scheduler =
			context(L);

		record->wakeTime =
			scheduler.time;

		record->minimumStep =
			scheduler.stepCounter + 1;

		record->scheduled = true;

		lua_getref(
			L,
			record->threadRef
		);

		return 1;
	}

	int taskDelay(lua_State* L)
	{
		const double seconds =
			std::max(
				0.0,
				luaL_checknumber(
					L,
					1
				)
			);

		luaL_checktype(
			L,
			2,
			LUA_TFUNCTION
		);

		ThreadRecord* record =
			createThread(
				L,
				2,
				3,
				lua_gettop(L)
			);

		SchedulerContext& scheduler =
			context(L);

		record->wakeTime =
			scheduler.time +
			seconds;

		record->minimumStep =
			scheduler.stepCounter + 1;

		record->scheduled = true;

		lua_getref(
			L,
			record->threadRef
		);

		return 1;
	}

	int taskCancel(lua_State* L)
	{
		lua_State* thread =
			lua_tothread(L, 1);

		if (!thread)
		{
			luaL_typeerror(
				L,
				1,
				"thread"
			);
		}

		SchedulerContext& scheduler =
			context(L);

		ThreadRecord* record =
			findRecord(
				scheduler,
				thread
			);

		if (!record)
			return 0;

		record->cancelled = true;

		releaseRecord(
			lua_mainthread(L),
			scheduler,
			record
		);

		return 0;
	}
}

void RobloxScheduler::install(
	lua_State* L
)
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

	contexts.emplace(
		mainThread,
		std::make_unique<
			SchedulerContext
		>()
	);

	lua_newtable(L);

	lua_pushcfunction(
		L,
		taskSpawn,
		"task.spawn"
	);
	lua_setfield(L, -2, "spawn");

	lua_pushcfunction(
		L,
		taskDefer,
		"task.defer"
	);
	lua_setfield(L, -2, "defer");

	lua_pushcfunction(
		L,
		taskDelay,
		"task.delay"
	);
	lua_setfield(L, -2, "delay");

	lua_pushcfunction(
		L,
		taskWait,
		"task.wait"
	);
	lua_setfield(L, -2, "wait");

	lua_pushcfunction(
		L,
		taskCancel,
		"task.cancel"
	);
	lua_setfield(L, -2, "cancel");

	lua_setglobal(L, "task");
}

void RobloxScheduler::shutdown(
	lua_State* L
)
{
	lua_State* mainThread =
		lua_mainthread(L);

	auto iterator =
		contexts.find(mainThread);

	if (iterator == contexts.end())
		return;

	SchedulerContext& scheduler =
		*iterator->second;

	for (
		auto& record :
		scheduler.records
	)
	{
		if (
			record->threadRef !=
			LUA_NOREF
		)
		{
			record->threadRef =
				lua_unref(
					mainThread,
					record->threadRef
				);
		}
	}

	scheduler.byThread.clear();
	scheduler.records.clear();

	contexts.erase(iterator);
}

void RobloxScheduler::step(
	lua_State* L,
	double deltaTime
)
{
	SchedulerContext& scheduler =
		context(L);

	scheduler.time +=
		std::max(
			0.0,
			deltaTime
		);

	++scheduler.stepCounter;

	std::vector<
		ThreadRecord*
	> due;

	for (
		const auto& record :
		scheduler.records
	)
	{
		if (
			record->cancelled ||
			!record->scheduled
		)
		{
			continue;
		}

		if (
			scheduler.stepCounter <
				record->minimumStep ||
			scheduler.time <
				record->wakeTime
		)
		{
			continue;
		}

		due.push_back(
			record.get()
		);
	}

	lua_State* mainThread =
		lua_mainthread(L);

	for (ThreadRecord* record : due)
	{
		if (
			findRecord(
				scheduler,
				record->thread
			) != record
		)
		{
			continue;
		}

		resumeRecord(
			mainThread,
			scheduler,
			record
		);
	}
}

bool RobloxScheduler::isManaged(
	lua_State* L,
	lua_State* thread
)
{
	if (!thread)
		return false;

	SchedulerContext& scheduler =
		context(L);

	return
		findRecord(
			scheduler,
			thread
		) != nullptr;
}
