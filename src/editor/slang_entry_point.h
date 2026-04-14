#pragma once

#include "slang-com-ptr.h"
#include "slang.h"

#include "godot_cpp/classes/ref_counted.hpp"

class SlangEntryPoint final : public godot::RefCounted {
	GDCLASS(SlangEntryPoint, RefCounted)

protected:
	static void _bind_methods();

public:
	slang::IEntryPoint* get_entry_point() const;
	slang::IEntryPoint** write_ref();

	godot::String get_name() const;

private:
	Slang::ComPtr<slang::IEntryPoint> entry_point{};
	godot::PackedByteArray compiled_bytes{};
	godot::String diagnostic{};
};
