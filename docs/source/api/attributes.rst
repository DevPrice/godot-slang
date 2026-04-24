Attributes
==========

The included ``godot`` Slang module exposes many useful attributes that effect the behavior of shaders within Godot.

----

.. _gd_ClassAttribute:

gd::Class
---------------------

Indicates that a struct is represented by a global Godot class. This will, for example, expose fields of the
type as the corresponding Godot class in the property inspector.

**Target:** ``Var``

**Fields:**

.. list-table::
   :widths: 20 20 60
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - ``class_name``
     - ``String``
     - The Godot class name to associate with this struct. Must be the name of a `named class <https://docs.godotengine.org/en/4.4/tutorials/scripting/gdscript/gdscript_basics.html#registering-named-classes>`_

**Example:**

.. code-block:: hlsl

    [gd::Class("MyGDScriptClass")]
    struct MyStruct {
        float some_value;
    };

    // will appear as an instance of MyGDScriptClass in the inspector
    uniform MyStruct my_struct;

.. _gd_ColorAttribute:

gd::Color
---------------------

Indicates that a variable should be treated as sRGB color, rather than linear color.
When applied to a ``float3`` or ``float4``, the value will be converted from sRGB to linear color when binding to the shader.
When applied to a texture, a value passed as a ``Texture2D`` will be read as sRGB.

**Target:** ``Var``

**Example:**

.. code-block:: hlsl

    [gd::Color]
    uniform float3 my_color;

    [gd::Color]
    uniform float4 my_color_with_alpha;

    [gd::Color]
    uniform Texture<float4> albedo;

.. _gd_DefaultBlackAttribute:

gd::DefaultBlack
---------------------

When used within a ``ComputeShaderTask``, will bind a 4x4 black texture if no texture is provided.

**Target:** ``Var``

**Example:**

.. code-block:: hlsl

    [gd::DefaultBlack]
    uniform Texture2D<float4> texture_param;

.. _gd_DefaultWhiteAttribute:

gd::DefaultWhite
---------------------

When used within a ``ComputeShaderTask``, will bind a 4x4 white texture if no texture is provided.

**Target:** ``Var``

**Example:**

.. code-block:: hlsl

    [gd::DefaultWhite]
    uniform Texture2D<float4> texture_param;

.. _gd_ExportAttribute:

gd::Export
---------------------

Indicates that a variable should be exported within Godot. This will expose it within a ``ComputeShaderTask``'s property inspector and allow its value to be serialized with that task.

**Target:** ``Var``

**Example:**

.. code-block:: hlsl

    [gd::Export]
    uniform float my_parameter;

.. _gd_ExportParamAttribute:

gd::ExportParam
---------------------

Indicates that a variable should be exported within Godot. This is the same as :ref:`gd_ExportAttribute` except it may be applied to function parameters.
Is only valid on parameters of an entry-point function parameter.

**Target:** ``Param``

**Example:**

.. code-block:: hlsl

    [shader("compute")]
    [numthreads(8, 8, 1)]
    void compute_main(uint3 threadId: SV_DispatchThreadID, [gd::ExportParam] float my_parameter) {
        // ...
    }

.. _gd_FrameIDAttribute:

gd::FrameID
---------------------

When used within a ``ComputeShaderTask``, will automatically bind the current frame (fetched via `get_frames_drawn <https://docs.godotengine.org/en/4.4/classes/class_engine.html#class-engine-method-get-frames-drawn>`_).

**Target:** ``Var``

**Example:**

.. code-block:: hlsl

    [gd::FrameID]
    uniform int frame_id;

.. _gd_GlobalParamAttribute:

gd::GlobalParam
---------------------

When used within a ``ComputeShaderTask``, will bind the value of the corresponding `global shader parameter <https://godotengine.org/article/godot-40-gets-global-and-instance-shader-uniforms/#global-uniforms>`_ if no value is otherwise provided.

**Target:** ``Var``

**Fields:**

.. list-table::
   :widths: 20 20 60
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - ``name``
     - ``String``
     - The name of the global shader parameter to bind.

**Example:**

.. code-block:: hlsl

    [gd::GlobalParam("my_noise_texture")]
    uniform Texture2D<float4> noise;

