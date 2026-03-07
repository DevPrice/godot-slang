#pragma once

#include <utility>

#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/rid.hpp"

template <typename T>
class UniqueRID final {
	godot::ObjectID owner_id;
	godot::RID rid;

public:
	UniqueRID() :
			owner_id(godot::ObjectID{}), rid(godot::RID{}) {}
	explicit UniqueRID(T* p_owner) :
			UniqueRID(p_owner, {}) {}
	UniqueRID(T* p_owner, const godot::RID& p_rid) :
			owner_id(p_owner ? p_owner->get_instance_id() : godot::ObjectID{}), rid(p_rid) {}
	UniqueRID(UniqueRID&) = delete;
	UniqueRID(const UniqueRID&) = delete;
	UniqueRID(UniqueRID&& other) noexcept :
			owner_id(other.owner_id), rid(other.release()) {}

	~UniqueRID() {
		reset();
	}

	godot::RID get_rid() const { return rid; }

	void reset() {
		if (rid.is_valid()) {
			if (T* owner = godot::Object::cast_to<T>(godot::ObjectDB::get_instance(owner_id))) {
				owner->free_rid(rid);
			}
			rid = {};
		}
	}

	godot::RID release() {
		godot::RID ret = rid;
		rid = {};
		return ret;
	}

	void set_rid(const godot::RID& p_rid) {
		reset();
		rid = p_rid;
	}

	bool is_valid() const { return rid.is_valid(); }

	UniqueRID& operator=(const UniqueRID& other) = delete;

	UniqueRID& operator=(const godot::RID& rid) {
		reset();
		this->rid = rid;
		return *this;
	}

	UniqueRID& operator=(UniqueRID&& other) noexcept {
		std::swap(owner_id, other.owner_id);
		std::swap(rid, other.rid);
		return *this;
	}

	operator godot::RID() const { return rid; }
};
