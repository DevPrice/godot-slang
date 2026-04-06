Attributes
==========

The included `godot` Slang module exposes many useful attributes that effect the behavior of shaders within Godot.

----

.. _gd_PropertyHintAttribute:

``[gd::PropertyHint]``
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