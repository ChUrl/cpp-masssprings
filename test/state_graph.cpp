#include "state_graph.hpp"
#include "cpu_layout_engine.hpp"
#include "lights_out_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <mutex>
#include <thread>

TEST_CASE("interactive graph growth keeps physics in sync", "[state_graph]")
{
    cpu_layout_engine physics(std::nullopt, nullptr);
    lights_out_model model;
    state_graph graph(physics);

    const state_id start = model.initial_state();
    graph.reset_to(start, model);

    // Make a handful of legal Lights Out moves through the same path the presenter uses.
    for (int i = 0; i < 8; ++i) {
        const state_id next =
            model.try_toggle(graph.get_current_index(), i % lights_out_model::SIZE,
                             (i / lights_out_model::SIZE) % lights_out_model::SIZE);
        graph.update_current(next, model);

        // Let the physics thread publish and continue.
        {
            std::lock_guard<std::mutex> lock(physics.state.data_mtx);
            physics.state.data_consumed = true;
        }
        physics.state.data_consumed_cnd.notify_all();
    }

    bool synced = false;
    for (int attempt = 0; attempt < 200; ++attempt) {
        {
            std::lock_guard<std::mutex> lock(physics.state.data_mtx);
            synced = physics.state.mass_count == model.state_count() &&
                physics.state.spring_count == graph.get_link_count();
            physics.state.data_consumed = true;
        }
        physics.state.data_consumed_cnd.notify_all();

        if (synced) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    REQUIRE(model.state_count() > 1);
    REQUIRE(graph.get_link_count() > 0);
    REQUIRE(synced);

    // Re-populating from a non-start current state must keep that state in the graph and must
    // explore its outgoing transitions as well.
    const std::uint64_t current_code = model.code(graph.get_current_index());
    graph.populate_graph(model, graph.get_starting_index(), graph.get_current_index(),
                         std::nullopt);

    REQUIRE(model.state_count() == 4096);
    REQUIRE(graph.get_link_count() == 32768);
    REQUIRE(model.code(graph.get_current_index()) == current_code);
}
