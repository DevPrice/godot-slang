@tool
class_name ComputeShaderEffect extends CompositorEffect

@export var compute_shader: ComputeShaderFile:
	set(value):
		compute_shader = value
		RenderingServer.call_on_render_thread.call_deferred(reload_shader)

var _context: StringName = &"compositor_effect"

var rd: RenderingDevice

var _linear_sampler: RID
var _nearest_sampler: RID

var _task: ComputeShaderTask

func _init() -> void:
	RenderingServer.call_on_render_thread.call_deferred(_initialize_compute)

func _notification(what) -> void:
	if what == NOTIFICATION_PREDELETE:
		# When this is called it should be safe to clean up our shader.
		# If not we'll crash anyway because we can no longer call our _render_callback.
		if _linear_sampler.is_valid():
			rd.free_rid(_linear_sampler)
		if _nearest_sampler.is_valid():
			rd.free_rid(_nearest_sampler)

func _initialize_compute() -> void:
	rd = RenderingServer.get_rendering_device()
	if not rd:
		printerr("Failed to obtain rendering device!")
		return

	reload_shader()

func reload_shader() -> void:
	if not rd: return
	if not compute_shader:
		printerr("No shader file specified!")
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

func _create_sampler(filter: RenderingDevice.SamplerFilter, repeat_mode: RenderingDevice.SamplerRepeatMode) -> RID:
	var sampler_state: RDSamplerState = RDSamplerState.new()
	sampler_state.min_filter = filter
	sampler_state.mag_filter = filter
	sampler_state.repeat_u = repeat_mode
	sampler_state.repeat_v = repeat_mode
	return rd.sampler_create(sampler_state)

func _get_linear_sampler() -> RID:
	if not _linear_sampler.is_valid():
		_linear_sampler = _create_sampler(RenderingDevice.SAMPLER_FILTER_LINEAR, RenderingDevice.SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE)
	return _linear_sampler

func _get_nearest_sampler() -> RID:
	if not _nearest_sampler.is_valid():
		_nearest_sampler = _create_sampler(RenderingDevice.SAMPLER_FILTER_NEAREST, RenderingDevice.SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE)
	return _nearest_sampler

func _render_callback(p_effect_callback_type: int, p_render_data: RenderData) -> void:
	if rd and _task and p_effect_callback_type == effect_callback_type:
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
					_task.set_shader_parameter(
						"scene_color",
						_create_image(kernel.parameters.scene_color.binding_index, render_scene_buffers.get_color_layer(view)),
					)
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

func _create_texture_sampler(binding: int, id: RID, use_linear: bool = true) -> RDUniform:
	var sampler := _get_linear_sampler() if use_linear else _get_nearest_sampler()
	return _create_uniform(binding, RenderingDevice.UniformType.UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, [sampler, id])

static func _create_ssbo(binding: int, id: RID) -> RDUniform:
	return _create_uniform(binding, RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER, [id])

#endregion

#region virtual functions

func _get_uniforms(_render_data: RenderData, _view: int) -> Array[RDUniform]:
	## TEMP
	if true:
		var render_scene_buffers: RenderSceneBuffersRD = _render_data.get_render_scene_buffers()
		var uniforms: Array[RDUniform] = []
		if compute_shader.kernels[0].parameters.has("scene_color"):
			uniforms.push_back(_create_image(compute_shader.kernels[0].parameters.scene_color.binding_index, render_scene_buffers.get_color_layer(_view)))
		return uniforms
	## ####
	return []

#endregion
