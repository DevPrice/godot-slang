#pragma once

#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/typed_array.hpp"
#include "memory"

#include "binding_macros.h"
#include "compute_shader_cursor.h"
#include "compute_shader_file.h"
#include "compute_shader_shape.h"

using namespace godot;

class ComputeShaderTask : public Resource {
	GDCLASS(ComputeShaderTask, Resource);

	GET_SET_PROPERTY(Ref<ComputeShaderFile>, shader)
	GET_SET_OBJECT_PTR(RenderingDevice, rendering_device)

protected:
	static void _bind_methods();

public:
	ComputeShaderTask();

	[[nodiscard]] TypedArray<ComputeShaderKernel> get_kernels() const;

	[[nodiscard]] Variant get_shader_parameter(const StringName& param) const;
	void set_shader_parameter(const StringName& param, const Variant& value);
	void clear_shader_parameters();

	void dispatch_all(Vector3i thread_groups, const Object* context = nullptr);
	void dispatch(const StringName& kernel_name, Vector3i thread_groups, const Object* context = nullptr);
	void dispatch_at(int64_t kernel_index, Vector3i thread_groups, const Object* context = nullptr);
	void dispatch_group(const StringName& group_name, Vector3i thread_groups, const Object* context = nullptr);

	TypedArray<RID> get_rids(const StringName& param) const;
	PackedByteArray get_buffer_data(const StringName& param) const;
	Error get_buffer_data_async(const StringName& param, const Callable& callback) const;

	[[nodiscard]] Dictionary get_shader_parameters() const;

	bool _set(const StringName& p_name, const Variant& p_value);
	bool _get(const StringName& p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo>* p_list) const;

	static void _get_property_list(List<PropertyInfo>* p_list, const String& prefix, const Dictionary& properties);
	static bool _can_show_property_info(const PropertyInfo& property_info);
	[[nodiscard]] bool _property_can_revert(const StringName& p_name) const;
	bool _property_get_revert(const StringName& p_name, Variant& r_property) const;
	bool _property_get_reflection(const StringName& p_name, FieldShape& r_reflection) const;

private:
	struct KernelData {
		Dictionary parameters{};
		UniqueRID<RenderingDevice> shader_rid{};
		UniqueRID<RenderingDevice> pipeline_rid{};
		std::unique_ptr<ComputeShaderObject> shader_object;
	};
	Dictionary _shader_parameters{};
	std::vector<std::unique_ptr<KernelData>> _kernel_data{};

	std::unique_ptr<SamplerCache> _sampler_cache;
	std::unique_ptr<ComputeShaderObject> _shader_object;

	void _reset();
	void _shader_changed();

	RenderingDevice* _get_active_rendering_device() const;
	KernelData* _get_or_create_kernel(int64_t kernel_index);

	void _dispatch(int64_t kernel_index, Vector3i thread_groups, const Object* context = nullptr);
};
