#ifndef HANOI_MODEL_HPP_
#define HANOI_MODEL_HPP_

#include "state_model.hpp"

#include <boost/unordered/unordered_flat_map.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/**
 * Towers of Hanoi with three pegs.
 *
 * A state stores the peg of every disk, two bits per disk. Disk 0 is the smallest disk, so the
 * top disk of a peg is the lowest-index disk on it. The default puzzle has 10 disks (59,049
 * states); the model supports up to 12 disks (531,441 states) for full exploration.
 */
class hanoi_model : public state_model
{
public:
    static constexpr int PEG_COUNT = 3;
    static constexpr int DEFAULT_DISKS = 10;
    static constexpr int MIN_DISKS = 1;
    static constexpr int MAX_DISKS = 12;

private:
    int disk_count_ = DEFAULT_DISKS;
    int target_peg_ = 2;

    std::vector<std::uint32_t> states;
    boost::unordered_flat_map<std::uint32_t, size_t> indices;

public:
    hanoi_model();
    explicit hanoi_model(int disks, int target_peg = 2);

    auto configure(int disks, int target_peg) -> void;

    [[nodiscard]] auto disk_count() const -> int;
    [[nodiscard]] auto target_peg() const -> int;
    [[nodiscard]] auto optimal_moves() const -> std::uint32_t;

    [[nodiscard]] auto state_count() const -> size_t override;
    [[nodiscard]] auto memory_bytes() const -> size_t override;
    [[nodiscard]] auto is_goal(state_id id) const -> bool override;
    [[nodiscard]] auto is_valid(state_id id) const -> bool override;
    [[nodiscard]] auto describe(state_id id) const -> std::string override;
    [[nodiscard]] auto clone() const -> std::unique_ptr<state_model> override;

    [[nodiscard]] auto initial_state() -> state_id override;
    [[nodiscard]] auto reset_to(state_id id) -> state_id override;
    [[nodiscard]] auto explore(state_id start, state_id current, threadpool pool,
                               exploration_observer* observer = nullptr)
        -> exploration_result override;

    [[nodiscard]] auto code(state_id id) const -> std::uint32_t;
    [[nodiscard]] auto peg(state_id id, int disk) const -> int;
    [[nodiscard]] auto top_disk(state_id id, int peg) const -> int;
    [[nodiscard]] auto disks_on_target(state_id id) const -> int;
    [[nodiscard]] auto find_or_insert(std::uint32_t state) -> state_id;
    [[nodiscard]] auto try_move(state_id id, int from_peg, int to_peg) -> std::optional<state_id>;
    [[nodiscard]] auto random_state() -> state_id;
    [[nodiscard]] auto solved_state() -> state_id;
    [[nodiscard]] auto start_state() -> state_id;

    template <typename F>
    auto for_each_legal_move(std::uint32_t state, F&& callback) const -> void;

private:
    [[nodiscard]] static auto peg_mask(int disk) -> std::uint32_t;
    [[nodiscard]] static auto peg_from_code(std::uint32_t state, int disk) -> int;
    [[nodiscard]] auto with_peg(std::uint32_t state, int disk, int peg) const -> std::uint32_t;
    [[nodiscard]] auto insert_code(std::uint32_t state) -> std::pair<state_id, bool>;
    [[nodiscard]] auto state_string(std::uint32_t state) const -> std::string;
};

template <typename F>
auto hanoi_model::for_each_legal_move(const std::uint32_t state, F&& callback) const -> void
{
    std::array<int, PEG_COUNT> top = {-1, -1, -1};
    for (int disk = 0; disk < disk_count_; ++disk) {
        const int p = peg_from_code(state, disk);
        if (top[static_cast<size_t>(p)] < 0) {
            top[static_cast<size_t>(p)] = disk;
        }
    }

    for (int from = 0; from < PEG_COUNT; ++from) {
        const int moving = top[static_cast<size_t>(from)];
        if (moving < 0) {
            continue;
        }

        for (int to = 0; to < PEG_COUNT; ++to) {
            if (to == from) {
                continue;
            }
            const int destination = top[static_cast<size_t>(to)];
            if (destination < 0 || destination > moving) {
                callback(with_peg(state, moving, to));
            }
        }
    }
}

#endif
