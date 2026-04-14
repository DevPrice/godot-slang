#pragma once

#include "slang.h"
#include "slang-com-ptr.h"

#include "godot_cpp/classes/ref_counted.hpp"

class SlangComponentType : public godot::RefCounted {
	GDCLASS(SlangComponentType, RefCounted)

protected:
	static void _bind_methods();

	virtual slang::IComponentType* get_component_type() const = 0;
};