return {
	Name = "Example",
	Dependencies = {},

	Init = function(context)
		local object = context.Scene:CreateObject("RuntimeExample")
		object.RuntimeLoaded = true
	end,

	Start = function(_context)
		print("[RobloxDX11Vulkan] Example runtime module started")
	end,
}
