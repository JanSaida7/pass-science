#pragma once

#include "core/types.hpp"
#include <string>

namespace workbench::parser {

/**
 * High-performance parser and schema validator for ANSYS Nodal CSV temperature exports.
 */
class AnsysParser {
public:
    /**
     * Parses an ANSYS nodal CSV file from the given disk path.
     */
    static core::Result<core::AnsysDataset> parse_file(const std::string& filepath);

    /**
     * Parses ANSYS CSV content directly from an in-memory string.
     */
    static core::Result<core::AnsysDataset> parse_string(const std::string& content);
};

} // namespace workbench::parser

