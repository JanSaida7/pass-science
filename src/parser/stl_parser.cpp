#include "parser/stl_parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <limits>
#include <chrono>

namespace workbench::parser {

static inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

core::Result<core::StlMesh> StlParser::parse_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return core::Result<core::StlMesh>::Fail("Unable to open STL file at: " + filepath);
    }

    // Inspect initial bytes to determine if it is binary
    char header_buffer[84];
    file.read(header_buffer, sizeof(header_buffer));
    std::streamsize bytes_read = file.gcount();

    if (bytes_read == 0) {
        return core::Result<core::StlMesh>::Fail("File is empty (0 bytes). An ASCII STL must begin with 'solid <name>' and contain facets.");
    }

    // Check for binary STL characteristics (null bytes in first 80 bytes or starts with binary header)
    for (int i = 0; i < bytes_read && i < 80; ++i) {
        if (header_buffer[i] == '\0') {
            return core::Result<core::StlMesh>::Fail("Binary STL format detected. Only ASCII STL geometry files are supported as per specification.");
        }
    }

    // Rewind to beginning and read entire file into memory for fast parsing
    file.clear();
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    constexpr size_t MAX_TESTED_SIZE = 100 * 1024 * 1024; // 100 MB tested limit
    if (file_size > MAX_TESTED_SIZE) {
        return core::Result<core::StlMesh>::Fail("STL file exceeds maximum tested limit of 100 MB (" + std::to_string(file_size) + " bytes).");
    }

    std::string content;
    content.resize(file_size);
    file.read(&content[0], file_size);

    return parse_string(content);
}

core::Result<core::StlMesh> StlParser::parse_string(const std::string& content) {
    auto start_time = std::chrono::high_resolution_clock::now();

    if (content.empty()) {
        return core::Result<core::StlMesh>::Fail("Empty STL content provided.");
    }

    std::istringstream stream(content);
    std::string line;
    size_t line_number = 0;

    core::StlMesh mesh;
    mesh.min_bounds = { std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max() };
    mesh.max_bounds = { std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest() };

    bool in_solid = false;
    bool in_facet = false;
    bool in_loop = false;
    core::StlFacet current_facet{};
    std::vector<core::Vector3> loop_vertices;
    loop_vertices.reserve(3);

    while (std::getline(stream, line)) {
        line_number++;
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        std::istringstream ls(trimmed);
        std::string token;
        ls >> token;
        std::string lower_token = to_lower(token);

        if (lower_token == "solid") {
            if (in_solid) {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": Unexpected nested 'solid' declaration.");
            }
            in_solid = true;
            std::string name;
            std::getline(ls, name);
            mesh.solid_name = trim(name);
            if (mesh.solid_name.empty()) mesh.solid_name = "UnnamedSolid";
        }
        else if (lower_token == "endsolid") {
            if (!in_solid) {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": 'endsolid' found without matching 'solid'.");
            }
            in_solid = false;
        }
        else if (lower_token == "facet") {
            std::string normal_token;
            ls >> normal_token;
            if (to_lower(normal_token) != "normal") {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": Expected 'normal' after 'facet'.");
            }
            double nx, ny, nz;
            if (!(ls >> nx >> ny >> nz)) {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": Malformed normal coordinates in facet.");
            }
            if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": Non-finite (NaN/Inf) normal values detected.");
            }
            current_facet.normal = { nx, ny, nz };
            in_facet = true;
            loop_vertices.clear();
        }
        else if (lower_token == "outer") {
            std::string loop_token;
            ls >> loop_token;
            if (to_lower(loop_token) != "loop") {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": Expected 'outer loop'.");
            }
            in_loop = true;
        }
        else if (lower_token == "vertex") {
            if (!in_loop) {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": 'vertex' declared outside of an active loop.");
            }
            double vx, vy, vz;
            if (!(ls >> vx >> vy >> vz)) {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": Malformed vertex coordinates.");
            }
            if (!std::isfinite(vx) || !std::isfinite(vy) || !std::isfinite(vz)) {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": Non-finite vertex coordinates (NaN/Inf).");
            }

            core::Vector3 v{ vx, vy, vz };
            loop_vertices.push_back(v);

            // Update bounding box
            mesh.min_bounds.x = std::min(mesh.min_bounds.x, vx);
            mesh.min_bounds.y = std::min(mesh.min_bounds.y, vy);
            mesh.min_bounds.z = std::min(mesh.min_bounds.z, vz);

            mesh.max_bounds.x = std::max(mesh.max_bounds.x, vx);
            mesh.max_bounds.y = std::max(mesh.max_bounds.y, vy);
            mesh.max_bounds.z = std::max(mesh.max_bounds.z, vz);
        }
        else if (lower_token == "endloop") {
            if (!in_loop) {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": 'endloop' found without matching 'outer loop'.");
            }
            if (loop_vertices.size() != 3) {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": Triangular facet must have exactly 3 vertices, found " + std::to_string(loop_vertices.size()));
            }
            current_facet.v1 = loop_vertices[0];
            current_facet.v2 = loop_vertices[1];
            current_facet.v3 = loop_vertices[2];
            in_loop = false;
        }
        else if (lower_token == "endfacet") {
            if (!in_facet) {
                return core::Result<core::StlMesh>::Fail("Line " + std::to_string(line_number) + ": 'endfacet' found without matching 'facet'.");
            }
            mesh.facets.push_back(current_facet);
            in_facet = false;
        }
    }

    if (mesh.facets.empty()) {
        return core::Result<core::StlMesh>::Fail("The STL file parsed successfully but contains 0 facets. Valid geometry must contain at least one triangular facet.");
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    mesh.parse_duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return core::Result<core::StlMesh>::Ok(mesh);
}

} // namespace workbench::parser
