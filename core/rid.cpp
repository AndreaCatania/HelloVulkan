#include "rid.h"

#include "core/error_macros.h"

bool RID_owner_base::set_owner(ResourceData *r_data, RID_owner_base *p_owner) {
	if (r_data->owner)
		return false;

	r_data->owner = p_owner;
}

ResourceData::ResourceData() :
		owner(nullptr) {}

template <class T>
RID RID_owner<T>::make_rid(T *r_resource) {

	r_resource.get_self();

	RID r;
	if (set_data(r, r_resource)) {
		resources.insert(r_resource);
	} else {
		WARN_PRINT("This resource was already registered");
	}

	return r;
}

template <class T>
void RID_owner<T>::release(RID &r_rid) {
	ERR_FAIL_COND(!is_owner(r_rid));

	ResourceData *rd = p_rid.get_data();

	set_owner(rd, nullptr);

#ifdef DEBUG_ENABLED
	resources.erase(resources.find(rd));
#endif
}

template <class T>
bool RID_owner<T>::is_owner(RID &p_rid) {

#ifdef DEBUG_ENABLED
	if (p_rid.get_data()) {

		ERR_FAIL_COND_V(
				resources.find(p_rid.get_data()) == resources.end(), false);
	}
#endif

	return p_rid.get_data()->owner == this;
}

template <class T>
T *RID_owner<T>::get(RID &p_rid) {

#ifdef DEBUG_ENABLED
	if (p_rid.get_data()) {

		ERR_FAIL_COND_V(
				resources.find(p_rid.get_data()) == resources.end(), nullptr);
	}
#endif

	return static_cast<T *>(p_rid.get_data());
}

template <class T>
bool RID_owner<T>::set_data(RID &r_rid, ResourceData *p_data) {
	r_rid.data = p_data;
	return set_owner(p_data, this);
}
