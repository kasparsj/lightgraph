#ifndef LIGHTGRAPH_INTEGRATION_DRAWING_CODEC_HPP
#define LIGHTGRAPH_INTEGRATION_DRAWING_CODEC_HPP

#pragma once
#include "drawing.hpp"
namespace lightgraph::integration {
class DrawingSceneCodec {
  public:
    static bool encode(const DrawingScene&, std::uint8_t*, std::size_t, std::size_t&);
    static bool decode(const std::uint8_t*, std::size_t, DrawingScene&);
};
} // namespace lightgraph::integration

#endif // LIGHTGRAPH_INTEGRATION_DRAWING_CODEC_HPP
