#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <chrono>

namespace workbench::core {

/**
 * Standard Result wrapper encapsulating operation success or actionable failure message.
 */
template <typename T>
struct Result {
    bool success{false};
    std::string error_message{};
    std::optional<T> data{std::nullopt};

    static Result<T> Ok(T value) {
        Result<T> res;
        res.success = true;
        res.data = std::move(value);
        return res;
    }

    static Result<T> Fail(const std::string& msg) {
        Result<T> res;
        res.success = false;
        res.error_message = msg;
        return res;
    }
};

/**
 * 3D Vector representation for CAD geometry and point clouds.
 */
struct Vector3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    [[nodiscard]] bool is_finite() const noexcept {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }
};

/**
 * Triangular facet structure for ASCII STL geometry.
 */
struct StlFacet {
    Vector3 normal;
    Vector3 v1;
    Vector3 v2;
    Vector3 v3;

    [[nodiscard]] bool is_valid() const noexcept {
        return normal.is_finite() && v1.is_finite() && v2.is_finite() && v3.is_finite();
    }
};

/**
 * ASCII STL Mesh model container.
 */
struct StlMesh {
    std::string solid_name;
    std::vector<StlFacet> facets;
    Vector3 min_bounds;
    Vector3 max_bounds;
    double parse_duration_ms{0.0};
};

/**
 * ANSYS Nodal thermal simulation result entry.
 */
struct AnsysNode {
    int64_t node_id{0};
    double x_m{0.0};
    double y_m{0.0};
    double z_m{0.0};
    double temperature_C{0.0};

    [[nodiscard]] bool is_valid() const noexcept {
        return node_id > 0 && std::isfinite(x_m) && std::isfinite(y_m) && 
               std::isfinite(z_m) && std::isfinite(temperature_C);
    }
};

/**
 * ANSYS Point Cloud dataset container.
 */
struct AnsysDataset {
    std::vector<AnsysNode> nodes;
    Vector3 min_bounds;
    Vector3 max_bounds;
    double min_temp_C{0.0};
    double max_temp_C{0.0};
    double parse_duration_ms{0.0};
};

} // namespace workbench::core
