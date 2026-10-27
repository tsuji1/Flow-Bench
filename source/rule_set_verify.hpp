#pragma once

// --isolated で渡された rule が互いに重ならないことを確かめる
// rule を field 0 の範囲の始点でソートし、各 rule を「field 0 の始点が自分の終点以下」の
// 後ろの rule とだけ比べる
// 1 field: 重なりがなければ隣の rule としか比べないので O(n log n)
// 多 field: 最悪 O(n^2 * m)（全 rule で field 0 が wildcard のときなど）

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

    // 重なる 2 つの rule があれば true を返し、その index を overlapping に入れる
    bool operator()(const UDRuleSet& ruleSet, std::pair<uint32_t, uint32_t>& overlapping) const;
};

template <class T>
bool RuleSetVerifier::findOverlap(const UDRuleSet& ruleSet, std::pair<uint32_t, uint32_t>& overlapping) const {
    std::vector<T> mins, maxs;
    mins.reserve(ruleSet.size());
    maxs.reserve(ruleSet.size());
    for (const auto& rule : ruleSet) {
        // field はすべて SizedField（EM・LPM・RM）。RuleType::createField を参照
        const auto& field = static_cast<const SizedField<T>&>(rule->getField(0));
        mins.push_back(field.getMin());
        maxs.push_back(field.getMax());
    }
    std::vector<uint32_t> order(ruleSet.size());
    std::iota(order.begin(), order.end(), 0);
    // 始点が同じなら入力順で並べ、報告する組を毎回同じにする
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
