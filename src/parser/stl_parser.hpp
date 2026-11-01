#pragma once

#include "core/types.hpp"
#include <string>

namespace workbench::parser {

/**
 * High-performance parser and validator for ASCII STL geometry files.
 */
class StlParser {
public:
    /**
     * Parses an ASCII STL file from the given disk path.
     * Rejects binary files, malformed tokens, and non-finite numbers.
     */
    static core::Result<core::StlMesh> parse_file(const std::string& filepath);

    /**
     * Parses an ASCII STL string content directly.
     */
    static core::Result<core::StlMesh> parse_string(const std::string& content);
};

} // namespace workbench::parser
