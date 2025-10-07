#pragma once

#include "godot_cpp/classes/ref_counted.hpp"

#include "binding_macros.h"

using namespace godot;

class RDBuffer : public RefCounted {
    GDCLASS(RDBuffer, RefCounted);

    GET_SET_PROPERTY(RID, rid)
    GET_SET_PROPERTY(PackedByteArray, buffer)
    GET_SET_PROPERTY(int64_t, stride)
    GET_SET_PROPERTY(int64_t, alignment)
    GET_SET_PROPERTY(bool, is_fixed_size)

protected:
    static void _bind_methods();

public:
    RDBuffer();
    ~RDBuffer() override;

    void write(int64_t offset, int64_t size, const Variant& data);
    void set_size(int64_t size);
    void flush();
    [[nodiscard]] RenderingDevice::UniformType get_uniform_type() const;

    static Ref<RDBuffer> ref(const RID& buffer_rid);
    static void write(PackedByteArray& destination, int64_t offset, int64_t size, const Variant& data);

private:
    bool is_ref = false;
    int64_t remote_size = 0;
    int64_t dirty_start = 0;
    int64_t dirty_end = 0;

    template <typename T>
    void buffer_copy(const T source_data, const int64_t offset, const int64_t size) {
        using ElementType = std::remove_pointer_t<decltype(source_data.ptr())>;
        const int64_t data_size_bytes = source_data.size() * sizeof(ElementType);
        const bool is_fixed_size = get_is_fixed_size();
        const int64_t write_size = !is_fixed_size ? data_size_bytes : Math::min(size, data_size_bytes);
        if (!is_fixed_size && buffer.size() < write_size + offset) {
            buffer.resize(write_size + offset);
        }
        memcpy(buffer.ptrw() + offset, source_data.ptr(), write_size);
        dirty_start = Math::min(offset, dirty_start);
        dirty_end = Math::max(offset + write_size, dirty_end);
    }
};