#include "puzzle_registry.hpp"

#include "eight_puzzle_codec.hpp"
#include "eight_puzzle_model.hpp"
#include "eight_puzzle_presenter.hpp"
#include "hanoi_codec.hpp"
#include "hanoi_model.hpp"
#include "hanoi_presenter.hpp"
#include "lights_out_codec.hpp"
#include "lights_out_model.hpp"
#include "lights_out_presenter.hpp"
#include "sliding_block_codec.hpp"
#include "sliding_block_model.hpp"
#include "sliding_block_presenter.hpp"
#include "sudoku4_codec.hpp"
#include "sudoku4_model.hpp"
#include "sudoku4_presenter.hpp"

#include <utility>

auto puzzle_registry::register_kind(std::string id, model_factory create_model,
                                    presenter_factory create_presenter, codec_factory create_codec)
    -> void
{
    kinds.emplace_back(std::move(id),
                       kind_factory{std::move(create_model), std::move(create_presenter),
                                    std::move(create_codec)});
}

auto puzzle_registry::create(const std::string& id) const -> std::unique_ptr<puzzle_kind>
{
    for (const auto& [kind_id, factory] : kinds) {
        if (kind_id != id) {
            continue;
        }

        std::unique_ptr<state_model> model = factory.model();
        if (!model) {
            return nullptr;
        }

        std::unique_ptr<puzzle_presenter> presenter = factory.presenter(*model);
        if (!presenter) {
            return nullptr;
        }

        std::unique_ptr<puzzle_codec> codec = factory.codec();
        if (!codec) {
            return nullptr;
        }

        return std::make_unique<puzzle_kind>(
            puzzle_kind{kind_id, std::move(model), std::move(presenter), std::move(codec)});
    }
    return nullptr;
}

auto puzzle_registry::create_with_model(const std::string& id,
                                        std::unique_ptr<state_model> model) const
    -> std::unique_ptr<puzzle_kind>
{
    if (!model) {
        return nullptr;
    }

    for (const auto& [kind_id, factory] : kinds) {
        if (kind_id != id) {
            continue;
        }

        std::unique_ptr<puzzle_presenter> presenter = factory.presenter(*model);
        if (!presenter) {
            return nullptr;
        }

        std::unique_ptr<puzzle_codec> codec = factory.codec();
        if (!codec) {
            return nullptr;
        }

        return std::make_unique<puzzle_kind>(
            puzzle_kind{kind_id, std::move(model), std::move(presenter), std::move(codec)});
    }
    return nullptr;
}

auto puzzle_registry::ids() const -> std::vector<std::string>
{
    std::vector<std::string> result;
    result.reserve(kinds.size());
    for (const auto& [kind_id, factory] : kinds) {
        (void)factory;
        result.push_back(kind_id);
    }
    return result;
}

auto register_builtin_puzzles(puzzle_registry& registry) -> void
{
    registry.register_kind(
        "sliding_block",
        [] { return std::unique_ptr<state_model>(std::make_unique<sliding_block_model>()); },
        [](state_model& model) -> std::unique_ptr<puzzle_presenter>
        {
            auto* sliding = dynamic_cast<sliding_block_model*>(&model);
            if (sliding == nullptr) {
                return nullptr;
            }
            return std::make_unique<sliding_block_presenter>(*sliding);
        },
        [] { return std::unique_ptr<puzzle_codec>(std::make_unique<sliding_block_codec>()); });

    registry.register_kind(
        "lights_out",
        [] { return std::unique_ptr<state_model>(std::make_unique<lights_out_model>()); },
        [](state_model& model) -> std::unique_ptr<puzzle_presenter>
        {
            auto* lights = dynamic_cast<lights_out_model*>(&model);
            if (lights == nullptr) {
                return nullptr;
            }
            return std::make_unique<lights_out_presenter>(*lights);
        },
        [] { return std::unique_ptr<puzzle_codec>(std::make_unique<lights_out_codec>()); });

    registry.register_kind(
        "hanoi", [] { return std::unique_ptr<state_model>(std::make_unique<hanoi_model>()); },
        [](state_model& model) -> std::unique_ptr<puzzle_presenter>
        {
            auto* hanoi = dynamic_cast<hanoi_model*>(&model);
            if (hanoi == nullptr) {
                return nullptr;
            }
            return std::make_unique<hanoi_presenter>(*hanoi);
        },
        [] { return std::unique_ptr<puzzle_codec>(std::make_unique<hanoi_codec>()); });

    registry.register_kind(
        "eight_puzzle",
        [] { return std::unique_ptr<state_model>(std::make_unique<eight_puzzle_model>()); },
        [](state_model& model) -> std::unique_ptr<puzzle_presenter>
        {
            auto* eight = dynamic_cast<eight_puzzle_model*>(&model);
            if (eight == nullptr) {
                return nullptr;
            }
            return std::make_unique<eight_puzzle_presenter>(*eight);
        },
        [] { return std::unique_ptr<puzzle_codec>(std::make_unique<eight_puzzle_codec>()); });

    registry.register_kind(
        "sudoku4", [] { return std::unique_ptr<state_model>(std::make_unique<sudoku4_model>()); },
        [](state_model& model) -> std::unique_ptr<puzzle_presenter>
        {
            auto* sudoku = dynamic_cast<sudoku4_model*>(&model);
            if (sudoku == nullptr) {
                return nullptr;
            }
            return std::make_unique<sudoku4_presenter>(*sudoku);
        },
        [] { return std::unique_ptr<puzzle_codec>(std::make_unique<sudoku4_codec>()); });
}
