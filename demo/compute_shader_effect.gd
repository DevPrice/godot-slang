@tool
class_name ComputeShaderEffect extends CompositorEffect

@export var compute_shader: ComputeShaderFile:
	set(value):
		compute_shader = value
		RenderingServer.call_on_render_thread.call_deferred(reload_shader)

var _task: ComputeShaderTask

func _init() -> void:
	RenderingServer.call_on_render_thread.call_deferred(reload_shader)

func reload_shader() -> void:
	if not compute_shader:
		_task = null
		return

	_task = ComputeShaderTask.new()
	_task.kernels = compute_shader.kernels

	if Engine.is_editor_hint():
		Engine.get_singleton("EditorInterface").get_resource_filesystem().resources_reimported.connect(
			func (resources: PackedStringArray):
				if resources.has(compute_shader.resource_path):
					RenderingServer.call_on_render_thread(reload_shader),
			Object.CONNECT_ONE_SHOT,
		)

func _render_callback(p_effect_callback_type: int, p_render_data: RenderData) -> void:
	if _task and p_effect_callback_type == effect_callback_type:
		# Get our render scene buffers object, this gives us access to our render buffers.
		# Note that implementation differs per renderer hence the need for the cast.
		var render_scene_buffers: RenderSceneBuffersRD = p_render_data.get_render_scene_buffers()
		if render_scene_buffers:
			# Get our render size, this is the 3D render resolution
			var size := render_scene_buffers.get_internal_size()
			if size.x == 0 or size.y == 0:
				return

			# Loop through views just in case we're doing stereo rendering. No extra cost if this is mono.
			var view_count := render_scene_buffers.get_view_count()
			for i: int in range(_task.kernels.size()):
				var kernel := compute_shader.kernels[i]
				var local_size := kernel.thread_group_size
				var groups := Vector3i(
					(size.x - 1) / local_size.x + 1,
					(size.y - 1) / local_size.y + 1,
					local_size.z,
				)
				for view in range(view_count):
					for param: String in kernel.parameters:
						if kernel.parameters[param].user_attributes.has("gd_compositor_Size"):
							_task.set_shader_parameter(param, render_scene_buffers.get_internal_size())
						if kernel.parameters[param].user_attributes.has("gd_compositor_ColorTexture"):
							_task.set_shader_parameter(param, render_scene_buffers.get_color_layer(view))
						if kernel.parameters[param].user_attributes.has("gd_compositor_DepthTexture"):
							_task.set_shader_parameter(param, render_scene_buffers.get_depth_layer(view))
						if kernel.parameters[param].user_attributes.has("gd_compositor_VelocityTexture"):
							_task.set_shader_parameter(param, render_scene_buffers.get_velocity_layer(view))
						if kernel.parameters[param].user_attributes.has("gd_compositor_SceneBuffer"):
							var context: String = kernel.parameters[param].user_attributes.gd_compositor_SceneBuffer.context
							var name: String = kernel.parameters[param].user_attributes.gd_compositor_SceneBuffer.name
							_task.set_shader_parameter(param, render_scene_buffers.get_texture(context, name))
					_task.dispatch_at(i, groups)

#region utility functions

static func _get_normal_roughness_texture(render_scene_buffers: RenderSceneBuffersRD) -> RID:
	return render_scene_buffers.get_texture("forward_clustered", "normal_roughness")

static func _create_uniform(binding: int, uniform_type: RenderingDevice.UniformType, ids: Array[RID]) -> RDUniform:
	var uniform := RDUniform.new()
	uniform.binding = binding
	uniform.uniform_type = uniform_type
	for id in ids:
		uniform.add_id(id)
	return uniform

static func _create_uniform_buffer(binding: int, render_data: RenderData) -> RDUniform:
	return _create_uniform(binding, RenderingDevice.UNIFORM_TYPE_UNIFORM_BUFFER, [render_data.get_render_scene_data().get_uniform_buffer()])

static func _create_image(binding: int, id: RID) -> RDUniform:
	return _create_uniform(binding, RenderingDevice.UniformType.UNIFORM_TYPE_IMAGE, [id])

static func _create_ssbo(binding: int, id: RID) -> RDUniform:
	return _create_uniform(binding, RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER, [id])

#endregion
