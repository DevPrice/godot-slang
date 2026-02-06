#pragma once

#include "godot_cpp/core/property_info.hpp"

using namespace godot;

#define ENUM_HINT_STRING(clazz, enum_name) Enums::get_enum_hint_string(clazz::get_class_static(), GetTypeInfo<clazz::enum_name>().get_class_info().class_name)

class Enums {
public:
    static PropertyInfo get_enum_property_info(const StringName& p_class, const StringName& p_enum, const StringName& p_name);
    static Dictionary get_enum_values(const StringName& p_class, const StringName& p_enum);
    static String get_enum_hint_string(const StringName& p_class, const StringName& p_enum);
    static String get_enum_hint_string(const Dictionary& p_enum_values);

private:
    static String _get_enum_name(const String& p_enum);
};
