#pragma once

#include "godot_cpp/classes/ref_counted.hpp"

#include "binding_macros.h"
#include "slang.h"

class SlangBlob : public godot::RefCounted {
	GDCLASS(SlangBlob, RefCounted)

	GET_SET_PROPERTY(godot::PackedByteArray, buffer)
	GET_SET_PROPERTY(godot::String, diagnostic)

protected:
	static void _bind_methods();

public:
	static godot::Ref<SlangBlob> create(slang::IBlob* blob, slang::IBlob* diagnostic);
	static godot::String blob_to_string(slang::IBlob* blob);
	static godot::PackedByteArray blob_to_bytes(slang::IBlob* blob);
};
