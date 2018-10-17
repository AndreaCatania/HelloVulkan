#pragma once

#include "core/error_macros.h"
#include "core/typedefs.h"
#include <set>

class RID_owner_base;

// This is the base class each Resource must extend
class ResourceData {
	friend class RID_owner_base;

	RID_owner_base *owner;

public:
	ResourceData() :
			owner(nullptr) {}
};

class RID {
	friend class RID_owner_base;

	ResourceData *data;

public:
	RID() :
			data(nullptr) {}

	_FORCE_INLINE_ ResourceData *get_data() const { return data; }
};

class RID_owner_base {
protected:
	mutable std::set<ResourceData *> resources;

public:
	bool _is_owner(const ResourceData *p_res) const {

		if (!p_res)
			return false;

#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V(
				resources.find(
						const_cast<ResourceData *>(p_res)) == resources.end(),
				false);
#endif

		return p_res->owner == this;
	}

	bool set_owner(ResourceData *r_data, RID_owner_base *p_owner) {
		if (r_data->owner)
			return false;

		r_data->owner = p_owner;
	}

	bool set_data(RID &r_rid, ResourceData *p_data) {
		r_rid.data = p_data;
		return set_owner(p_data, this);
	}
};

template <class T>
class RID_owner : public RID_owner_base {

public:
	RID make_rid(T *r_resource) {

		RID r;
		const bool should_be_register = set_data(r, r_resource);
		if (should_be_register) {
			resources.insert(r_resource);
		} else {
			WARN_PRINT("This resource was already registered");
		}
		return r;
	}

	void release(RID &r_rid) {
		ERR_FAIL_COND(!is_owner(r_rid));

		ResourceData *rd = r_rid.get_data();

		set_owner(rd, nullptr);

#ifdef DEBUG_ENABLED
		resources.erase(resources.find(rd));
#endif
	}

	bool is_owner(const RID &p_rid) const {

		return _is_owner(p_rid.get_data());
	}

	T *get(const RID &p_rid) const {

#ifdef DEBUG_ENABLED
		if (p_rid.get_data()) {

			ERR_FAIL_COND_V(
					resources.find(p_rid.get_data()) == resources.end(),
					nullptr);
		}
#endif

		return static_cast<T *>(p_rid.get_data());
	}
};
