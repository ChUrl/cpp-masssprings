#include "bits.hpp"

auto print_bitmap(const uint64_t bitmap, const uint8_t w, const uint8_t h, const std::string& title) -> void {
    traceln("{}:", title);
    traceln("{}", std::string(2 * w - 1, '='));
    for (size_t y = 0; y < w; ++y) {
        std::cout << "         ";
        for (size_t x = 0; x < h; ++x) {
            std::cout << static_cast<int>(get_bits(bitmap, y * w + x, y * h + x)) << " ";
        }
        std::cout << "\n";
    }
    std::cout << std::flush;
    traceln("{}", std::string(2 * w - 1, '='));
}