#include "Heptagon3024.h"

void Heptagon3024::setup() {
    static constexpr HeptagonStar::BridgePlacement kOuterBridges[7] = {
        {3023, 0},
        {2159, 2160},
        {1295, 1296},
        {431, 432},
        {2591, 2592},
        {1727, 1728},
        {863, 864},
    };
    static constexpr HeptagonStar::IntersectionPlacement kMiddleIntersections[7] = {
        {2014, 138},
        {2292, 1142},
        {1428, 267},
        {2443, 566},
        {2752, 1573},
        {1869, 717},
        {2883, 1001},
    };
    static constexpr HeptagonStar::IntersectionPlacement kInnerIntersections[7] = {
        {1964, 1053},
        {181, 1099},
        {2330, 227},
        {2376, 1474},
        {1520, 628},
        {2790, 678},
        {2841, 1915},
    };
    static constexpr PixelGap kGaps[6] = {
        {0, 143},
        {287, 431},
        {864, 935},
        {1223, 1295},
        {2016, 2735},
        {2880, 3023},
    };
    static constexpr HeptagonStar::LayoutDescriptor kLayout = {
        kOuterBridges,
        kMiddleIntersections,
        kInnerIntersections,
        kGaps,
        6,
    };

    setupLayout(kLayout);
}
