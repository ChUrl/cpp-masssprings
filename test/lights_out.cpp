#include "graph_distances.hpp"
#include "lights_out_codec.hpp"
#include "lights_out_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

namespace
{
auto encode_digits(const std::string_view digits, const int base) -> std::uint64_t
{
    std::uint64_t code = 0;
    std::uint64_t power = 1;
    for (const char c : digits) {
        code += static_cast<std::uint64_t>(c - '0') * power;
        power *= static_cast<std::uint64_t>(base);
    }
    return code;
}

auto parse_mask(const std::string_view bits) -> std::uint32_t
{
    std::uint32_t mask = 0;
    for (size_t i = 0; i < bits.size(); ++i) {
        if (bits[i] == '1') {
            mask |= 1u << i;
        }
    }
    return mask;
}

auto find_goal(const lights_out_model& model) -> std::optional<state_id>
{
    for (state_id id = 0; id < model.state_count(); ++id) {
        if (model.is_goal(id)) {
            return id;
        }
    }
    return std::nullopt;
}

auto distance_to_goal(const lights_out_model& model, const exploration_result& result,
                      const state_id start) -> std::optional<int>
{
    const std::optional<state_id> goal = find_goal(model);
    if (!goal) {
        return std::nullopt;
    }

    graph_distances distances;
    distances.calculate_distances(model.state_count(), result.links, {*goal},
                                  model.directed_edges());
    if (start >= distances.distances.size()) {
        return std::nullopt;
    }
    return distances.distances[start];
}

auto max_distance_to_goal(const lights_out_model& model, const exploration_result& result)
    -> std::optional<int>
{
    const std::optional<state_id> goal = find_goal(model);
    if (!goal) {
        return std::nullopt;
    }

    graph_distances distances;
    distances.calculate_distances(model.state_count(), result.links, {*goal},
                                  model.directed_edges());
    int maximum = -1;
    for (const int distance : distances.distances) {
        maximum = std::max(maximum, distance);
    }
    return maximum;
}
} // namespace

TEST_CASE("4x4 Lights Out reachable component has 4096 states", "[lights_out]")
{
    lights_out_model model;
    const state_id start = model.initial_state();

    REQUIRE(model.mask(start) == lights_out_model::ALL_ON);

    const exploration_result result = model.explore(start, start, std::nullopt);

    // The 4x4 Lights Out move matrix has rank 12 over GF(2), so the reachable component has
    // 2^12 states.
    REQUIRE(model.state_count() == 4096);
    REQUIRE(result.start == result.current);
    REQUIRE(result.complete);

    size_t goal_count = 0;
    for (state_id id = 0; id < model.state_count(); ++id) {
        if (model.is_goal(id)) {
            ++goal_count;
        }
    }
    REQUIRE(goal_count == 1);

    REQUIRE(!result.links.empty());
    for (const auto& [from, to] : result.links) {
        REQUIRE(from < to);
        REQUIRE(to < model.state_count());
    }
}

TEST_CASE("4x4 Lights Out toggles are reversible", "[lights_out]")
{
    lights_out_model model;
    const state_id solved = model.solved_state();

    const state_id toggled = model.try_toggle(solved, 0, 0);
    REQUIRE(toggled != solved);
    REQUIRE(model.on_count(toggled) > 0);

    const state_id back = model.try_toggle(toggled, 0, 0);
    REQUIRE(back == solved);
    REQUIRE(model.is_goal(back));
}

TEST_CASE("4x4 Lights Out codec round-trips", "[lights_out]")
{
    lights_out_model model;
    lights_out_codec codec;

    const std::optional<state_id> solved = codec.parse("LO:[4x4] B:[0000000000000000]", model);
    REQUIRE(solved);
    REQUIRE(model.is_goal(*solved));
    REQUIRE(codec.format(model, *solved) == "LO:[4x4] B:[0000000000000000]");
    REQUIRE(codec.validate(model, *solved) == std::nullopt);

    const std::optional<state_id> all_on = codec.parse("LO:[4x4] B:[1111111111111111]", model);
    REQUIRE(all_on);
    REQUIRE(model.mask(*all_on) == lights_out_model::ALL_ON);
    REQUIRE(codec.format(model, *all_on) == "LO:[4x4] B:[1111111111111111]");
}

TEST_CASE("4x4 Lights Out reset keeps only one state", "[lights_out]")
{
    lights_out_model model;
    const state_id start = model.initial_state();
    (void)model.explore(start, start, std::nullopt);
    REQUIRE(model.state_count() == 4096);

    const state_id toggled = model.try_toggle(start, 1, 1);
    const std::uint16_t expected = model.mask(toggled);
    const state_id reset = model.reset_to(toggled);

    REQUIRE(model.state_count() == 1);
    REQUIRE(reset == 0);
    REQUIRE(model.mask(reset) == expected);
}

