#include "parameter_store.h"

#include <utility>

using namespace godot;

void ParameterStore::set(const StringName& path, const Variant& value) {
	const PackedStringArray parts = path.split("/");
	Variant current = _values;
	int64_t i = 0;
	bool valid;
	for (; i < parts.size() - 1; ++i) {
		Variant next = current.get_named(parts[i], valid);
		if (!valid || next.get_type() == Variant::NIL) {
			next = Dictionary();
			current.set_named(parts[i], next, valid);
		}
		current = next;
	}
	current.set_named(parts[i], value, valid);
	_dirty.insert(path);
}

Variant ParameterStore::get(const StringName& path) const {
	const PackedStringArray parts = path.split("/");
	Variant current = _values;
	int64_t i = 0;
	bool valid;
	for (; i < parts.size() - 1; ++i) {
		current = current.get_named(parts[i], valid);
		if (!valid || current.get_type() == Variant::NIL) {
			return {};
		}
	}
	return current.get_named(parts[i], valid);
}

void ParameterStore::clear() {
	_values.clear();
	_dirty.clear();
	_write_all = true;
}

ParameterStore::DirtyPaths ParameterStore::take_dirty() {
	return std::exchange(_dirty, {});
}

bool ParameterStore::take_write_all() {
	return std::exchange(_write_all, false);
}
