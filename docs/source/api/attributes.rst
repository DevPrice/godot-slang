Attributes
==========

The included ``godot`` Slang module exposes many useful attributes that effect the behavior of shaders within Godot.

----

.. _gd_ClassAttribute:

Class
---------------------

Indicates that a struct is represented by a global Godot class. This will, for example, expose fields of the
type as the corresponding Godot class in the property inspector.

See the `@export_custom documentation <https://docs.godotengine.org/en/stable/tutorials/scripting/gdscript/gdscript_exports.html#export-custom>`__ for more information.

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

In the below example, you would set the value of ``my_parameter.internal_name`` in godot via:

.. tabs::

 .. code-tab:: gdscript

    var task: ComputeShaderTask = get_task()
    task.set_shader_parameter("exposed_parameter/exposed_name")

 .. code-tab:: hlsl

    struct MyStruct {
        [gd::Class("exposed_name")]
        float internal_name;
    };

    [gd::Class("exposed_parameter")]
    uniform MyStruct my_parameter;

.. code-block:: hlsl
    struct MyStruct {
        [gd::Class("exposed_name")]
        float internal_name;
    };

    [gd::Class("exposed_parameter")]
    uniform MyStruct my_parameter;

.. _gd_ExportAttribute:

gd::Export
---------------------

Indicates that a variable should be exported within Godot. This will expose it within a ``ComputeShaderTask``'s property inspector and allow its value to be serialized with that task.

**Target:** ``Var``

**Example:**

.. code-block:: hlsl
    [gd::Export]
    uniform float my_parameter;

.. _gd_PropertyHintAttribute:

PropertyHint
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
