extends Camera3D

@export var move_speed: float = 4.0
@export var mouse_look_sensitivity := Vector2(0.002, 0.002)

func _process(delta: float) -> void:
	var movement = Input.get_axis("move_left", "move_right") * basis.x + \
		Input.get_axis("move_down", "move_up") * basis.y + \
		Input.get_axis("move_forward", "move_backward") * basis.z
	position += delta * move_speed * movement.normalized()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_RIGHT:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED if event.pressed else Input.MOUSE_MODE_VISIBLE
	elif event is InputEventMouseMotion:
		if event.button_mask & MOUSE_BUTTON_MASK_RIGHT:
			var look_amount: Vector2 = event.relative * mouse_look_sensitivity
			rotation.y -= look_amount.x
			rotation.x = clamp(rotation.x - look_amount.y, -TAU / 4, TAU / 4)
		
