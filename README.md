# Godot Slang

[![Build GDExtension](https://github.com/DevPrice/godot-slang/actions/workflows/builds.yml/badge.svg)](https://github.com/DevPrice/godot-slang/actions/workflows/builds.yml)

This project is a work-in-progress to support [Slang](https://shader-slang.org/)-based compute shaders in Godot. The primary goal of this project is to simplify working with compute shaders within Godot.

## Features
* Simple and straightforward handling of `.slang` files. You can `load("res://something.slang")` and it works exactly how you would expect.
* Uses a high-level API for binding shader parameters, similar to GDShader (`set_shader_parameter(...)`).
* Minimizes boilerplate by automatically binding shader parameters for common textures (color, depth, normal/roughness, etc.), current time, shader globals, and more.
* Allows exposing shader parameters to the editor via a familiar export pattern.
* First-class support for compositor effects.
* Supports most major Slang features, including modules.
* Supports most `.glsl` and `.hlsl` shaders in addition to `.slang`.

## Usage

After installing this plugin in Godot, you'll see a few new types available in the editor:
* `ComputeShaderFile`
  * A resource imported from a `.slang` file in the project. Includes a list of `ComputeShaderKernel`s.
  * Unlike `.glsl` compute shaders, a Slang compute shader can have more than one entry-point.
* `ComputeShaderKernel`
  * A resource containing the compiled SPIR-V and reflection information about the shader parameters for a single entry-point.
* `ComputeShaderTask`
  * Similar to a "material" in the fragment shader world. Associated with a single compute shader file.
  * Stores shader parameters and exposes methods for dispatching the shader.
  * Automatically reloads if the attached compute shader is modified.
* `ComputeShaderEffect`
  * This offers a convenient way to use Slang compute shaders in `CompositorEffect`s. For many shaders, you can create an effect from a Slang file and have it running with no additional application code required.
* `ComputeTexture` (experimental)
  * `Texture2D` resource backed by a compute shader.

For more information about these classes, see [the class documentation](https://devprice.github.io/godot-slang/classes/index.html).

After installing, Slang files in your project will be automatically imported as compute shaders.
Each function in the Slang source annotated with `[shader("compute")]` will be imported as a `ComputeShaderKernel`.

If you'd like, you can ignore the `ComputeShaderTask` and `ComputeShaderEffect` classes, and instead directly bind your shader parameters from code just like you would with a standard `.glsl` import.
If you choose to do this, it can be helpful to explicitly declare Vulkan bindings in your shader:
```slang
[[vk::binding(1,2)]]
Texture2D explicit_binding_texture;
```

## Installation

Available in the [Godot asset library](https://godotengine.org/asset-library/asset/4471), find it in the `AssetsLib` tab within the editor.

Files for the addon can also be downloaded from the [Releases page](https://github.com/DevPrice/godot-slang/releases),
[GitHub Actions builds](https://github.com/DevPrice/godot-slang/actions/workflows/builds.yml) on each commit to `main`,
or you can build from source as described below.

After downloading a release, you can install this addon in Godot by:
* Opening the `AssetsLib` tab
* Clicking "Import..." at the top right
* Selecting the `.zip` file that you downloaded above

## Demo

See [example shaders](demo/shaders) and their usage in the [demo project](demo). Here is an example of what a Slang compute shader for Godot might look like:

```slang
import godot;

[gd::compositor::ColorTexture]
RWTexture2D<float4> scene_color;

// makes the property editable in the inspector
[gd::Export]
float3 luminance_weights;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 threadId: SV_DispatchThreadID) {
    float4 color = scene_color[threadId.xy];
    float luminance = dot(color.rgb, luminance_weights);
    scene_color[threadId.xy] = float4(luminance.xxx, color.a);
}
```

## Library

This plugin also includes a [utility Slang module](demo/addons/shader-slang/modules/godot.slang) for common compute/CompositorEffect use-cases in Godot.

For example, the `gd::compositor::ColorTexture` attribute used above, which automatically binds the screen color texture to the parameter when the shader is used in a `ComputeShaderEffect`.
This library includes many other attributes, Godot-specific utility functions such as `normal_roughness_compatibility`, and declares the `SceneDataBlock` struct, as exposed by the Godot engine to compositor effects.

## Building

Set up your dev environment for [compiling GDExtension with scons](https://docs.godotengine.org/en/stable/engine_details/development/compiling/index.html#building-from-source).

After cloning the repo, initialize submodules:

```shell
git submodule update --init --recursive
```

Then, build the GDExtension:
```shell
scons target=editor debug_symbols=yes dev_build=yes api_version=4.5
```

## Work-in-progress

Although `ComputeShaderFile` and `ComputeShaderKernel` should be fully functional, generating full reflection data for Godot is still very much a work-in-progress.
If you don't need reflection information, then these should suffice for importing and running Slang shaders. However, you may need to handle shader dispatching and parameter bindings manually.

`ComputeShaderTask::set_shader_parameter` supports most parameter types, but is not yet fully implemented. In particular, `ParameterBlock` is not yet supported.

[Web documentation](https://devprice.github.io/godot-slang/) is currently bare-bones, but in-progress.

If you have questions or comments, please start a [discussion](https://github.com/DevPrice/godot-slang/discussions/).

## License

This project is available under the MIT license.

This project includes code from [Slang](https://github.com/shader-slang/slang), licensed under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).
