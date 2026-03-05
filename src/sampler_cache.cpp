#include "sampler_cache.h"

using namespace godot;

SamplerCache::SamplerCache(RenderingDevice* p_rendering_device) :
		rd(p_rendering_device) {
	ERR_FAIL_NULL(rd);
}

RID SamplerCache::get_sampler(const Ref<RDSamplerState>& sampler_state) {
	ERR_FAIL_NULL_V(sampler_state, {});
	const SamplerKey cache_key(*sampler_state.ptr());
	if (const auto it = cache.find(cache_key); it != cache.end()) {
		return it->second;
	}
	ERR_FAIL_NULL_V(rd, {});
	const RID sampler_rid = rd->sampler_create(sampler_state);
	auto [it, _] = cache.try_emplace(cache_key, rd, sampler_rid);
	return it->second;
}
