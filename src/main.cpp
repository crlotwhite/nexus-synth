#include <iostream>
#include <cstdlib>
#include "nexussynth/cli_interface.h"

int main(int argc, char* argv[]) {
    try {
        nexussynth::cli::CliInterface cli;
        auto result = cli.run(argc, argv);
        return result.exit_code;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}