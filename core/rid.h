#pragma once

#include "core/typedefs.h"
#include <set>

class RID_owner_base;

// This is the base class each Resource must extend
class ResourceData {
	friend class RID_owner_base;

	RID_owner_base *owner;

public:
	ResourceData();
};

class RID {

	ResourceData *data;

public:
	RID();

	_FORCE_INLINE_ ResourceData *get_data() { return data; }
};

class RID_owner_base {
	bool set_owner(ResourceData *r_data, RID_owner_base *p_owner);
};

template <class T>
class RID_owner : public RID_owner_base {

	std::set<ResourceData *> resources;

	RID make_rid(T *r_resource);
	void release(RID &p_rid);

	bool is_owner(RID &p_rid);
	T *get(RID &p_rid);

private:
	bool set_data(RID &r_rid, ResourceData *p_data);
};
