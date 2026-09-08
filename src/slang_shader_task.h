#pragma once

#include "godot_cpp/classes/mutex.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/typed_array.hpp"
#include "memory"

#include "binding_macros.h"
#include "compute_shader_cursor.h"
#include "slang_shader_file.h"
#include "compute_shader_shape.h"
#include "parameter_store.h"
#include "sampler_cache.h"

class SlangShaderTask : public godot::Resource {
	GDCLASS(SlangShaderTask, Resource);

	GET_SET_PROPERTY(godot::Ref<SlangShaderFile>, shader)
	GET_SET_OBJECT_PTR(godot::RenderingDevice, rendering_device)

protected:
	static void _bind_methods();

public:
	SlangShaderTask();

	[[nodiscard]] godot::TypedArray<SlangShaderProgram> get_programs() const;

	[[nodiscard]] godot::Variant get_shader_parameter(const godot::StringName& param) const;
	void set_shader_parameter(const godot::StringName& param, const godot::Variant& value);
	void clear_shader_parameters();

	[[nodiscard]] godot::Variant get_kernel_parameter(const godot::StringName& kernel, const godot::StringName& param) const;
	void set_kernel_parameter(const godot::StringName& kernel, const godot::StringName& param, const godot::Variant& value);

	void dispatch_all(godot::Vector3i thread_groups, const Object* context = nullptr);
	void dispatch(const godot::StringName& kernel_name, godot::Vector3i thread_groups, const Object* context = nullptr);
	void dispatch_at(int64_t kernel_index, godot::Vector3i thread_groups, const Object* context = nullptr);
	void dispatch_group(const godot::StringName& group_name, godot::Vector3i thread_groups, const Object* context = nullptr);

	godot::TypedArray<godot::RID> get_rids(const godot::StringName& param) const;
	godot::PackedByteArray get_buffer_data(const godot::StringName& param) const;
	godot::Error get_buffer_data_async(const godot::StringName& param, const godot::Callable& callback) const;

	[[nodiscard]] godot::Dictionary get_shader_parameters() const;
	[[nodiscard]] godot::Dictionary get_kernel_parameters(const godot::StringName& kernel_name) const;

	godot::TypedArray<godot::RID> get_kernel_rids(const godot::StringName& kernel, const godot::StringName& param) const;
	godot::PackedByteArray get_kernel_buffer_data(const godot::StringName& kernel, const godot::StringName& param) const;
	godot::Error get_kernel_buffer_data_async(const godot::StringName& kernel, const godot::StringName& param, const godot::Callable& callback) const;

	bool _set(const godot::StringName& p_name, const godot::Variant& p_value);
	bool _get(const godot::StringName& p_name, godot::Variant &r_ret) const;
	void _get_property_list(godot::List<godot::PropertyInfo>* p_list) const;

	static void _get_property_list(godot::List<godot::PropertyInfo>* p_list, const godot::String& prefix, const godot::Dictionary& properties);
	static bool _can_show_property_info(const godot::PropertyInfo& property_info);
	[[nodiscard]] bool _property_can_revert(const godot::StringName& p_name) const;
	bool _property_get_revert(const godot::StringName& p_name, godot::Variant& r_property) const;
	bool _property_get_reflection(const godot::StringName& p_name, FieldShape& r_reflection) const;

private:
	struct ProgramData {
		UniqueRID<godot::RenderingDevice> shader_rid{};
		UniqueRID<godot::RenderingDevice> pipeline_rid{};
		std::unique_ptr<ComputeShaderObject> shader_object{};
	};
	ParameterStore _shader_parameters{};
	godot::HashMap<godot::StringName, ParameterStore> _kernel_parameters{};
	std::vector<std::unique_ptr<ProgramData>> _program_data{};

	std::unique_ptr<SamplerCache> _sampler_cache;
	std::unique_ptr<ComputeShaderObject> _shader_object;

	godot::Ref<godot::Mutex> _mutex;

	void _reset();
	void _shader_changed();

	godot::RenderingDevice* _get_active_rendering_device() const;
	ProgramData* _get_or_create_program(int64_t kernel_index);
	ProgramData* _get_program_data(const godot::StringName& kernel_name) const;

	void _dispatch(int64_t kernel_index, godot::Vector3i thread_groups, const Object* context = nullptr);
	static void _write_assigned(ComputeShaderObject* object, const Object* context, const ParameterStore& store, const ParameterStore::DirtyPaths& dirty);
};
