#ifndef LIGHTS_OUT_RULES_HPP_
#define LIGHTS_OUT_RULES_HPP_

#include <cstdint>
#include <string>

enum class lights_out_neighborhood : std::uint8_t
{
    orthogonal,
    diagonal,
    orthogonal_wrap,
    diagonal_wrap,
};

/**
 * Rule set for a Lights Out-style puzzle. The model is generic over the grid size, the number of
 * light states (2 or 3), the neighbour pattern, the set of pressable cells and the target pattern.
 */
struct lights_out_rules
{
    int width = 4;
    int height = 4;
    int base = 2;
    lights_out_neighborhood neighborhood = lights_out_neighborhood::orthogonal;
    std::uint32_t pressable_mask = 0xFFFFu;
    std::uint64_t target = 0;
    std::string name = "Lights Out 4x4";

    [[nodiscard]] auto cell_count() const -> int
    {
        return width * height;
    }

    [[nodiscard]] auto is_pressable(const int x, const int y) const -> bool
    {
        if (x < 0 || x >= width || y < 0 || y >= height) {
            return false;
        }
        return (pressable_mask & (1u << (y * width + x))) != 0;
    }

    [[nodiscard]] auto is_classic() const -> bool
    {
        return width == 4 && height == 4 && base == 2 &&
            neighborhood == lights_out_neighborhood::orthogonal && pressable_mask == 0xFFFFu &&
            target == 0;
    }

    [[nodiscard]] auto neighborhood_name() const -> const char*
    {
        switch (neighborhood) {
        case lights_out_neighborhood::orthogonal:
            return "orth";
        case lights_out_neighborhood::diagonal:
            return "diag";
        case lights_out_neighborhood::orthogonal_wrap:
            return "orth_wrap";
        case lights_out_neighborhood::diagonal_wrap:
            return "diag_wrap";
        }
        return "orth";
    }
};

#endif
