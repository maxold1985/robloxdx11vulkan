local ReplicatedStorage = game:GetService("ReplicatedStorage")

local EnginePackage = ReplicatedStorage:WaitForChild("RobloxEngine")

local Engine = require(EnginePackage.Core.Engine)
local RobloxAdapter = require(EnginePackage.Roblox.RobloxAdapter)
local PhysicsSystem = require(EnginePackage.Systems.PhysicsSystem)

local engine = Engine.new(RobloxAdapter.new())

engine:RegisterSystem(PhysicsSystem.new())

local gameModules = script:FindFirstChild("GameModules")
if gameModules ~= nil then
	engine.Runtime:LoadFolder(gameModules)
end

engine:Start()

game:BindToClose(function()
	engine:Destroy()
end)
