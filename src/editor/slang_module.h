#pragma once

#include "slang.h"
#include "slang-com-ptr.h"

#include "slang_shader_file.h"
#include "slang_shader_program.h"
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

	godot::Error _compile_programs(godot::TypedArray<godot::Ref<SlangShaderProgram>>& out_programs, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape, const godot::PackedStringArray& additional_entry_points = godot::PackedStringArray{});
	godot::Ref<SlangShaderFile> compile_shader(const godot::PackedStringArray& additional_entry_points);

	// Returns the GDShader source for this module's [gd::Material] entry point, preceded by the
	// `shader_type` and `render_mode` its attributes ask for, or an empty string if the module
	// declares no material. Diagnostics are reported through `r_error`.
	//
	// The module must have been loaded in a session from SlangSession::create_material_session();
	// in any other session the emitted code is GLSL or SPIR-V, not GDShader.
	godot::String compile_material(godot::String& r_error);

	// True if this module declares a [gd::Material] entry point. Such a file can legitimately
	// produce no SPIR-V programs at all, which is otherwise an error.
	bool has_material_entry_point() const;

	int64_t get_defined_entry_point_count() const;
	godot::Ref<SlangEntryPoint> get_defined_entry_point(int64_t index) const;
	godot::Ref<SlangEntryPoint> find_entry_point(const godot::String& name) const;
	godot::Ref<SlangEntryPoint> find_and_check_entry_point(const godot::String& name, godot::RenderingDevice::ShaderStage shader_stage) const;

private:
	// Non-owning. `ISession::loadModule*` returns a borrowed pointer: the module is owned by the
	// `ISession` that loaded it (via its loaded-module map), and the returned pointer is not
	// ref-counted on our behalf. Holding it in a `ComPtr` would release a reference we never
	// acquired, freeing the module one release early and corrupting the heap when the session's
	// `ASTBuilder` is later torn down.
	//
	// This stays valid because `SlangComponentType::session` keeps the owning `SlangSession`
	// (and therefore the `ISession`) alive for at least as long as this object. Anything that
	// sets `module` must set `session` too.
	//
	// Note the asymmetry with `SlangEntryPoint::write_ref`: entry points come back through an
	// `IEntryPoint**` out-param, which *is* a +1 reference, so that one is correctly a `ComPtr`.
	slang::IModule* module{};

	godot::Ref<SlangShaderProgram> _compile_kernel(slang::IEntryPoint* entry_point, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape);
	godot::Ref<SlangShaderProgram> _compile_pass(slang::IEntryPoint* vertex_entry_point, slang::IEntryPoint* fragment_entry_point);

	static int64_t _raster_stages();
	static SlangStage _get_entry_point_stage(slang::IEntryPoint* entry_point);
	static slang::Attribute* _find_attribute(slang::FunctionReflection* function, const godot::StringName& attribute_name);
	static godot::String _get_string_argument(slang::Attribute* attribute, uint32_t argument_index, const godot::String& fallback);
	static bool _is_material_entry_point(slang::IEntryPoint* entry_point);
	static godot::Ref<SlangShaderProgram> _make_error_program(const godot::String& program_name, int64_t stages, godot::RenderingDevice::ShaderStage error_stage, const godot::String& compile_error);
};

}
