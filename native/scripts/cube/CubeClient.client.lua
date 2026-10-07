local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local UserInputService = game:GetService("UserInputService")
local Workspace = game:GetService("Workspace")

local player = Players.LocalPlayer

local remote =
	ReplicatedStorage:WaitForChild("CubeControl")

local KEY_ACTIONS = {
	[Enum.KeyCode.W] = "Forward",
	[Enum.KeyCode.S] = "Backward",
	[Enum.KeyCode.A] = "Left",
	[Enum.KeyCode.D] = "Right",
	[Enum.KeyCode.Space] = "Jump",
}

local function sendInput(input, pressed)
	local action = KEY_ACTIONS[input.KeyCode]

	if not action then
		return
	end

	remote:FireServer(
		action,
		pressed
	)
end

UserInputService.InputBegan:Connect(
	function(input, gameProcessed)
		if gameProcessed then
			return
		end

		sendInput(input, true)
	end
)

UserInputService.InputEnded:Connect(
	function(input)
		sendInput(input, false)
	end
)

local cube =
	Workspace:WaitForChild(
		"Cube_" .. player.UserId
	)

local camera = Workspace.CurrentCamera
camera.CameraSubject = cube
