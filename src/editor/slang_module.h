#pragma once

#include "slang.h"
#include "slang-com-ptr.h"

#include "godot_cpp/classes/ref_counted.hpp"

#include "compute_shader_file.h"
#include "compute_shader_kernel.h"
#include "compute_shader_shape.h"

namespace gdslang {

class SlangModule final : public godot::RefCounted {
	GDCLASS(SlangModule, RefCounted)

	GET_SET_PROPERTY(godot::String, diagnostic)

protected:
	static void _bind_methods();

public:
	slang::IModule* get_module() const;
	slang::IModule** get_write_ref();
	void set_module(slang::IModule* p_module);

	slang::ProgramLayout* get_layout() const;
	godot::String get_file_path() const;

	godot::PackedStringArray get_dependency_files() const;

	godot::TypedArray<godot::Ref<ComputeShaderKernel>> compile_kernels(const godot::PackedStringArray& additional_entry_points = godot::PackedStringArray{});
	godot::Error _compile_kernels(godot::TypedArray<godot::Ref<ComputeShaderKernel>>& out_kernels, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape, const godot::PackedStringArray& additional_entry_points = godot::PackedStringArray{});
	godot::Ref<ComputeShaderFile> compile_shader(const godot::PackedStringArray& additional_entry_points);

	int64_t get_defined_entry_point_count() const;
	godot::Ref<SlangEntryPoint> get_defined_entry_point(int64_t index) const;
	godot::Ref<SlangEntryPoint> find_entry_point(const godot::String& name) const;
	godot::Ref<SlangEntryPoint> find_and_check_entry_point(const godot::String& name, godot::RenderingDevice::ShaderStage shader_stage) const;

	godot::Ref<StructTypeLayoutShape> get_params_shape() const;
	godot::Variant to_json() const;

private:
	Slang::ComPtr<slang::IModule> module{};

	godot::Ref<ComputeShaderKernel> _compile_kernel(slang::IEntryPoint* entry_point, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape);
	static void _get_used_bindings_sets(slang::IMetadata* metadata, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape, const godot::Ref<ShaderTypeLayoutShape>& entry_point_params_shape, godot::Dictionary& out_used_binding_sets, int64_t kernel_space_offset, int64_t kernel_slot_offset);
	static bool _is_location_used(slang::IMetadata* metadata, int64_t space, int64_t binding_index);
};

}
