#pragma once

#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/rid.hpp"

template <typename T>
class UniqueRID {
	godot::ObjectID owner_id;
	godot::RID rid;

public:
	explicit UniqueRID(T* p_owner) : UniqueRID(p_owner, {}) { }
	UniqueRID(T* p_owner, const godot::RID& p_rid) :
			owner_id(p_owner ? p_owner->get_instance_id() : godot::ObjectID{}), rid(p_rid) {
		ERR_FAIL_NULL(p_owner);
	}
	UniqueRID(UniqueRID&) = delete;
	UniqueRID(const UniqueRID&) = delete;
	UniqueRID(UniqueRID&&) = delete;

	virtual ~UniqueRID() {
		reset();
	}

	godot::RID get_rid() const { return rid; }

	void reset() {
		if (rid.is_valid()) {
			if (T* owner = godot::Object::cast_to<T>(godot::ObjectDB::get_instance(owner_id))) {
				owner->free_rid(rid);
			}
		}
	}

	void set_rid(const godot::RID& p_rid) {
		reset();
		rid = p_rid;
	}

	bool is_valid() const { return rid.is_valid(); }

	godot::RID release() {
		godot::RID ret = rid;
		rid = {};
		return ret;
	}

	UniqueRID& operator=(const godot::RID& rid) {
		this->rid = rid;
		return *this;
	}

	operator godot::RID() const { return rid; }
};
