local UserInputService = game:GetService("UserInputService")

local InputSystem = {}
InputSystem.__index = InputSystem

InputSystem.Name = "Input"
InputSystem.Priority = 10

function InputSystem.new()
	local self = setmetatable({}, InputSystem)

	self.Name = InputSystem.Name
	self.Priority = InputSystem.Priority
	self._down = {}
	self._beganConnection = nil
	self._endedConnection = nil

	return self
end

function InputSystem:Start(context)
	assert(context.Adapter:IsClient(), "InputSystem can only run on the client")

	self._beganConnection = UserInputService.InputBegan:Connect(function(input, gameProcessed)
		if not gameProcessed then
			self._down[input.KeyCode] = true
		end
	end)

	self._endedConnection = UserInputService.InputEnded:Connect(function(input)
		self._down[input.KeyCode] = nil
	end)
end

function InputSystem:IsDown(keyCode: Enum.KeyCode): boolean
	return self._down[keyCode] == true
end

function InputSystem:GetMouseDelta(): Vector2
	return UserInputService:GetMouseDelta()
end

function InputSystem:Stop()
	if self._beganConnection ~= nil then
		self._beganConnection:Disconnect()
		self._beganConnection = nil
	end

	if self._endedConnection ~= nil then
		self._endedConnection:Disconnect()
		self._endedConnection = nil
	end

	table.clear(self._down)
end

return InputSystem
