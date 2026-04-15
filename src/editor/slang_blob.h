#pragma once

#include "godot_cpp/classes/ref_counted.hpp"

#include "binding_macros.h"

class SlangBlob : public godot::RefCounted {
	GDCLASS(SlangBlob, RefCounted)

	GET_SET_PROPERTY(godot::PackedByteArray, buffer)
	GET_SET_PROPERTY(godot::String, diagnostic)

protected:
	static void _bind_methods();
};
