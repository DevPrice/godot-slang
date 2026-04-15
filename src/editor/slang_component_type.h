#pragma once

#include "slang.h"
#include "slang-com-ptr.h"

#include "godot_cpp/classes/ref_counted.hpp"

#include "binding_macros.h"
#include "compute_shader_shape.h"
#include "slang_blob.h"

class SlangComponentType : public godot::RefCounted {
	GDCLASS(SlangComponentType, RefCounted)

	GET_SET_PROPERTY(godot::String, diagnostic)

protected:
	static void _bind_methods();

public:
	virtual slang::IComponentType* get_component_type() const;
	slang::ProgramLayout* get_layout() const;

	virtual godot::Ref<StructTypeLayoutShape> get_params_shape() const;
	godot::Variant to_json() const;

	godot::Ref<SlangComponentType> link() const;
	godot::Ref<SlangBlob> compile_entry_point(int64_t entry_point_index = 0, int64_t target_index = 0) const;

	static godot::Ref<SlangComponentType> create(slang::IComponentType* component_type, const godot::String& diagnostic = "");

private:
	Slang::ComPtr<slang::IComponentType> component_type;
};
