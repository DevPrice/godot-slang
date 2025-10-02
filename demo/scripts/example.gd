extends Node3D

@export var world_env: WorldEnvironment

func _unhandled_input(event: InputEvent) -> void:
	var compositor := world_env.compositor
	if event.is_action_pressed("toggle_data_mosh"):
		var index := compositor.compositor_effects.find_custom(
			func (effect: CompositorEffect):
				return effect is ComputeShaderEffect and effect.compute_shader == preload("uid://drcr6fv2t323a")
		)
		if index >= 0:
			var data_mosh_effect: ComputeShaderEffect = compositor.compositor_effects[index]
			if data_mosh_effect:
				data_mosh_effect.enabled = not data_mosh_effect.enabled
				data_mosh_effect.queue_dispatch("capture_screen")
