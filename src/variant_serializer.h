#pragma once

#include <array>
#include <cstdint>
#include <variant>

#include "compute_shader_shape.h"
#include "godot_cpp/core/error_macros.hpp"

// TODO: Handle both std140 and std430 correctly
class VariantSerializer {

public:
    class Buffer {
    private:
        static constexpr size_t max_size = 64;

        struct InlineBuffer {
            std::array<uint8_t, max_size> data{};
            size_t size = 0;
            InlineBuffer() = default;
        };

        std::variant<InlineBuffer, PackedByteArray> buffer;

        Buffer();
        Buffer(const PackedByteArray& p_array);

        void set_inline_size(size_t size);

        friend class VariantSerializer;

    public:
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        Buffer(Buffer&&) noexcept = default;
        Buffer& operator=(Buffer&&) noexcept = default;

        [[nodiscard]] uint8_t* data();
        [[nodiscard]] const uint8_t* data() const;
        [[nodiscard]] size_t size() const;

        void copy(uint8_t* destination, size_t max_size) const;
        int64_t compare(const uint8_t* other, size_t max_size) const;
        PackedByteArray as_packed_byte_array() const;
    };

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

    explicit VariantSerializer(uint8_t* p_destination, const size_t p_max_size) : destination(p_destination), max_size(p_max_size) { }

    size_t write(const Variant& data, ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR);
    void align(size_t alignment);

public:
    static Buffer serialize(const Variant& data, ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR);
};
