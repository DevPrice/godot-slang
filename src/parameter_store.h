#pragma once

#include <unordered_set>

#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/string_name.hpp"
#include "godot_cpp/variant/variant.hpp"

#include "variant_utils.h"

// Parameter values addressed by "/"-separated paths, alongside which paths have been
// assigned since they were last written.
class ParameterStore {

public:
	using DirtyPaths = std::unordered_set<godot::StringName, gdslang::GodotHasher>;

	// Replaces the value at `path` outright rather than merging into it, so a member left
	// out of the new value is gone from the store -- which is what lets the dispatch tell
	// "reseed this GPU-owned field" from "don't touch it".
	void set(const godot::StringName& path, const godot::Variant& value);
	[[nodiscard]] godot::Variant get(const godot::StringName& path) const;

	[[nodiscard]] const godot::Dictionary& values() const { return _values; }

	void clear();

	DirtyPaths take_dirty();
	bool take_write_all();

private:
	godot::Dictionary _values{};
	DirtyPaths _dirty{};
	bool _write_all = true;
};
