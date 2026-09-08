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


func _ready() -> void:
	if shader == null:
		push_error("No shader assigned!")
		return
	if shader.passes.is_empty():
		push_error("'%s' has no raster passes!" % shader.resource_path)
		return
	var compile_error: String = shader.passes[0].get_compile_error()
	if not compile_error.is_empty():
		push_error("'%s' failed to compile:\n%s" % [shader.resource_path, compile_error])
		return
	if target != null:
		target.texture = _texture_rd
	RenderingServer.call_on_render_thread(_draw)

func _exit_tree() -> void:
	RenderingServer.call_on_render_thread(_free_resources)

func _process(_delta: float) -> void:
	if target and target.size and auto_size:
		size = target.size
		RenderingServer.call_on_render_thread(_draw)

func _draw() -> void:
	_rendering_device = RenderingServer.get_rendering_device()

	_shader = _rendering_device.shader_create_from_spirv(shader.get_pass_spirv(), shader.passes[0].kernel_name)

	var texture_format := RDTextureFormat.new()
	texture_format.width = size.x
	texture_format.height = size.y
	texture_format.format = RenderingDevice.DATA_FORMAT_R8G8B8A8_UNORM
	texture_format.texture_type = RenderingDevice.TEXTURE_TYPE_2D
	texture_format.usage_bits = (RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT
			| RenderingDevice.TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)
	_texture = _rendering_device.texture_create(texture_format, RDTextureView.new())
	_framebuffer = _rendering_device.framebuffer_create([_texture])

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

	var draw_list := _rendering_device.draw_list_begin(
			_framebuffer,
			RenderingDevice.DRAW_CLEAR_COLOR_ALL,
			PackedColorArray([clear_color]))
	_rendering_device.draw_list_bind_render_pipeline(draw_list, _pipeline)
	_rendering_device.draw_list_draw(draw_list, false, 1, 3)
	_rendering_device.draw_list_end()

	_texture_rd.texture_rd_rid = _texture


func _free_resources() -> void:
	if _rendering_device == null:
		return
	_texture_rd.texture_rd_rid = RID()
	for rid: RID in [_pipeline, _framebuffer, _texture, _shader]:
		if rid.is_valid():
			_rendering_device.free_rid(rid)
