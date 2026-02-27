#pragma once

#include "godot_cpp/templates/hashfuncs.hpp"

struct GodotHasher {
    template <typename T>
    size_t operator()(const T& k) const noexcept {
        return static_cast<size_t>(godot::HashMapHasherDefault::hash(k));
    }
};
