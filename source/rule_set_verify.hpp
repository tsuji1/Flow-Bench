#pragma once

// verify that the rules given with --isolated do not overlap with each other
// rules are sorted by the range of field 0, and each rule is compared only with
// the following rules whose field-0 range starts before it ends
// 1-field rules: O(n log n), since only adjacent rules are compared when the set is disjoint
// multi-field rules: O(n^2 * m) in the worst case (e.g., field 0 is a wildcard in every rule)

#include <algorithm>
#include <numeric>
#include <utility>
#include <vector>

#include "int32.hpp"
#include "int64.hpp"
#include "int128.hpp"
#include "match_field_sized.hpp"
#include "rule_set.hpp"

namespace flowbench {

class RuleSetVerifier : public Singleton<RuleSetVerifier> {
private:
    template <class T> // where T : Integer
    bool findOverlap(const UDRuleSet& ruleSet, std::pair<uint32_t, uint32_t>& overlapping) const;

public:
    RuleSetVerifier() = default;

    // return true if two rules overlap, and set their indexes to overlapping
    bool operator()(const UDRuleSet& ruleSet, std::pair<uint32_t, uint32_t>& overlapping) const;
};

template <class T>
bool RuleSetVerifier::findOverlap(const UDRuleSet& ruleSet, std::pair<uint32_t, uint32_t>& overlapping) const {
    std::vector<T> mins, maxs;
    mins.reserve(ruleSet.size());
    maxs.reserve(ruleSet.size());
    for (const auto& rule : ruleSet) {
        // every field is a SizedField (EM, LPM, and RM), see RuleType::createField
        const auto& field = static_cast<const SizedField<T>&>(rule->getField(0));
        mins.push_back(field.getMin());
        maxs.push_back(field.getMax());
    }
    std::vector<uint32_t> order(ruleSet.size());
    std::iota(order.begin(), order.end(), 0);
    // ties are broken by input order so that the reported pair is deterministic
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        return mins[a] < mins[b] || (!(mins[b] < mins[a]) && a < b);
    });
    for (uint32_t i = 0; i < order.size(); i++) {
        const T& max = maxs[order[i]];
        for (uint32_t j = i + 1; j < order.size() && mins[order[j]] <= max; j++) {
            if (ruleSet.getRule(order[i]).overlap(ruleSet.getRule(order[j]))) {
                overlapping = std::minmax(order[i], order[j]);
                return true;
            }
        }
    }
    return false;
}

bool RuleSetVerifier::operator()(const UDRuleSet& ruleSet, std::pair<uint32_t, uint32_t>& overlapping) const {
    uint8_t width = RuleTypeUD::getInstance().getFieldWidth(0);
    if (width <= 32) {
        return findOverlap<Int32>(ruleSet, overlapping);
    } else if (width <= 64) {
        return findOverlap<Int64>(ruleSet, overlapping);
    } else {
        return findOverlap<Int128>(ruleSet, overlapping);
    }
}

}
