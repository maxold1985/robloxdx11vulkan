local RunService = game:GetService("RunService")

local RobloxAdapter = {}
RobloxAdapter.__index = RobloxAdapter

function RobloxAdapter.new()
	return setmetatable({}, RobloxAdapter)
end

function RobloxAdapter:IsClient(): boolean
	return RunService:IsClient()
end

function RobloxAdapter:IsServer(): boolean
	return RunService:IsServer()
end

function RobloxAdapter:GetService(serviceName: string)
	return game:GetService(serviceName)
end

function RobloxAdapter:ConnectHeartbeat(callback)
	return RunService.Heartbeat:Connect(callback)
end

function RobloxAdapter:Defer(callback, ...)
	task.defer(callback, ...)
end

function RobloxAdapter:Delay(seconds: number, callback, ...)
	task.delay(seconds, callback, ...)
end

return RobloxAdapter
