#pragma once

#include <unordered_map>

#include "godot_cpp/classes/rd_sampler_state.hpp"

#include "rids.h"

class SamplerCache {

private:
	struct SamplerKey {
		int64_t mag_filter;
		int64_t min_filter;
		int64_t mip_filter;
		int64_t repeat_u;
		int64_t repeat_v;
		int64_t repeat_w;
		double lod_bias;
		real_t min_lod;
		real_t max_lod;
		bool use_anisotropy;
		int64_t anisotropy_max;
		bool enable_compare;
		int64_t compare_op;
		int64_t border_color;
		bool unnormalized_uvw;

		explicit SamplerKey(const godot::RDSamplerState& sampler_state) :
				mag_filter(static_cast<int64_t>(sampler_state.get_mag_filter())),
				min_filter(static_cast<int64_t>(sampler_state.get_min_filter())),
				mip_filter(static_cast<int64_t>(sampler_state.get_mip_filter())),
				repeat_u(static_cast<int64_t>(sampler_state.get_repeat_u())),
				repeat_v(static_cast<int64_t>(sampler_state.get_repeat_v())),
				repeat_w(static_cast<int64_t>(sampler_state.get_repeat_w())),
				lod_bias(sampler_state.get_lod_bias()),
				min_lod(sampler_state.get_min_lod()),
				max_lod(sampler_state.get_max_lod()),
				use_anisotropy(sampler_state.get_use_anisotropy()),
				anisotropy_max(sampler_state.get_anisotropy_max()),
				enable_compare(sampler_state.get_enable_compare()),
				compare_op(static_cast<int64_t>(sampler_state.get_compare_op())),
				border_color(static_cast<int64_t>(sampler_state.get_border_color())),
				unnormalized_uvw(sampler_state.get_unnormalized_uvw()) {}

		bool operator==(const SamplerKey& other) const {
			return mag_filter == other.mag_filter &&
					min_filter == other.min_filter &&
					mip_filter == other.mip_filter &&
					repeat_u == other.repeat_u &&
					repeat_v == other.repeat_v &&
					repeat_w == other.repeat_w &&
					lod_bias == other.lod_bias &&
					min_lod == other.min_lod &&
					max_lod == other.max_lod &&
					use_anisotropy == other.use_anisotropy &&
					anisotropy_max == other.anisotropy_max &&
					enable_compare == other.enable_compare &&
					compare_op == other.compare_op &&
					border_color == other.border_color &&
					unnormalized_uvw == other.unnormalized_uvw;
		}
	};


	struct SamplerStateHasher {
		template <class T>
		void hash_combine(std::size_t& seed, const T& v) const {
			std::hash<T> hasher;
			seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}

		size_t operator()(const SamplerKey& sampler_key) const {
			size_t seed = 0;
			hash_combine(seed, sampler_key.mag_filter);
			hash_combine(seed, sampler_key.min_filter);
			hash_combine(seed, sampler_key.mip_filter);
			hash_combine(seed, sampler_key.repeat_u);
			hash_combine(seed, sampler_key.repeat_v);
			hash_combine(seed, sampler_key.repeat_w);
			hash_combine(seed, sampler_key.lod_bias);
			hash_combine(seed, sampler_key.use_anisotropy);
			hash_combine(seed, sampler_key.anisotropy_max);
			hash_combine(seed, sampler_key.enable_compare);
			hash_combine(seed, sampler_key.compare_op);
			hash_combine(seed, sampler_key.min_lod);
			hash_combine(seed, sampler_key.max_lod);
			hash_combine(seed, sampler_key.border_color);
			hash_combine(seed, sampler_key.unnormalized_uvw);

			return seed;
		}
	};

    godot::RenderingDevice* rd;
	std::unordered_map<SamplerKey, UniqueRID<godot::RenderingDevice>, SamplerStateHasher> cache;

public:
    explicit SamplerCache(godot::RenderingDevice* p_rendering_device);
    godot::RID get_sampler(const godot::Ref<godot::RDSamplerState>& sampler_state);
};
