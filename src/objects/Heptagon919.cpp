#include "Heptagon919.h"

void Heptagon919::setup() {
    static constexpr HeptagonStar::BridgePlacement kOuterBridges[7] = {
        {918, 1},
        {653, 654},
        {389, 390},
        {125, 126},
        {789, 791},
        {522, 525},
        {260, 263},
    };
    static constexpr HeptagonStar::IntersectionPlacement kMiddleIntersections[7] = {
        {612, 42},
        {696, 347},
        {434, 81},
        {742, 172},
        {836, 478},
        {568, 218},
        {876, 304},
    };
    static constexpr HeptagonStar::IntersectionPlacement kInnerIntersections[7] = {
        {597, 320},
        {55, 334},
        {708, 69},
        {722, 448},
        {462, 191},
        {848, 206},
        {863, 582},
    };
    static constexpr HeptagonStar::LayoutDescriptor kLayout = {
        kOuterBridges,
        kMiddleIntersections,
        kInnerIntersections,
        nullptr,
        0,
    };

    setupLayout(kLayout);
}
