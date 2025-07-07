# Godot Slang

[![Build GDExtension](https://github.com/DevPrice/godot-slang/actions/workflows/builds.yml/badge.svg)](https://github.com/DevPrice/godot-slang/actions/workflows/builds.yml)

This project is a work-in-progress to support [Slang](https://shader-slang.org/)-based compute shaders in Godot.

## Features
* Simple and straightforward handling of `.slang` files. You can `load("res://something.slang")` and it works exactly how you would expect.
* Supports most major Slang features, including modules.
* Uses a high-level API for binding shader parameters, similar to GDShader (`set_shader_parameter(...)`).
* Minimizes boilerplate by automatically binding shader parameters for common textures (color, depth, normal/roughness, etc.), current time, shader globals, and more.
* First-class support for compositor effects written in Slang.

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

## Installation

Files for the addon can be downloaded from GitHub Actions builds on each commit to `main`.

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

Not all features are supported. You should find that basic stuff works, and you can bind most basic parameters.
You may run into issues if you try to bind more complex structures into buffers, as I haven't yet started testing that.
Feel free to file an issue or contribute a fix if you run into any problems.

Known issues:
* Global shader parameters of the `sampler*` type can't be bound.
* Storage buffers and push constants are not yet supported.
* `ParameterBlock` is not yet fully supported (but may work in some cases). `ConstantBuffer` should mostly work.
* Nested struct parameters are untested and probably don't work yet.

## License

This project is available under the MIT license.

This project includes code from [Slang](https://github.com/shader-slang/slang), licensed under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).
