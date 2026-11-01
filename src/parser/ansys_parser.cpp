#include "parser/ansys_parser.hpp"
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

static std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        tokens.push_back(trim(item));
    }
    return tokens;
}

core::Result<core::AnsysDataset> AnsysParser::parse_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return core::Result<core::AnsysDataset>::Fail("Unable to open ANSYS CSV file at: " + filepath);
    }

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file_size == 0) {
        return core::Result<core::AnsysDataset>::Fail("File is empty (0 bytes). Expected ANSYS nodal CSV header: 'node_id,x_m,y_m,z_m,temperature_C'");
    }

    constexpr size_t MAX_TESTED_SIZE = 100 * 1024 * 1024; // 100 MB tested limit
    if (file_size > MAX_TESTED_SIZE) {
        return core::Result<core::AnsysDataset>::Fail("CSV file exceeds maximum tested limit of 100 MB (" + std::to_string(file_size) + " bytes).");
    }

    std::string content;
    content.resize(file_size);
    file.read(&content[0], file_size);

    return parse_string(content);
}

core::Result<core::AnsysDataset> AnsysParser::parse_string(const std::string& content) {
    auto start_time = std::chrono::high_resolution_clock::now();

    if (content.empty()) {
        return core::Result<core::AnsysDataset>::Fail("Empty CSV content provided.");
    }

    std::istringstream stream(content);
    std::string line;
    size_t line_number = 0;

    // Read header line
    while (std::getline(stream, line)) {
        line_number++;
        std::string trimmed = trim(line);
        if (!trimmed.empty()) {
            break;
        }
    }

    if (line.empty()) {
        return core::Result<core::AnsysDataset>::Fail("File contains only whitespace. Expected CSV header: 'node_id,x_m,y_m,z_m,temperature_C'");
    }

    std::vector<std::string> header_tokens = split_csv_line(line);
    if (header_tokens.size() < 5) {
        return core::Result<core::AnsysDataset>::Fail("Line " + std::to_string(line_number) + ": Invalid CSV header. Expected 5 columns: 'node_id,x_m,y_m,z_m,temperature_C', but found " + std::to_string(header_tokens.size()));
    }

    // Match column names
    int col_id = -1, col_x = -1, col_y = -1, col_z = -1, col_t = -1;
    for (size_t i = 0; i < header_tokens.size(); ++i) {
        std::string col = to_lower(header_tokens[i]);
        if (col == "node_id" || col == "nodeid" || col == "id") col_id = static_cast<int>(i);
        else if (col == "x_m" || col == "x(m)" || col == "x") col_x = static_cast<int>(i);
        else if (col == "y_m" || col == "y(m)" || col == "y") col_y = static_cast<int>(i);
        else if (col == "z_m" || col == "z(m)" || col == "z") col_z = static_cast<int>(i);
        else if (col == "temperature_c" || col == "temp_c" || col == "temperature" || col == "temp") col_t = static_cast<int>(i);
    }

    if (col_id == -1 || col_x == -1 || col_y == -1 || col_z == -1 || col_t == -1) {
        return core::Result<core::AnsysDataset>::Fail("Line " + std::to_string(line_number) + ": Missing required column headers. Must contain 'node_id,x_m,y_m,z_m,temperature_C'.");
    }

    core::AnsysDataset dataset;
    dataset.min_bounds = { std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max() };
    dataset.max_bounds = { std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest() };
    dataset.min_temp_C = std::numeric_limits<double>::max();
    dataset.max_temp_C = std::numeric_limits<double>::lowest();

    while (std::getline(stream, line)) {
        line_number++;
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        std::vector<std::string> cols = split_csv_line(trimmed);
        if (cols.size() < 5) {
            return core::Result<core::AnsysDataset>::Fail("Line " + std::to_string(line_number) + ": Malformed row with insufficient columns (" + std::to_string(cols.size()) + " of 5).");
        }

        try {
            int64_t nid = std::stoll(cols[col_id]);
            double xm = std::stod(cols[col_x]);
            double ym = std::stod(cols[col_y]);
            double zm = std::stod(cols[col_z]);
            double temp = std::stod(cols[col_t]);

            if (nid <= 0) {
                return core::Result<core::AnsysDataset>::Fail("Line " + std::to_string(line_number) + ": Invalid node_id <= 0 (" + std::to_string(nid) + ").");
            }

            if (!std::isfinite(xm) || !std::isfinite(ym) || !std::isfinite(zm) || !std::isfinite(temp)) {
                return core::Result<core::AnsysDataset>::Fail("Line " + std::to_string(line_number) + ": Non-finite numerical value (NaN or Inf) detected.");
            }

            core::AnsysNode node{ nid, xm, ym, zm, temp };
            dataset.nodes.push_back(node);

            // Update spatial bounds
            dataset.min_bounds.x = std::min(dataset.min_bounds.x, xm);
            dataset.min_bounds.y = std::min(dataset.min_bounds.y, ym);
            dataset.min_bounds.z = std::min(dataset.min_bounds.z, zm);

            dataset.max_bounds.x = std::max(dataset.max_bounds.x, xm);
            dataset.max_bounds.y = std::max(dataset.max_bounds.y, ym);
            dataset.max_bounds.z = std::max(dataset.max_bounds.z, zm);

            // Update temperature bounds
            dataset.min_temp_C = std::min(dataset.min_temp_C, temp);
            dataset.max_temp_C = std::max(dataset.max_temp_C, temp);

        } catch (const std::exception& e) {
            return core::Result<core::AnsysDataset>::Fail("Line " + std::to_string(line_number) + ": Numerical parse failure: " + e.what());
        }
    }

    if (dataset.nodes.empty()) {
        return core::Result<core::AnsysDataset>::Fail("File contains valid CSV header but 0 data rows.");
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    dataset.parse_duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return core::Result<core::AnsysDataset>::Ok(dataset);
}

} // namespace workbench::parser
