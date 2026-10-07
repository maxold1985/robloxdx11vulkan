local Transform = {}
Transform.__index = Transform

Transform.Name = "Transform"

function Transform.new(target: Instance)
	assert(target ~= nil, "Transform requires a Roblox instance")
	assert(target:IsA("BasePart") or target:IsA("Model"), "Transform target must be BasePart or Model")

	local self = setmetatable({}, Transform)

	self.Name = Transform.Name
	self.Target = target
	self.Owner = nil

	return self
end

function Transform:GetCFrame(): CFrame
	if self.Target:IsA("Model") then
		return self.Target:GetPivot()
	end

	return self.Target.CFrame
end

function Transform:SetCFrame(value: CFrame)
	if self.Target:IsA("Model") then
		self.Target:PivotTo(value)
	else
		self.Target.CFrame = value
	end
end

function Transform:GetPosition(): Vector3
	return self:GetCFrame().Position
end

function Transform:SetPosition(position: Vector3)
	local current = self:GetCFrame()
	self:SetCFrame(CFrame.new(position) * current.Rotation)
end

function Transform:Translate(offset: Vector3)
	self:SetCFrame(self:GetCFrame() + offset)
end

function Transform:Rotate(xRadians: number, yRadians: number, zRadians: number)
	self:SetCFrame(self:GetCFrame() * CFrame.fromOrientation(xRadians, yRadians, zRadians))
end

function Transform:ScaleTo(scale: number)
	assert(scale > 0, "Scale must be greater than zero")

	if self.Target:IsA("Model") then
		self.Target:ScaleTo(scale)
	else
		self.Target.Size *= scale
	end
end

return Transform
