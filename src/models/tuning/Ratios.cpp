#include "Ratios.h"
#include <numeric>

namespace MoTool {

std::vector<int> inverseRatios(const std::vector<int>& ratios) {
    // periods are in reverse relation to freqRatios
    // For example for freq ratios of {48,    45,    40,    36,    32,    30,    27}
    // Ratios with respect to root is {48:48, 45:48, 40:48, 36:48, 32:48, 30:48, 27:48}
    // Which is simply                {1:1,   15:16,  5:6,   3:4,   2:3,   5:8,   9:16}
    // 2 * 2 * 2 * 2 * 3 = 48
    // So period ratios is            {1:1,   16:15,   6:5,    4:3,    3:2,    8:5,   16:9}
    // 5 * 3 * 3 * 2 = 90             {90:90, 96:90, 108:90, 120:90, 135:90, 144:90, 160:90}
    // Result is                      {90, 96, 108, 120, 135, 144, 160}

    if (ratios.size() <= 1) {
        return ratios; // Single or empty value returns same value
    }

    std::vector<int> result;
    result.reserve(ratios.size());

    // Step 1: Find the LCM of all frequency ratios to establish a common base
    // Calculate LCM of all frequency values
    int lcm = ratios[0];
    for (size_t i = 1; i < ratios.size(); ++i) {
        lcm = (lcm * ratios[i]) / std::gcd(lcm, ratios[i]);
    }

    // Step 2: Convert frequency ratios to period ratios by inversion
    // If frequency ratio is A:B, period ratio is B:A
    // Multiply by LCM to get integer period values
    for (size_t i = 0; i < ratios.size(); ++i) {
        result.push_back(lcm / ratios[i]);
    }

    return result;
}

}