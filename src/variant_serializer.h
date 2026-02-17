#pragma once

#include <cstdint>

#include "compute_shader_shape.h"
#include "godot_cpp/core/error_macros.hpp"

class VariantSerializer {

private:
    uint8_t* destination{};
    size_t offset{};
    size_t max_size{};

    template<typename T>
    size_t write(T value) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        ERR_FAIL_COND_V(max_size - offset < sizeof(T), 0);
        memcpy(destination + offset, &value, sizeof(T));
        offset += sizeof(T);
        return sizeof(T);
    }

    void align(size_t alignment);

public:
    explicit VariantSerializer(uint8_t* p_destination, const size_t p_max_size) : destination(p_destination), max_size(p_max_size) { }

    size_t serialize(const Variant& data, ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR);
};