TEST_CASE("4x4 Lights Out intermediate rule variants", "[lights_out]")
{
    SECTION("diagonal neighbours")
    {
        lights_out_rules rules;
        rules.neighborhood = lights_out_neighborhood::diagonal;
        lights_out_model model(rules);

        const state_id start = model.initial_state();
        const exploration_result result = model.explore(start, start, std::nullopt);

        REQUIRE(model.state_count() == 4096);
        REQUIRE(!result.directed);
        REQUIRE(distance_to_goal(model, result, start) == 8);
    }

    SECTION("wrap-around neighbours")
    {
        lights_out_rules rules;
        rules.neighborhood = lights_out_neighborhood::orthogonal_wrap;
        lights_out_model model(rules);

        const state_id start = model.initial_state();
        const exploration_result result = model.explore(start, start, std::nullopt);

        REQUIRE(model.state_count() == 65536);
        REQUIRE(!result.directed);
        REQUIRE(distance_to_goal(model, result, start) == 16);
    }
}

TEST_CASE("4x4 Lights Out hard 3-state variants", "[lights_out]")
{
    SECTION("restricted ternary, 20 moves")
    {
        lights_out_rules rules;
        rules.base = 3;
        rules.pressable_mask = parse_mask("0010111110011011");
        lights_out_model model(rules);

        const state_id start = model.find_or_insert(encode_digits("1222001002002220", 3));
        const exploration_result result = model.explore(start, start, std::nullopt);

        REQUIRE(model.state_count() == 59049);
        REQUIRE(result.directed);
        REQUIRE(model.directed_edges());
        REQUIRE(distance_to_goal(model, result, start) == 20);
        REQUIRE(max_distance_to_goal(model, result) == 20);
    }

    SECTION("restricted ternary wrap, 20 moves")
    {
        lights_out_rules rules;
        rules.base = 3;
        rules.neighborhood = lights_out_neighborhood::orthogonal_wrap;
        rules.pressable_mask = parse_mask("0110111000111110");
        lights_out_model model(rules);

        const state_id start = model.find_or_insert(encode_digits("0111211000122110", 3));
        const exploration_result result = model.explore(start, start, std::nullopt);

        REQUIRE(model.state_count() == 59049);
        REQUIRE(result.directed);
        REQUIRE(model.directed_edges());
        REQUIRE(distance_to_goal(model, result, start) == 20);
        REQUIRE(max_distance_to_goal(model, result) == 20);
    }
}

TEST_CASE("Lights Out re-exploring from a non-start state keeps it connected", "[lights_out]")
{
    lights_out_rules rules;
    rules.base = 3;
    rules.pressable_mask = parse_mask("0010111110011011");
    lights_out_model model(rules);

    const state_id start = model.find_or_insert(encode_digits("1222001002002220", 3));
    const exploration_result first = model.explore(start, start, std::nullopt);
    REQUIRE(model.state_count() == 59049);
    REQUIRE(first.links.size() == 590490);

    const state_id current = 1;
    const std::uint64_t current_code = model.code(current);

    const exploration_result second = model.explore(start, current, std::nullopt);

    REQUIRE(model.state_count() == 59049);
    REQUIRE(model.code(second.current) == current_code);
    REQUIRE(second.links.size() == first.links.size());
}

TEST_CASE("4x4 Lights Out hard rules randomize and reset", "[lights_out]")
{
    lights_out_rules rules;
    rules.base = 3;
    rules.pressable_mask = parse_mask("0010111110011011");
    lights_out_model model(rules);

    const state_id random = model.random_state();
    REQUIRE(model.is_valid(random));
    REQUIRE(!model.is_goal(random));

    const state_id solved = model.solved_state();
    REQUIRE(model.is_goal(solved));

    const std::uint64_t solved_code = model.code(solved);
    REQUIRE(model.try_toggle(solved, 0, 0) == solved);
    REQUIRE(model.code(model.try_toggle(solved, 2, 0)) != solved_code);
}

TEST_CASE("4x4 Lights Out extended codec round-trips", "[lights_out]")
{
    lights_out_model model;
    lights_out_codec codec;

    const std::string payload =
        "LO:[4x4] V:[3] N:[orth_wrap] P:[0110111000111110] T:[0000000000000000] "
        "B:[0111211000122110]";

    const std::optional<state_id> id = codec.parse(payload, model);
    REQUIRE(id);
    REQUIRE(model.rules().base == 3);
    REQUIRE(model.rules().neighborhood == lights_out_neighborhood::orthogonal_wrap);
    REQUIRE(model.rules().pressable_mask == parse_mask("0110111000111110"));
    REQUIRE(model.code(*id) == encode_digits("0111211000122110", 3));
    REQUIRE(codec.format(model, *id) == payload);

    const std::optional<state_id> diagonal =
        codec.parse("LO:[4x4] N:[diag] B:[1111111111111111]", model);
    REQUIRE(diagonal);
    REQUIRE(model.rules().neighborhood == lights_out_neighborhood::diagonal);
    REQUIRE(!model.directed_edges());

    const std::optional<state_id> wrap =
        codec.parse("LO:[4x4] N:[orth_wrap] B:[1111111111111111]", model);
    REQUIRE(wrap);
    REQUIRE(model.rules().neighborhood == lights_out_neighborhood::orthogonal_wrap);
    REQUIRE(!model.directed_edges());
}
