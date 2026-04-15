#pragma once

#include "slang.h"
#include "slang-com-ptr.h"

#include "godot_cpp/classes/ref_counted.hpp"

#include "binding_macros.h"

class SlangComponentType : public godot::RefCounted {
	GDCLASS(SlangComponentType, RefCounted)

	GET_SET_PROPERTY(godot::String, diagnostic)

protected:
	static void _bind_methods();

public:
	virtual slang::IComponentType* get_component_type() const;

	godot::Ref<SlangComponentType> link() const;

	static godot::Ref<SlangComponentType> create(slang::IComponentType* component_type, const godot::String& diagnostic = "");

private:
	Slang::ComPtr<slang::IComponentType> component_type;
};