.. _gd_KernelGroupAttribute:

gd::KernelGroup
---------------------

Associates an entry-point with the specified group name. Allows a group of entry-points to be conveniently dispatched together.
Currently, an entry-point may be associated with at most one kernel group.

**Target:** ``Function``

**Fields:**

.. list-table::
   :widths: 20 20 60
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - ``group_name``
     - ``String``
     - The name of the kernel group.

**Example:**

.. tabs::

 .. code-tab:: hlsl

    [shader("compute")]
    [numthreads(8, 8, 1)]
    [gd::KernelGroup("my_group")]
    void first(uint3 threadId: SV_DispatchThreadID) {
        // ...
    }

    [shader("compute")]
    [numthreads(8, 8, 1)]
    [gd::KernelGroup("my_group")]
    void second(uint3 threadId: SV_DispatchThreadID) {
        // ...
    }

 .. code-tab:: gdscript

    var task: ComputeShaderTask = get_task()
    task.dispatch_group("my_group", thread_groups)

.. _gd_MousePositionAttribute:

gd::MousePosition
---------------------

When used within a ``ComputeShaderTask``, will automatically bind the mouse position within the root window.

**Target:** ``Var``

**Example:**

.. code-block:: hlsl

    [gd::MousePosition]
    uniform int2 mouse_position;

.. _gd_NameAttribute:

gd::Name
---------------------

Indicates that a variable should use the specified name within Godot.
The specified name will be emitted in the reflection metadata instead of the name declared in the shader code.

**Target:** ``Var``

**Fields:**

.. list-table::
   :widths: 20 20 60
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - ``name``
     - ``String``
     - The identifier/name to use for this variable within Godot.

**Example:**

.. tabs::

 .. code-tab:: hlsl

    struct MyStruct {
        [gd::Name("exposed_name")]
        float internal_name;
    };

    [gd::Name("exposed_parameter")]
    uniform MyStruct my_parameter;

 .. code-tab:: gdscript

    var task: ComputeShaderTask = get_task()
    task.set_shader_parameter("exposed_parameter/exposed_name", 1234.0)

.. _gd_PropertyHintAttribute:

gd::PropertyHint
---------------------

Sets the property hint and hint string for an exported parameter, controlling
how Godot displays it in the property inspector.

See the `@export_custom documentation <https://docs.godotengine.org/en/stable/tutorials/scripting/gdscript/gdscript_exports.html#export-custom>`__ for more information.

**Target:** ``Var``

**Fields:**

.. list-table::
   :widths: 20 20 60
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - ``property_hint``
     - ``gd::PropertyHint``
     - The hint enum value (e.g. ``PropertyHint.Range``, ``PropertyHint.Enum``).
   * - ``hint_string``
     - ``String``
     - The hint string passed to Godot (e.g. ``"0,1,0.01"`` for a range).

**Example:**

.. code-block:: hlsl

    [gd::PropertyHint(PropertyHint.Range, "0,1,0.01")]
    uniform float my_param;

.. _gd_SamplerAttribute:

gd::Sampler
---------------------

When used within a ``ComputeShaderTask``, will bind a cached sampler with the specified filter and repeat mode if no sampler is provided.

**Target:** ``Var``

**Fields:**

.. list-table::
   :widths: 20 20 60
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - ``filter``
     - ``gd::SamplerFilter``
     - The filter to use for this sampler (linear or nearest).
   * - ``repeat_mode``
     - ``gd::SamplerRepeatMode``
     - The repeat mode to use for this sampler.

**Example:**

.. code-block:: hlsl

    [gd::Sampler(gd::SamplerFilter.LINEAR, gd::SamplerRepeatMode.REPEAT)]
    uniform SamplerState sampler_state;

.. _gd_TimeAttribute:

gd::Time
---------------------

When used within a ``ComputeShaderTask``, will automatically bind the current time in seconds (fetched via `get_ticks_msec <https://docs.godotengine.org/en/stable/classes/class_time.html#class-time-method-get-ticks-msec>`_).

**Target:** ``Var``

**Example:**

.. code-block:: hlsl

    [gd::Time]
    uniform float time;
