#pragma once

#include <optional>

#include "slang.h"
#include "slang-com-ptr.h"

#include "godot_cpp/classes/ref_counted.hpp"

#include "binding_macros.h"
#include "slang_shader_program.h"
#include "compute_shader_shape.h"
#include "slang_blob.h"

namespace gdslang {
class SlangSession;
}

class SlangComponentType : public godot::RefCounted {
	GDCLASS(SlangComponentType, RefCounted)

	GET_SET_PROPERTY(godot::String, diagnostic)
	GET_SET_PROPERTY(godot::Ref<gdslang::SlangSession>, session)

protected:
	static void _bind_methods();

public:
	SlangComponentType() = default;
	virtual ~SlangComponentType() override;

	virtual slang::IComponentType* get_component_type() const;
	slang::ProgramLayout* get_layout() const;

	virtual godot::Ref<StructTypeLayoutShape> get_params_shape() const;
	godot::Variant to_json() const;

	godot::Ref<SlangComponentType> link() const;
	godot::Ref<SlangBlob> compile_entry_point(int64_t entry_point_index = 0, int64_t target_index = 0) const;

	godot::Ref<SlangShaderProgram> compile_kernel(const godot::Ref<ShaderTypeLayoutShape>& global_params_shape) const;
	godot::Ref<SlangShaderProgram> compile_pass() const;

	static std::optional<godot::RenderingDevice::ShaderStage> to_godot_shader_stage(SlangStage stage);
	static std::optional<SlangStage> to_slang_stage(godot::RenderingDevice::ShaderStage stage);

	static godot::Ref<SlangComponentType> create(slang::IComponentType* component_type, const godot::String& diagnostic = "");

private:
	Slang::ComPtr<slang::IComponentType> component_type;

	static void _get_used_bindings_sets(slang::IMetadata* metadata, const godot::Ref<ShaderTypeLayoutShape>& global_params_shape, const godot::Ref<ShaderTypeLayoutShape>& entry_point_params_shape, godot::Dictionary& out_used_binding_sets, int64_t kernel_space_offset, int64_t kernel_slot_offset);
	static bool _is_location_used(slang::IMetadata* metadata, int64_t space, int64_t binding_index);
};
