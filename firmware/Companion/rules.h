#pragma once
#include <cstddef>
#include <cstdint>

namespace dnd {
enum class Ability : uint8_t { Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma };
enum class Kind : uint8_t { AbilityCheck, SkillCheck, SavingThrow };
enum class Training : uint8_t { None, Half, Proficient, Expertise };
enum class Mode : uint8_t { Normal, Advantage, Disadvantage };

constexpr int abilityModifier(int score) { return score / 2 - 5; } // scores 1..30
constexpr int proficiencyBonus(int level) { return 2 + (level - 1) / 4; } // levels 1..20
constexpr int trainingBonus(Training training, int pb) {
    switch (training) {
    case Training::Half: return pb / 2;
    case Training::Proficient: return pb;
    case Training::Expertise: return 2 * pb;
    default: return 0;
    }
}
constexpr Mode rollMode(bool advantage, bool disadvantage) {
    if (advantage == disadvantage) return Mode::Normal;
    return advantage ? Mode::Advantage : Mode::Disadvantage;
}

// The importer supplies the named entry, its optional sheet override and
// whether its inputs are resolved. Situational effects are a roll-time choice.
struct Check {
    const char *name;
    Kind kind;
    Ability ability;
    Training training;
    int16_t bonus;
    bool resolved;
    bool hasOverride;
    int16_t overrideTotal;
};
struct Profile {
    uint8_t level;
    uint8_t scores[6];
    const Check *checks;
    size_t count;
};
struct Result { int kept; int64_t total; bool meetsDC(int dc) const { return total >= dc; } };

inline bool modifier(const Profile &p, size_t index, int &value) {
    if (index >= p.count || !p.checks || p.level < 1 || p.level > 20) return false;
    const auto &check = p.checks[index];
    if (!check.resolved) return false;
    if (check.hasOverride) { value = check.overrideTotal; return true; }
    auto ability = static_cast<uint8_t>(check.ability);
    if (ability >= 6 || p.scores[ability] < 1 || p.scores[ability] > 30) return false;
    value = abilityModifier(p.scores[ability]) + trainingBonus(check.training, proficiencyBonus(p.level)) + check.bonus;
    return true;
}
inline bool resolveD20(Mode mode, int first, int second, int bonus, Result &result) {
    if (first < 1 || first > 20 || (mode != Mode::Normal && (second < 1 || second > 20))) return false;
    int kept = first;
    if ((mode == Mode::Advantage && second > kept) || (mode == Mode::Disadvantage && second < kept)) kept = second;
    result = {kept, int64_t(kept) + bonus};
    return true;
}
struct CheckRoll { Mode mode; int first, second, bonus; Result result; };
template<class NextFace>
bool rollCheck(const Profile &p, size_t index, bool advantage, bool disadvantage,
               NextFace next, CheckRoll &out) {
    CheckRoll r{};
    if (!modifier(p, index, r.bonus)) return false;
    r.mode = rollMode(advantage, disadvantage);
    r.first = next(20);
    if (r.mode != Mode::Normal) r.second = next(20);
    if (!resolveD20(r.mode, r.first, r.second, r.bonus, r.result)) return false;
    out = r;
    return true;
}

struct DieGroup { uint16_t count, sides; };
// Caller owns the random source, animation and history; returns faces in [1,sides].
template<class NextFace>
bool rollPool(const DieGroup *groups, size_t count, int bonus, NextFace next, int64_t &total) {
    if (!groups || !count) return false;
    int64_t sum = bonus;
    unsigned dice = 0;
    for (size_t i = 0; i < count; i++) {
        const auto &g = groups[i];
        if (!g.count || g.sides < 2 || g.sides > 100 || (dice += g.count) > 100) return false;
        for (size_t n = 0; n < g.count; n++) {
            int face = next(g.sides);
            if (face < 1 || face > g.sides) return false;
            sum += face;
        }
    }
    total = sum;
    return true;
}
}
