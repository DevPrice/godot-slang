#pragma once

#include "slang-com-ptr.h"
#include "slang.h"

#include "godot_cpp/classes/ref_counted.hpp"

#include "slang_component_type.h"

class SlangEntryPoint final : public SlangComponentType {
	GDCLASS(SlangEntryPoint, SlangComponentType)

protected:
	static void _bind_methods();

public:
	slang::IEntryPoint* get_entry_point() const;
	slang::IEntryPoint** write_ref();

	slang::IComponentType* get_component_type() const override;

	godot::String get_name() const;

private:
	Slang::ComPtr<slang::IEntryPoint> entry_point{};
	godot::PackedByteArray compiled_bytes{};
	godot::String diagnostic{};
};
