#pragma once

#include "compute_shader_shape.h"

enum class WriteScope {
	ALL,
	SYNCED,
	// as SYNCED, but GPU-owned fields the assigned value supplies are written too:
	// naming a value is how you ask for it to be pushed
	ASSIGNED,
};

// Deliberately independent of where a cursor is pointing, so the write rules can be
// reasoned about without a rendering device.
struct SyncPolicy {

	WriteScope scope = WriteScope::ALL;
	// nothing encloses the root, so it starts out undeclared
	SyncMode inherited = SyncMode::DEFAULT;

	[[nodiscard]] SyncMode effective_mode(const FieldShape& field) const {
		return field.sync_mode == SyncMode::DEFAULT ? inherited : field.sync_mode;
	}

	[[nodiscard]] SyncPolicy descend(const FieldShape& field) const {
		return SyncPolicy{ scope, effective_mode(field) };
	}

	[[nodiscard]] SyncPolicy with_scope(const WriteScope p_scope) const {
		return SyncPolicy{ p_scope, inherited };
	}

	// answers from the resolved mode alone, so a cursor that isn't filtering can still ask
	[[nodiscard]] bool writes_on_assignment_only() const {
		return inherited == SyncMode::NEVER;
	}

	// `supplied` is whether the value being written carries an entry for this field, which
	// is what separates assigning a whole parameter from assigning one member of it
	[[nodiscard]] bool should_write(const FieldShape& field, bool supplied) const;
};
