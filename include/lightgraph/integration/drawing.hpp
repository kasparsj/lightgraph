#ifndef LIGHTGRAPH_INTEGRATION_DRAWING_HPP
#define LIGHTGRAPH_INTEGRATION_DRAWING_HPP

#pragma once
#include "src/drawing/DrawingRuntime.h"
namespace lightgraph::integration {
using lightgraph::drawing::DRAWING_MAX_IMAGE_BYTES;
using lightgraph::drawing::DRAWING_MAX_IMAGE_HEIGHT;
using lightgraph::drawing::DRAWING_MAX_IMAGE_WIDTH;
using lightgraph::drawing::DRAWING_MAX_SHAPES;
using lightgraph::drawing::DrawingBaseKind;
using lightgraph::drawing::DrawingControl;
using lightgraph::drawing::DrawingHandle;
using lightgraph::drawing::DrawingHandleKind;
using lightgraph::drawing::DrawingMode;
using lightgraph::drawing::DrawingPlacement;
using lightgraph::drawing::DrawingPrecondition;
using lightgraph::drawing::DrawingResult;
using lightgraph::drawing::DrawingRuntime;
using lightgraph::drawing::DrawingScene;
using lightgraph::drawing::DrawingShape;
using lightgraph::drawing::DrawingShapeKind;
using lightgraph::drawing::DrawingSource;
struct DrawingStatus : lightgraph::drawing::DrawingStatus {
    DrawingStatus() = default;
    DrawingStatus(const lightgraph::drawing::DrawingStatus& value)
        : lightgraph::drawing::DrawingStatus(value) {}
    std::uint8_t transferChunksReceived = 0;
    std::uint8_t transferChunksExpected = 0;
    std::uint32_t droppedTransfers = 0;
};
} // namespace lightgraph::integration

#endif // LIGHTGRAPH_INTEGRATION_DRAWING_HPP
