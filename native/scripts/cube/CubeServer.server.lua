local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local Workspace = game:GetService("Workspace")

local REMOTE_NAME = "CubeControl"

local MOVE_SPEED = 24
local JUMP_SPEED = 35

local baseplate = Workspace:FindFirstChild("Baseplate")

if not baseplate then
	baseplate = Instance.new("Part")
	baseplate.Name = "Baseplate"
	baseplate.Size = Vector3.new(2048, 1, 2048)
	baseplate.Position = Vector3.new(0, -0.5, 0)
	baseplate.Anchored = true
	baseplate.CanCollide = true
	baseplate.Color = Color3.fromRGB(91, 93, 105)
	baseplate.Material = Enum.Material.SmoothPlastic
	baseplate.Parent = Workspace
end

local remote = ReplicatedStorage:FindFirstChild(REMOTE_NAME)

if not remote then
	remote = Instance.new("RemoteEvent")
	remote.Name = REMOTE_NAME
	remote.Parent = ReplicatedStorage
end

local cubes = {}
local inputs = {}

local VALID_ACTIONS = {
	Forward = true,
	Backward = true,
	Left = true,
	Right = true,
	Jump = true,
}

local function createCube(player)
	local cube = Instance.new("Part")

	cube.Name = "Cube_" .. player.UserId
	cube.Size = Vector3.new(4, 4, 4)

	cube.Position = Vector3.new(
		math.random(-20, 20),
		10,
		math.random(-20, 20)
	)

	cube.Color = Color3.fromRGB(0, 170, 255)
	cube.Material = Enum.Material.SmoothPlastic

	cube.Anchored = false
	cube.CanCollide = true

	cube:SetAttribute("OwnerUserId", player.UserId)

	cube.Parent = Workspace

	-- Física controlada pelo servidor.
	cube:SetNetworkOwner(nil)

	cubes[player] = cube

	inputs[player] = {
		Forward = false,
		Backward = false,
		Left = false,
		Right = false,
		Jump = false,
		JumpPressed = false,
	}
end

local function destroyCube(player)
	local cube = cubes[player]

	if cube then
		cube:Destroy()
	end

	cubes[player] = nil
	inputs[player] = nil
end

local function isGrounded(cube)
	local params = RaycastParams.new()

	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = {
		cube,
	}

	local distance =
		(cube.Size.Y * 0.5) + 0.3

	local result = Workspace:Raycast(
		cube.Position,
		Vector3.new(0, -distance, 0),
		params
	)

	return result ~= nil
end

remote.OnServerEvent:Connect(function(player, action, pressed)
	if typeof(action) ~= "string" then
		return
	end

	if typeof(pressed) ~= "boolean" then
		return
	end

	if not VALID_ACTIONS[action] then
		return
	end

	local input = inputs[player]

	if not input then
		return
	end

	if action == "Jump" then
		if pressed and not input.Jump then
			input.JumpPressed = true
		end

		input.Jump = pressed

		return
	end

	input[action] = pressed
end)

RunService.Heartbeat:Connect(function()
	for player, cube in pairs(cubes) do
		if not cube.Parent then
			continue
		end

		local input = inputs[player]

		if not input then
			continue
		end

		local x = 0
		local z = 0

		if input.Forward then
			z -= 1
		end

		if input.Backward then
			z += 1
		end

		if input.Left then
			x -= 1
		end

		if input.Right then
			x += 1
		end

		local direction = Vector3.new(x, 0, z)

		if direction.Magnitude > 1 then
			direction = direction.Unit
		end

		local velocity = cube.AssemblyLinearVelocity

		cube.AssemblyLinearVelocity = Vector3.new(
			direction.X * MOVE_SPEED,
			velocity.Y,
			direction.Z * MOVE_SPEED
		)

		if input.JumpPressed then
			input.JumpPressed = false

			if isGrounded(cube) then
				local impulse = Vector3.new(
					0,
					cube.AssemblyMass * JUMP_SPEED,
					0
				)

				cube:ApplyImpulse(impulse)
			end
		end
	end
end)

Players.PlayerAdded:Connect(createCube)
Players.PlayerRemoving:Connect(destroyCube)

for _, player in Players:GetPlayers() do
	createCube(player)
end
