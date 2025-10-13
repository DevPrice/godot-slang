# Godot Slang

[![Build GDExtension](https://github.com/DevPrice/godot-slang/actions/workflows/builds.yml/badge.svg)](https://github.com/DevPrice/godot-slang/actions/workflows/builds.yml)

This project is a work-in-progress to support [Slang](https://shader-slang.org/)-based compute shaders in Godot.

## Features
* Simple and straightforward handling of `.slang` files. You can `load("res://something.slang")` and it works exactly how you would expect.
* Supports most `.glsl` and `.hlsl` shaders in addition to `.slang`
* Supports most major Slang features, including modules.
* Uses a high-level API for binding shader parameters, similar to GDShader (`set_shader_parameter(...)`).
* Minimizes boilerplate by automatically binding shader parameters for common textures (color, depth, normal/roughness, etc.), current time, shader globals, and more.
* First-class support for compositor effects.

## Usage

After installing this plugin in Godot, you'll see a few new types available in the editor:
* `ComputeShaderFile`
  * A resource imported from a `.slang` file in the project. Includes a list of `ComputeShaderKernel`s.
  * Unlike `.glsl` compute shaders, a Slang compute shader can have more than one entry-point.
* `ComputeShaderKernel`
  * A resource containing the compiled SPIR-V and reflection information about the shader parameters for a single entry-point.
* `ComputeShaderTask`
  * Similar to a "material" in the fragment shader world. You can create this from a list of kernels.
  * Stores shader parameters and exposes methods for dispatching the shader.
* `ComputeShaderEffect`
  * This offers a convenient way to use Slang compute shaders in `CompositorEffect`s. For many shaders, you can drag a Slang file onto an instance of this to have it running with no additional application code required.
  * Automatically reloads if the attached compute shader is modified.

After installing, Slang files in your project will be automatically imported as compute shaders.
Each function in the Slang source annotated with `[shader("compute")]` will be imported as a `ComputeShaderKernel`.

If you'd like, you can ignore the `ComputeShaderTask` and `ComputeShaderEffect` classes, and instead directly bind your shader parameters from code just like you would with `.glsl`.
If you choose to do this, it can be helpful to explicitly declare Vulkan bindings in your shader:
```slang
[[vk::binding(1,2)]]
Texture2D explicit_binding_texture;
```

## Installation

Files for the addon can be downloaded from the [Releases page](https://github.com/DevPrice/godot-slang/releases),
[GitHub Actions builds](https://github.com/DevPrice/godot-slang/actions/workflows/builds.yml) on each commit to `main`,
or you can build from source as described below.

After downloading a release, you can install this addon in Godot by:
* Opening the `AssetsLib` tab
* Clicking "Import..." at the top right
* Selecting the `.zip` file that you downloaded above

## Building

After cloning the repo, build Slang:

```shell
git submodule update --init --recursive
cd slang
cmake --preset vs2022 -DSLANG_LIB_TYPE=STATIC # or 'vs2019' or `vs2022-dev` or `default`
cmake --build --preset releaseWithDebugInfo
```

Then, build the GDExtension:
```shell
scons target=editor debug_symbols=yes dev_build=yes
```

## Demo

See [example shaders](demo/shaders) and their usage in the [demo project](demo). Here is an example of what a Slang compute shader for Godot might look like:

```slang
import godot;

[gd::compositor::ColorTexture]
RWTexture2D<float4> scene_color;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 threadId: SV_DispatchThreadID) {
    float4 color = scene_color[threadId.xy];
    scene_color[threadId.xy] = color.r * 0.299 + color.g * 0.587 + color.b * 0.114;
}
```

## Library

This plugin also includes a [utility Slang module](demo/addons/shader-slang/modules/godot.slang) for common compute/CompositorEffect use-cases in Godot.

For example, the `gd::compositor::ColorTexture` attribute used above, which automatically binds the screen color texture to the parameter when the shader is used in a `ComputeShaderEffect`.
This library includes many other attributes, Godot-specific utility functions such as `normal_roughness_compatibility`, and declares the `SceneDataBlock` struct, as exposed by the Godot engine to compositor effects.

## Work-in-progress

Although `ComputeShaderFile` and `ComputeShaderKernel` should be fully functional, generating full reflection data for Godot is still very much a work-in-progress.
If you don't need reflection information, then these should suffice for importing and running Slang shaders. However, you may need to handle shader dispatching and parameter bindings manually.

`ComputeShaderTask::set_shader_parameter` supports many basic types, but is not yet fully implemented. See the table below for the status of specific binding types.
`ComputeShaderTask` and `ComputeShaderEffect` are in a useful state if you are mostly reading render buffer textures and simple uniform data, but aren't quite ready for being used with complex data or nested structs.

| Binding type                                       | Status                |
|----------------------------------------------------|-----------------------|
| Global uniforms                                    | ✅ Mostly working      |
| Push constants                                     | ✅ Mostly working      |
| User-defined structs                               | ✅ Mostly working      |
| `Texture2D`/`RWTexture2D`                          | ✅ Mostly working      |
| `Sampler2D`                                        | ✅ Mostly working      |
| `SamplerState`                                     | ✅ Mostly working      |
| `ConstantBuffer`                                   | ✅ Mostly working      |
| `StructuredBuffer`/`RWStructuredBuffer`            | ✅ Mostly working      |
| `ByteAddressBuffer`/`RWByteAddressBuffer`          | ❌ Not yet implemented |
| `AppendStructuredBuffer`/`ConsumeStructuredBuffer` | ❌ Not yet implemented |
| `ParameterBlock`                                   | ❌ Not yet implemented |
| Interfaces                                         | ❌ Not yet implemented |

## License

This project is available under the MIT license.

This project includes code from [Slang](https://github.com/shader-slang/slang), licensed under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).
