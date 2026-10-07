# RobloxDX11Vulkan Engine

Camada de engine própria escrita em Luau sobre a API oficial do Roblox.

## Objetivos

- Core de engine e ciclo de vida
- Entidades e componentes
- Scene graph leve
- Sistemas desacoplados
- Física via API moderna do Roblox
- Input cliente
- Runtime/registry para módulos
- Adapter isolando dependências da API Roblox
- Estrutura compatível com Rojo

> Esta engine não substitui o renderer interno do Roblox (DirectX/Vulkan). Ela organiza gameplay, física, input, cenas e runtime sobre as APIs suportadas pela plataforma.

## Estrutura

```text
src/
  shared/Engine/
    Core/
    Components/
    Systems/
    Runtime/
    Roblox/
  server/
  client/
```

## Princípio do runtime

Roblox não permite que um jogo comum leia arbitrariamente o `.Source` de scripts publicados em runtime. Portanto o runtime desta engine interpreta **módulos registrados e descritores Luau**, em vez de usar `loadstring` ou APIs inseguras.

Os módulos podem declarar:

```lua
return {
    Name = "VehicleController",
    Dependencies = { "Physics", "Input" },
    Start = function(context)
        -- inicialização
    end,
}
```

O `ScriptRuntime` resolve dependências e executa os módulos na ordem correta.

## Rojo

Use `default.project.json` para sincronizar com Roblox Studio.
