extends Node

## Draws the raster pass of a SlangShaderFile into a texture.
##
## SlangShaderFile only hands out compiled SPIR-V, exactly like Godot's own .glsl
## import does, so the framebuffer, pipeline and draw list are all built by hand here.

@export var shader: SlangShaderFile
@export var target: TextureRect
@export var size := Vector2i(512, 512)
@export var auto_size := true
@export var clear_color := Color(0.05, 0.05, 0.08)

var _rendering_device: RenderingDevice
var _shader := RID()
var _pipeline := RID()
var _texture := RID()
var _framebuffer := RID()
var _texture_rd := Texture2DRD.new()
var _program: SlangShaderProgram
var _can_draw := false
## The size the current texture and framebuffer were built for.
var _drawn_size := Vector2i.ZERO

func _ready() -> void:
	if shader == null:
		push_error("No shader assigned!")
		return
	_program = _find_raster_program()
	if _program == null:
		push_error("'%s' has no raster passes!" % shader.resource_path)
		return
	var compile_error: String = _program.get_compile_error()
	if not compile_error.is_empty():
		push_error("'%s' failed to compile:\n%s" % [shader.resource_path, compile_error])
		return
	if target != null:
		target.texture = _texture_rd
	_can_draw = true

## A shader file lists compute kernels and raster passes together in `programs`,
## so the pass is the program built from both a vertex and a fragment stage.
func _find_raster_program() -> SlangShaderProgram:
	for program: SlangShaderProgram in shader.programs:
		if (program.has_stage(RenderingDevice.SHADER_STAGE_VERTEX)
				and program.has_stage(RenderingDevice.SHADER_STAGE_FRAGMENT)):
			return program
	return null

func _exit_tree() -> void:
	RenderingServer.call_on_render_thread(_free_resources)

func _process(_delta: float) -> void:
	if not _can_draw:
		return
	if auto_size and target != null:
		var target_size := Vector2i(target.size)
		if target_size.x > 0 and target_size.y > 0:
			size = target_size
	# only rebuild when the size actually changed, so the draw resources aren't
	# recreated (and leaked) every frame
	if size != _drawn_size:
		_drawn_size = size
		RenderingServer.call_on_render_thread(_draw.bind(size))

func _draw(draw_size: Vector2i) -> void:
	_rendering_device = RenderingServer.get_rendering_device()

	# the texture and framebuffer are the only size-dependent resources, so the old
	# pair is released here and the shader and pipeline outlive a resize
	_free_framebuffer()

	var texture_format := RDTextureFormat.new()
	texture_format.width = draw_size.x
	texture_format.height = draw_size.y
	texture_format.format = RenderingDevice.DATA_FORMAT_R8G8B8A8_UNORM
	texture_format.texture_type = RenderingDevice.TEXTURE_TYPE_2D
	texture_format.usage_bits = (RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT
			| RenderingDevice.TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)
	_texture = _rendering_device.texture_create(texture_format, RDTextureView.new())
	_framebuffer = _rendering_device.framebuffer_create([_texture])

	if not _pipeline.is_valid():
		_create_pipeline()

	var draw_list := _rendering_device.draw_list_begin(
			_framebuffer,
			RenderingDevice.DRAW_CLEAR_COLOR_ALL,
			PackedColorArray([clear_color]))
	_rendering_device.draw_list_bind_render_pipeline(draw_list, _pipeline)
	_rendering_device.draw_list_draw(draw_list, false, 1, 3)
	_rendering_device.draw_list_end()

	_texture_rd.texture_rd_rid = _texture

## Builds the shader and pipeline. A resize doesn't invalidate either: the pipeline is
## created against the framebuffer *format*, which only depends on the attachment
## formats and sample count, not on the framebuffer's dimensions.
func _create_pipeline() -> void:
	_shader = _rendering_device.shader_create_from_spirv(_program.spirv, _program.program_name)

	var blend_state := RDPipelineColorBlendState.new()
	blend_state.attachments = [RDPipelineColorBlendStateAttachment.new()]
	_pipeline = _rendering_device.render_pipeline_create(
			_shader,
			_rendering_device.framebuffer_get_format(_framebuffer),
			# no vertex format: the vertex shader builds the triangle from SV_VertexID
			RenderingDevice.INVALID_FORMAT_ID,
			RenderingDevice.RENDER_PRIMITIVE_TRIANGLES,
			RDPipelineRasterizationState.new(),
			RDPipelineMultisampleState.new(),
			RDPipelineDepthStencilState.new(),
			blend_state)

func _free_framebuffer() -> void:
	if _rendering_device == null:
		return
	_texture_rd.texture_rd_rid = RID()
	for rid: RID in [_framebuffer, _texture]:
		if rid.is_valid():
			_rendering_device.free_rid(rid)
	_framebuffer = RID()
	_texture = RID()

func _free_resources() -> void:
	if _rendering_device == null:
		return
	_free_framebuffer()
	for rid: RID in [_pipeline, _shader]:
		if rid.is_valid():
			_rendering_device.free_rid(rid)
	_pipeline = RID()
	_shader = RID()
