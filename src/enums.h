#pragma once

#include "godot_cpp/core/property_info.hpp"

#define ENUM_HINT_STRING(clazz, enum_name) Enums::get_enum_hint_string(clazz::get_class_static(), godot::GetTypeInfo<clazz::enum_name>().get_class_info().class_name)

class Enums {
public:
    static godot::PropertyInfo get_enum_property_info(const godot::StringName& p_class, const godot::StringName& p_enum, const godot::StringName& p_name);
    static godot::Dictionary get_enum_values(const godot::StringName& p_class, const godot::StringName& p_enum);
    static godot::String get_enum_hint_string(const godot::StringName& p_class, const godot::StringName& p_enum);
    static godot::String get_enum_hint_string(const godot::Dictionary& p_enum_values);

private:
    static godot::String _get_enum_name(const godot::String& p_enum);
};
