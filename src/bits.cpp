#include "bits.hpp"

auto print_bitmap(const u64 bitmap, const u8 w, const u8 h, const std::string& title) -> void {
    traceln("{}:", title);
    traceln("{}", std::string(2 * w - 1, '='));
    for (size_t y = 0; y < h; ++y) {
        std::cout << "         ";
        for (size_t x = 0; x < w; ++x) {
            std::cout << static_cast<int>(get_bits(bitmap, y * w + x, y * w + x)) << " ";
        }
        std::cout << "\n";
    }
    std::cout << std::flush;
    traceln("{}", std::string(2 * w - 1, '='));
}