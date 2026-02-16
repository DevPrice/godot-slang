#include "attributes.h"

#include <memory>

AttributeRegistry::AttributeRegistry() {
    register_write_handler(GodotAttributes::color(), [](const Dictionary&, Variant& value) {
        if (value.get_type() == Variant::COLOR) {
            value = static_cast<Color>(value).srgb_to_linear();
        }
        if (value.get_type() == Variant::PACKED_COLOR_ARRAY) {
            PackedColorArray converted = value.duplicate();
            for (Color& color : converted) {
                color = color.srgb_to_linear();
            }
            value = converted;
        }
    });
}

void AttributeRegistry::register_write_handler(const StringName& attribute_name, const WriteHandler& handler) {
    write_handlers.emplace(attribute_name, handler);
}

void AttributeRegistry::register_write_handler(const StringName& attribute_name, const Callable& handler) {
    const WriteHandler write_handler = [handler](const Dictionary& arguments, Variant& value) {
        // ReSharper disable once CppExpressionWithoutSideEffects
        value = handler.call(arguments, value);
    };
    register_write_handler(attribute_name, write_handler);
}

AttributeRegistry::WriteHandler* AttributeRegistry::get_write_handler(const StringName& attribute_name) {
    const auto it = write_handlers.find(attribute_name);
    return it != write_handlers.end() ? &it->second : nullptr;
}

AttributeRegistry* AttributeRegistry::get_instance() {
    static auto instance = std::make_unique<AttributeRegistry>();
    return instance.get();
}
