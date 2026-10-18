#pragma once

#include <string>

namespace dataconv {

std::string convert_text(const std::string& input,
                         const std::string& input_extension,
                         const std::string& output_extension);

void convert_file(const std::string& input_path, const std::string& output_path);

}  // namespace dataconv
