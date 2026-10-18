#include "converter.hpp"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: dataconv <input_file> <output_file>\n";
        return 2;
    }
    try {
        dataconv::convert_file(argv[1], argv[2]);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "dataconv: " << e.what() << '\n';
        return 1;
    }
}
