# RobloxDX11Vulkan

Runtime/engine experimental para executar **Luau escrito no estilo Roblox fora do cliente Roblox**.

O objetivo do projeto é construir uma implementação clean-room da camada de scripting da Roblox Engine:

```text
script .luau
    |
    v
Luau Compiler
    |
    v
Luau VM
    |
    v
Roblox Compatibility API
    |
    +-- game / DataModel
    +-- workspace
    +-- Instance
    +-- Vector3 / CFrame / Color3 ...
    +-- Services
    +-- RBXScriptSignal
    +-- task scheduler
    +-- physics
    +-- renderer DX11/Vulkan
```

## Estado atual

A pasta `native/` é o runtime principal.

Já existe uma primeira camada executável com:

- VM oficial Luau embutida em C++
- compilação de fonte Luau para bytecode
- sandbox por script
- `game:GetService()`
- `workspace`
- `Instance.new()`
- `Vector3.new()`
- soma/subtração e tostring de Vector3
- `typeof()`
- `warn()`
- serviços iniciais: Workspace, RunService, Players, ReplicatedStorage, ServerScriptService e UserInputService

Exemplo:

```lua
local Workspace = game:GetService("Workspace")

local part = Instance.new("Part")
part.Name = "EmulatedPart"
part.Position = Vector3.new(1, 2, 3)
part.Parent = Workspace

print(part.Name, part.Position, typeof(part))
```

## Build nativo

Requisitos:

- CMake 3.20+
- compilador C++17
- Git para o FetchContent baixar Luau

```bat
cmake -S . -B build
cmake --build build --config Release
build\Release\roblox_runtime.exe native\scripts\test.luau
```

O CMake usa o repositório oficial `luau-lang/luau` e liga `Luau.Compiler` + `Luau.VM`.

## Próximos subsistemas

1. Object model real em C++ usando userdata/metatables
2. Parent/Children e métodos de Instance
3. RBXScriptSignal / RBXScriptConnection
4. scheduler para `task.spawn`, `task.defer`, `task.delay`, `task.wait`
5. RunService e loop de frames
6. ModuleScript + `require`
7. CFrame, Color3, UDim2, Enum
8. Workspace raycast/física
9. renderer DirectX 11
10. backend Vulkan

## Importante

Isto não tenta reutilizar binários privados do Roblox. A VM Luau é pública; a camada Roblox precisa ser reimplementada pelo projeto. Luau sozinho não fornece `game`, `Instance`, `Workspace` ou serviços: essas APIs são responsabilidade do host que embute a VM.

A antiga pasta `src/` contém o protótipo inicial de uma framework que rodava dentro do Roblox. Ela não é o runtime principal desta nova arquitetura.
