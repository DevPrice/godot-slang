#pragma once

#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/typed_array.hpp"

#include "binding_macros.h"
#include "compute_shader_kernel.h"

using namespace godot;

class RDBuffer : public RefCounted {
	GDCLASS(RDBuffer, RefCounted);

	GET_SET_PROPERTY(RID, rid)
	GET_SET_PROPERTY(PackedByteArray, buffer)
	GET_SET_PROPERTY(int64_t, stride)
	GET_SET_PROPERTY(bool, is_ssbo)

protected:
	static void _bind_methods();

public:
	RDBuffer() = default;
	~RDBuffer() override;

	void write(int64_t offset, int64_t size, const Variant& data);
	void set_size(int64_t size);
	void flush();
	[[nodiscard]] RenderingDevice::UniformType get_uniform_type() const;
	static Ref<RDBuffer> ref(const RID& buffer_rid, bool is_ssbo);

private:
	bool is_ref = false;

	template <typename T>
	void buffer_copy(const T source_data, const int64_t offset, const int64_t size) {
		using ElementType = std::remove_pointer_t<decltype(source_data.ptr())>;
		const int64_t data_size_bytes = source_data.size() * sizeof(ElementType);
		const int64_t write_size = get_is_ssbo() ? data_size_bytes : Math::min(size, data_size_bytes);
		memcpy(buffer.ptrw() + offset, source_data.ptr(), write_size);
	}
};

class ComputeShaderTask : public RefCounted {
	GDCLASS(ComputeShaderTask, RefCounted);

	GET_SET_PROPERTY(TypedArray<ComputeShaderKernel>, kernels)

protected:
	static void _bind_methods();

public:
	ComputeShaderTask();
	~ComputeShaderTask() override;

	[[nodiscard]] Variant get_shader_parameter(const StringName& param) const;
	void set_shader_parameter(const StringName& param, const Variant& value);

	void dispatch_all(Vector3i thread_groups);
	void dispatch(const StringName& kernel_name, Vector3i thread_groups);
	void dispatch_at(int64_t kernel_index, Vector3i thread_groups);

	[[nodiscard]] Dictionary get_shader_parameters() const;

private:
	Dictionary _shader_parameters{};
	Dictionary _kernel_shaders{};
	Dictionary _kernel_pipelines{};
	Dictionary _buffers{};
	TypedArray<RID> _linear_sampler_cache{};
	TypedArray<RID> _nearest_sampler_cache{};

	void _reset();

	RID _get_shader_rid(int64_t kernel_index, RenderingDevice* rd);
	RID _get_shader_pipeline_rid(int64_t kernel_index, RenderingDevice* rd);

	[[nodiscard]] RID _get_sampler(RenderingDevice::SamplerFilter filter, RenderingDevice::SamplerRepeatMode repeat_mode) const;
	[[nodiscard]] Variant _get_default_uniform(RenderingDevice::UniformType type, Dictionary user_attributes) const;
	Ref<RDBuffer> _get_buffer(int32_t binding, int32_t set, bool is_ssbo);
	void _set_buffer(int32_t binding, int32_t set, const RID& buffer_rid, bool is_ssbo);
	void _update_buffers(int64_t kernel_index);
	void _bind_uniform_sets(int64_t kernel_index, int64_t compute_list, RenderingDevice* rd);
	void _dispatch(int64_t kernel_index, Vector3i thread_groups);
};
