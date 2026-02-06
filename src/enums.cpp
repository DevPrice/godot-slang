#include "enums.h"

#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/variant/utility_functions.hpp"

PropertyInfo Enums::get_enum_property_info(const StringName& p_class, const StringName& p_enum, const StringName& p_name) {
    return {Variant::Type::INT, p_name, PROPERTY_HINT_ENUM, get_enum_hint_string(p_class, p_enum)};
}

Dictionary Enums::get_enum_values(const StringName& p_class, const StringName& p_enum) {
    Dictionary enum_values{};
    for (const String& name : ClassDB::class_get_enum_constants(p_class, _get_enum_name(p_enum))) {
        enum_values.set(name, ClassDB::class_get_integer_constant(p_class, name));
    }
    return enum_values;
}

String Enums::get_enum_hint_string(const StringName& p_class, const StringName& p_enum) {
    PackedStringArray key_value_pairs{};
    for (const String& name : ClassDB::class_get_enum_constants(p_class, _get_enum_name(p_enum))) {
        key_value_pairs.push_back(String("%s:%s") % TypedArray<String> { name.capitalize(), String::num_int64(ClassDB::class_get_integer_constant(p_class, name)) });
    }
    return String(",").join(key_value_pairs);
}

String Enums::get_enum_hint_string(const Dictionary& p_enum_values) {
    PackedStringArray item_strings{};
    for (const String name: p_enum_values.keys()) {
        const int64_t enum_value = p_enum_values[name];
        item_strings.push_back(String("%s:%s") % TypedArray<String> { name.capitalize(), String::num_int64(enum_value) });
    }
    return String(",").join(item_strings);
}

String Enums::_get_enum_name(const String& p_enum) {
    int64_t i = p_enum.rfind(".");
    if (i >= 0) {
        return p_enum.substr(i + 1);
    }
    return p_enum;
}
