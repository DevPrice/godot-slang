#pragma once

#include "slang.h"
#include "slang-com-ptr.h"

#include "godot_cpp/classes/ref_counted.hpp"

#include "compute_shader_shape.h"

namespace gdslang {

class SlangModule final : public godot::RefCounted {
	GDCLASS(SlangModule, RefCounted)

	GET_SET_PROPERTY(godot::String, diagnostic)

protected:
	static void _bind_methods();

public:
	slang::IModule* get_module() const;
	void set_module(slang::IModule* p_module);

	slang::ProgramLayout* get_layout() const;
	godot::String get_file_path() const;

private:
	Slang::ComPtr<slang::IModule> module{};
};

}
