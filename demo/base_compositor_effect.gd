@tool
class_name BaseCompositorEffect extends CompositorEffect

@export var compute_shader: ComputeShaderFile:
	set(value):
		compute_shader = value
		reload_shader()

var _context: StringName = &"compositor_effect"

var rd: RenderingDevice

var _shader: RID
var _pipeline: RID

var backbuffer_shader: RID
var backbuffer_pipeline: RID

var _linear_sampler: RID
var _nearest_sampler: RID

func _init() -> void:
	call_deferred("_post_init")

func _post_init() -> void:
	RenderingServer.call_on_render_thread(_initialize_compute)

func _notification(what) -> void:
	if what == NOTIFICATION_PREDELETE:
		# When this is called it should be safe to clean up our shader.
		# If not we'll crash anyway because we can no longer call our _render_callback.
		if _shader.is_valid():
			rd.free_rid(_shader)
		if backbuffer_shader.is_valid():
			rd.free_rid(backbuffer_shader)
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

	if Engine.is_editor_hint():
		Engine.get_singleton("EditorInterface").get_resource_filesystem().resources_reimported.connect(
			func (resources: PackedStringArray):
				if resources.has(compute_shader.resource_path):
					RenderingServer.call_on_render_thread(reload_shader),
			Object.CONNECT_ONE_SHOT,
		)

	var kernel := compute_shader.kernels[0] if not compute_shader.kernels.is_empty() else null
	if not kernel:
		return

	var shader_spirv := kernel.get_spirv()
	if not shader_spirv:
		printerr("Failed to create spirv from shader resource!")
		return

	# TODO: We should free the old RID, but that gives an error for some reason?
	_shader = rd.shader_create_from_spirv(shader_spirv)

	if _shader:
		_pipeline = rd.compute_pipeline_create(_shader)
	else:
		_pipeline = RID()

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
	if rd and _shader.is_valid() and _pipeline.is_valid() and p_effect_callback_type == effect_callback_type:
		# Get our render scene buffers object, this gives us access to our render buffers.
		# Note that implementation differs per renderer hence the need for the cast.
		var render_scene_buffers: RenderSceneBuffersRD = p_render_data.get_render_scene_buffers()
		if render_scene_buffers:
			# Get our render size, this is the 3D render resolution
			var size := render_scene_buffers.get_internal_size()
			if size.x == 0 or size.y == 0:
				return

			var local_size := _get_local_size()
			var groups := Vector3i(
				(size.x - 1) / local_size.x + 1,
				(size.y - 1) / local_size.y + 1,
				1,
			)

			# Loop through views just in case we're doing stereo rendering. No extra cost if this is mono.
			var view_count := render_scene_buffers.get_view_count()
			for view in range(view_count):
				_render_view(p_render_data, view, groups)

func _render_view(render_data: RenderData, view: int, groups: Vector3i) -> void:
	# Create a uniform set, this will be cached, the cache will be cleared if our viewports configuration is changed
	var uniforms := _get_uniforms(render_data, view)

	_render_shader(_pipeline, _shader, uniforms, PackedByteArray(), groups)

func _render_backbuffer(render_data: RenderData, view: int) -> void:
	var render_scene_buffers: RenderSceneBuffersRD = render_data.get_render_scene_buffers()
	var size := render_scene_buffers.get_internal_size()
	var groups := Vector3i(
		(size.x - 1) / 8 + 1,
		(size.y - 1) / 8 + 1,
		1,
	)
	var uniforms: Array[RDUniform] = [
		_create_uniform_buffer(0, render_data),
		_create_texture_sampler(1, render_scene_buffers.get_color_layer(view)),
		_create_image(2, _get_backbuffer_texture(render_data)),
	]

func _render_shader(pipeline: RID, shader: RID, uniforms: Array[RDUniform], push_constant: PackedByteArray, groups: Vector3i) -> void:
	var uniform_set := UniformSetCacheRD.get_cache(shader, 0, uniforms)
	var compute_list := rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(compute_list, pipeline)
	rd.compute_list_bind_uniform_set(compute_list, uniform_set, 0)
	if not push_constant.is_empty():
		rd.compute_list_set_push_constant(compute_list, push_constant, push_constant.size())
	rd.compute_list_dispatch(compute_list, groups.x, groups.y, groups.z)
	rd.compute_list_end()

#region utility functions

static func _get_normal_roughness_texture(render_scene_buffers: RenderSceneBuffersRD) -> RID:
	return render_scene_buffers.get_texture("forward_clustered", "normal_roughness")

func _get_backbuffer_texture(render_data: RenderData) -> RID:
	var render_scene_buffers: RenderSceneBuffersRD = render_data.get_render_scene_buffers()
	return render_scene_buffers.create_texture(
		_context,
		"backbuffer",
		RenderingDevice.DATA_FORMAT_R16G16B16A16_SNORM,
		RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice.TEXTURE_USAGE_STORAGE_BIT,
		RenderingDevice.TextureSamples.TEXTURE_SAMPLES_1,
		render_scene_buffers.get_internal_size(),
		1,
		1,
		false,
		false,
	)

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

func _get_shader_file() -> RDShaderFile:
	return null

func _get_local_size() -> Vector2i:
	return Vector2i(8, 8)

func _get_uniforms(_render_data: RenderData, _view: int) -> Array[RDUniform]:
	## TEMP
	if true:
		var render_scene_buffers: RenderSceneBuffersRD = _render_data.get_render_scene_buffers()
		return [
			_create_image(0, render_scene_buffers.get_color_layer(_view)),
		]
	## ####
	return []

#endregion
