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

	virtual ~ResourceData() {}
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
#ifdef DEBUG_ENABLED
protected:
	mutable std::set<ResourceData *> resources;
#endif

protected:
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

	void _make_rid(RID &r_rid, ResourceData *p_data) {
		DEBUG_ONLY(ERR_FAIL_COND(p_data->owner));
		r_rid.data = p_data;
		p_data->owner = this;

#ifdef DEBUG_ENABLED
		resources.insert(p_data);
#endif
	}

	void _release(ResourceData *p_data) {
		DEBUG_ONLY(ERR_FAIL_COND(!_is_owner(p_data)));
		p_data->owner = nullptr;

#ifdef DEBUG_ENABLED
		resources.erase(resources.find(p_data));
#endif
	}
};

template <class T>
class RID_owner : public RID_owner_base {

public:
	RID make_rid(T *r_resource) {
		RID r;
		_make_rid(r, r_resource);
		return r;
	}

	void release(RID &r_rid) {
		ResourceData *rd = r_rid.get_data();
		_release(rd);
	}

	bool is_owner(const RID &p_rid) const {
		return _is_owner(p_rid.get_data());
	}

	T *get(const RID &p_rid) const {
		DEBUG_ONLY(ERR_FAIL_COND_V(!is_owner(p_rid), nullptr));
		return static_cast<T *>(p_rid.get_data());
	}
};
