# Godot Slang
This project is a work-in-progress to support [Slang](https://shader-slang.org/)-based compute shaders in Godot.

## Features
* Simple and straightforward handling of `.slang` files. You can `load("res://something.slang")` and it works exactly how you would expect.
* Support all major Slang features, including modules.
* Uses a high-level API for binding shader parameters, similar to GDShader (`set_shader_parameter(...)`).
* Minimizes boilerplate by supporting automatically bound shader parameters for things like the render texture, engine time, and more. Everything works out of the box.
* First-class support for compositor effects written in Slang.

## Usage

After installing this plugin in Godot, you'll see a few new types available in the editor:
* `ComputeShaderFile`
  * A resource imported from a `.slang` file in the project. Includes a list of `ComputeShaderKernel`s. A Slang resource may have multiple entrypoints, each mapped to a single kernel.
* `ComputeShaderKernel`
  * A resource containing the compiled SPIR-V and reflection information about the shader parameters for a single entry-point.
  * Unlike `.glsl` compute shaders, a Slang compute shader can have more than one entry-point.
* `ComputeShaderTask`
  * Similar to a "material" in the fragment shader world. You can create this from a list of kernels. It stores shader parameters and exposes methods for dispatching the shader.
* `ComputeShaderEffect`
  * This offers a convenient way to use Slang compute shaders in `CompositorEffect`s. For many shaders, you can drag a Slang file onto an instance of this to have it running with no additional application code required.
  * *Not yet exposed. See [ComputeShaderEffect](demo/compute_shader_effect.gd) for how this will eventually work.*

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

## License

This project is available under the MIT license.

This project includes code from [Slang](https://github.com/shader-slang/slang), licensed under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).
