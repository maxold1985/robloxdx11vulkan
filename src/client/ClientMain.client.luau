local ReplicatedStorage = game:GetService("ReplicatedStorage")

local EnginePackage = ReplicatedStorage:WaitForChild("RobloxEngine")

local Engine = require(EnginePackage.Core.Engine)
local RobloxAdapter = require(EnginePackage.Roblox.RobloxAdapter)
local InputSystem = require(EnginePackage.Systems.InputSystem)
local PhysicsSystem = require(EnginePackage.Systems.PhysicsSystem)

local engine = Engine.new(RobloxAdapter.new())

engine:RegisterSystem(InputSystem.new())
engine:RegisterSystem(PhysicsSystem.new())

local gameModules = script.Parent:FindFirstChild("GameModules")
if gameModules ~= nil then
	engine.Runtime:LoadFolder(gameModules)
end

engine:Start()
