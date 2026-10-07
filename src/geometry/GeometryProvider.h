#ifndef PACKAGES_LIGHTGRAPH_SRC_GEOMETRY_GEOMETRYPROVIDER_H_
#define PACKAGES_LIGHTGRAPH_SRC_GEOMETRY_GEOMETRYPROVIDER_H_

#pragma once

#include <cstddef>
#include <cstdint>

class TopologyObject;

namespace lightgraph::geometry {

enum class GeometryResult : uint8_t {
    Ready = 0,
    Unsupported,
    AdmissionDenied,
    AllocationFailed,
};

struct GeometryPoint {
    float x;
    float y;
};

template <typename T>
class GeometryArrayView {
public:
    GeometryArrayView() = default;
    GeometryArrayView(const T* data, size_t size) : data_(data), size_(size) {}

    const T* begin() const { return data_; }
    const T* end() const { return size_ == 0 ? data_ : data_ + size_; }
    const T& operator[](size_t index) const { return data_[index]; }
    const T* data() const { return data_; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

private:
    const T* data_ = nullptr;
    size_t size_ = 0;
};

class GeometryProvider {
public:
    using Point = GeometryPoint;
    template <typename T>
    using ArrayView = GeometryArrayView<T>;

    explicit GeometryProvider(TopologyObject& object) : object_(object) {}
    virtual ~GeometryProvider() = default;

    virtual GeometryResult refresh() noexcept = 0;
    virtual bool compatible() const noexcept = 0;
    virtual ArrayView<Point> points() const noexcept = 0;
    virtual ArrayView<Point> outline() const noexcept = 0;

    uint32_t topologyRevision() const noexcept { return geometryRevision_; }
    uint32_t sourceTopologyRevision() const noexcept;

protected:
    TopologyObject& object_;
    uint32_t geometryRevision_ = 0;
};

} // namespace lightgraph::geometry

#endif
