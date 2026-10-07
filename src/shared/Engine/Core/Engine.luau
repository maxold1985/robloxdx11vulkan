local Scene = require(script.Parent.Scene)
local ScriptRuntime = require(script.Parent.Parent.Runtime.ScriptRuntime)

local Engine = {}
Engine.__index = Engine

function Engine.new(adapter)
	assert(adapter ~= nil, "Engine requires a RobloxAdapter")

	local self = setmetatable({}, Engine)

	self.Adapter = adapter
	self.Scene = Scene.new()
	self.Runtime = ScriptRuntime.new()

	self._systems = {}
	self._systemsByName = {}
	self._running = false
	self._heartbeatConnection = nil

	self.Context = {
		Engine = self,
		Scene = self.Scene,
		Adapter = self.Adapter,
	}

	return self
end

function Engine:RegisterSystem(system)
	assert(type(system) == "table", "System must be a table")
	assert(type(system.Name) == "string" and system.Name ~= "", "System requires Name")

	if self._systemsByName[system.Name] ~= nil then
		error(("System '%s' is already registered"):format(system.Name))
	end

	system.Priority = system.Priority or 100
	self._systemsByName[system.Name] = system
	table.insert(self._systems, system)

	table.sort(self._systems, function(a, b)
		return a.Priority < b.Priority
	end)

	return system
end

function Engine:GetSystem(name: string)
	return self._systemsByName[name]
end

function Engine:Start()
	if self._running then
		return
	end

	self._running = true

	for _, system in self._systems do
		if type(system.Start) == "function" then
			system:Start(self.Context)
		end
	end

	self.Runtime:Start(self.Context)

	self._heartbeatConnection = self.Adapter:ConnectHeartbeat(function(deltaTime)
		self:Step(deltaTime)
	end)
end

function Engine:Step(deltaTime: number)
	if not self._running then
		return
	end

	for _, system in self._systems do
		if type(system.Update) == "function" then
			system:Update(deltaTime, self.Context)
		end
	end

	self.Runtime:Update(deltaTime, self.Context)
	self.Scene:Update(deltaTime)
end

function Engine:Stop()
	if not self._running then
		return
	end

	self._running = false

	if self._heartbeatConnection ~= nil then
		self._heartbeatConnection:Disconnect()
		self._heartbeatConnection = nil
	end

	self.Runtime:Stop(self.Context)

	for index = #self._systems, 1, -1 do
		local system = self._systems[index]
		if type(system.Stop) == "function" then
			system:Stop(self.Context)
		end
	end
end

function Engine:Destroy()
	self:Stop()
	self.Scene:Destroy()
	table.clear(self._systems)
	table.clear(self._systemsByName)
end

return Engine
