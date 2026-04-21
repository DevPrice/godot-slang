#pragma once

#include "slang.h"
#include "slang-com-ptr.h"

#include "compute_shader_file.h"
#include "compute_shader_kernel.h"
#include "compute_shader_shape.h"
#include "slang_component_type.h"

namespace gdslang {

class SlangModule final : public SlangComponentType {
	GDCLASS(SlangModule, SlangComponentType)

protected:
	static void _bind_methods();

public:
	slang::IModule* get_module() const;
	slang::IModule** get_write_ref();

	slang::IComponentType* get_component_type() const override;

	godot::String get_file_path() const;

	godot::PackedStringArray get_dependency_files() const;

	godot::Error _compile_kernels(godot::TypedArray<godot::Ref<ComputeShaderKernel>>& out_kernels, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape, const godot::PackedStringArray& additional_entry_points = godot::PackedStringArray{});
	godot::Ref<ComputeShaderFile> compile_shader(const godot::PackedStringArray& additional_entry_points);

	int64_t get_defined_entry_point_count() const;
	godot::Ref<SlangEntryPoint> get_defined_entry_point(int64_t index) const;
	godot::Ref<SlangEntryPoint> find_entry_point(const godot::String& name) const;
	godot::Ref<SlangEntryPoint> find_and_check_entry_point(const godot::String& name, godot::RenderingDevice::ShaderStage shader_stage) const;

private:
	Slang::ComPtr<slang::IModule> module{};

	godot::Ref<ComputeShaderKernel> _compile_kernel(slang::IEntryPoint* entry_point, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape);
};

}
