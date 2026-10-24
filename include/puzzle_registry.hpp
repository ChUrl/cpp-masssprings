#ifndef PUZZLE_REGISTRY_HPP_
#define PUZZLE_REGISTRY_HPP_

#include "puzzle_context.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class puzzle_registry
{
public:
    using model_factory = std::function<std::unique_ptr<state_model>()>;
    using presenter_factory = std::function<std::unique_ptr<puzzle_presenter>(state_model&)>;
    using codec_factory = std::function<std::unique_ptr<puzzle_codec>()>;

    struct kind_factory
    {
        model_factory model;
        presenter_factory presenter;
        codec_factory codec;
    };

private:
    std::vector<std::pair<std::string, kind_factory>> kinds;

public:
    auto register_kind(std::string id, model_factory create_model,
                       presenter_factory create_presenter, codec_factory create_codec) -> void;
    [[nodiscard]] auto create(const std::string& id) const -> std::unique_ptr<puzzle_kind>;
    [[nodiscard]] auto create_with_model(const std::string& id,
                                         std::unique_ptr<state_model> model) const
        -> std::unique_ptr<puzzle_kind>;
    [[nodiscard]] auto ids() const -> std::vector<std::string>;
};

auto register_builtin_puzzles(puzzle_registry& registry) -> void;

#endif
