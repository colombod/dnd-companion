#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace play {
// Keep a continuous touch bound to its original die, even if it crosses another.
struct DieHold {
    static constexpr uint32_t interval = 500;
    int die = -1;
    bool consumed = false;
    uint32_t last = 0;

    void begin(int target, uint32_t now) {
        die = target;
        last = now;
        consumed = false;
    }
    bool due(int current, uint32_t now) {
        if (die != current) die = -1;
        if (die < 0 || now - last < interval) return false;
        // After a delayed frame, decrement once and allow time to release.
        const uint32_t elapsed = now - last;
        last = elapsed < interval + 50 ? last + interval : now;
        consumed = true;
        return true;
    }
};

// Measure only detents from this throw. Speed controls the illustration alone.
struct SpinMotion {
    uint32_t start = 0, last = 0;
    int strength = 0, direction = 1;

    void begin(int steps, uint32_t now) {
        start = last = now;
        direction = steps < 0 ? -1 : 1;
        strength = std::min(4, std::abs(steps) - 1);
    }
    void sample(int steps, uint32_t now) {
        if (now - start > 260) return;
        const auto interval = std::max(uint32_t(1), now - last) / std::max(1, std::abs(steps));
        const int energy = interval <= 30 ? 4 : interval <= 60 ? 3 : interval <= 95 ? 2 : interval <= 145 ? 1 : 0;
        strength = std::max(strength, energy);
        direction = steps < 0 ? -1 : 1;
        last = now;
    }
    unsigned duration() const { return 390 + 95 * strength; }
};
}
