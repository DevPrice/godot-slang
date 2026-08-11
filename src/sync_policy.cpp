#include "sync_policy.h"

#include "attributes.h"

using namespace godot;

bool SyncPolicy::should_write(const FieldShape& field, const bool supplied) const {
	// anything that didn't resolve to NEVER is written, so an unresolved DEFAULT writes too
	if (scope == WriteScope::ALL || effective_mode(field) != SyncMode::NEVER) {
		return true;
	}
	// the only way to reseed a GPU-owned field
	if (scope == WriteScope::ASSIGNED && supplied) {
		return true;
	}
	const AttributeRegistry* registry = AttributeRegistry::get_instance();
	for (const StringName attribute_name : field.user_attributes.keys()) {
		if (registry->writes_every_dispatch(attribute_name)) {
			return true;
		}
	}
	// descend anyway if something nested still has to be written; those fields filter themselves
	return field.shape.is_valid() && field.shape->has_forced_writes();
}
