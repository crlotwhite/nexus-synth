#include "ab_comparator.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

using namespace nexussynth::integration_test;

void print_usage(const char* program_name) {
    std::cout << "NexusSynth A/B Quality Comparison Tool\n";
    std::cout << "=====================================\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS] COMMAND\n\n";
    std::cout << "Commands:\n";
    std::cout << "  single <input_file> [reference_file]  - Run single A/B comparison\n";
    std::cout << "  batch <input_directory>               - Run batch A/B comparison\n";
    std::cout << "  config <config_file>                  - Load configuration file\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help                           - Show this help message\n";
    std::cout << "  -o, --output <path>                  - Output directory for reports\n";
    std::cout << "  -c, --config <file>                  - Configuration file path\n";
    std::cout << "  -r, --repetitions <num>              - Number of test repetitions (default: 5)\n";
    std::cout << "  -v, --verbose                        - Enable verbose output\n";
    std::cout << "  --html                               - Generate HTML report\n";
    std::cout << "  --csv                                - Generate CSV report\n";
    std::cout << "  --statistical-analysis               - Include statistical analysis\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " single test_input.wav reference.wav\n";
    std::cout << "  " << program_name << " batch ./test_data/ -o ./reports/ --html --csv\n";
    std::cout << "  " << program_name << " -c ab_config.json batch ./voice_banks/\n";
}

struct CommandLineArgs {
    std::string command;
    std::string input_path;
    std::string reference_path;
    std::string output_path = "./ab_reports";
    std::string config_path;
    int repetitions = 5;
    bool verbose = false;
    bool generate_html = false;
    bool generate_csv = false;
    bool statistical_analysis = false;
    bool help = false;
};

CommandLineArgs parse_arguments(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            args.help = true;
        } else if (arg == "-v" || arg == "--verbose") {
            args.verbose = true;
        } else if (arg == "--html") {
            args.generate_html = true;
        } else if (arg == "--csv") {
            args.generate_csv = true;
        } else if (arg == "--statistical-analysis") {
            args.statistical_analysis = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                args.output_path = argv[++i];
            }
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                args.config_path = argv[++i];
            }
        } else if (arg == "-r" || arg == "--repetitions") {
            if (i + 1 < argc) {
                args.repetitions = std::atoi(argv[++i]);
            }
        } else if (args.command.empty()) {
            args.command = arg;
        } else if (args.input_path.empty()) {
            args.input_path = arg;
        } else if (args.reference_path.empty()) {
            args.reference_path = arg;
        }
    }
    
    return args;
}

int run_single_comparison(const CommandLineArgs& args) {
    ABComparator comparator;
    
    // Load configuration if specified
    if (!args.config_path.empty()) {
        if (!comparator.load_config(args.config_path)) {
            std::cerr << "Error: Failed to load configuration file: " << args.config_path << std::endl;
            return 1;
        }
    }
    
    std::cout << "Running A/B comparison on: " << args.input_path << std::endl;
    if (!args.reference_path.empty()) {
        std::cout << "Reference file: " << args.reference_path << std::endl;
    }
    
    auto result = comparator.compare_single_test(args.input_path, args.reference_path);
    
    if (!result.comparison_successful) {
        std::cerr << "Comparison failed: " << result.error_message << std::endl;
        return 1;
    }
    
    // Display results
    std::cout << "\n" << result.detailed_report << std::endl;
    
    // Generate reports if requested
    std::filesystem::create_directories(args.output_path);
    
    if (args.generate_html) {
        std::vector<ABComparisonResult> results = {result};
        std::string html_path = args.output_path + "/ab_comparison_report.html";
        if (comparator.generate_html_report(results, html_path)) {
            std::cout << "HTML report saved to: " << html_path << std::endl;
        }
    }
    
    if (args.generate_csv) {
        std::vector<ABComparisonResult> results = {result};
        std::string csv_path = args.output_path + "/ab_comparison_report.csv";
        if (comparator.generate_csv_report(results, csv_path)) {
            std::cout << "CSV report saved to: " << csv_path << std::endl;
        }
    }
    
    return 0;
}

int run_batch_comparison(const CommandLineArgs& args) {
    ABComparator comparator;
    
    // Load configuration if specified
    if (!args.config_path.empty()) {
        if (!comparator.load_config(args.config_path)) {
            std::cerr << "Error: Failed to load configuration file: " << args.config_path << std::endl;
            return 1;
        }
    }
    
    // Find all audio files in input directory
    std::vector<std::string> input_files;
    
    if (!std::filesystem::exists(args.input_path)) {
        std::cerr << "Error: Input directory does not exist: " << args.input_path << std::endl;
        return 1;
    }
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(args.input_path)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".wav" || ext == ".flac" || ext == ".ogg") {
                input_files.push_back(entry.path().string());
            }
        }
    }
    
    if (input_files.empty()) {
        std::cerr << "Error: No audio files found in: " << args.input_path << std::endl;
        return 1;
    }
    
    std::cout << "Found " << input_files.size() << " audio files for batch comparison" << std::endl;
    
    // Create output directory
    std::filesystem::create_directories(args.output_path);
    
    // Run batch comparison
    std::string report_base = args.output_path + "/batch_comparison_report";
    auto results = comparator.compare_batch(input_files, report_base);
    
    if (results.empty()) {
        std::cerr << "Error: No successful comparisons completed" << std::endl;
        return 1;
    }
    
    std::cout << "\\nBatch comparison completed. " << results.size() << " tests run." << std::endl;
    
    // Generate statistical analysis if requested
    if (args.statistical_analysis) {
        std::string analysis_report;
        if (comparator.perform_statistical_analysis(results, analysis_report)) {
            std::cout << "\\n" << analysis_report << std::endl;
            
            // Save analysis to file
            std::string analysis_path = args.output_path + "/statistical_analysis.txt";
            std::ofstream analysis_file(analysis_path);
            if (analysis_file.is_open()) {
                analysis_file << analysis_report;
                std::cout << "Statistical analysis saved to: " << analysis_path << std::endl;
            }
        }
    }
    
    // Count wins
    int system_a_wins = 0, system_b_wins = 0, ties = 0;
    for (const auto& result : results) {
        if (result.winner == "NexusSynth") system_a_wins++;
        else if (result.winner == "moresampler") system_b_wins++;
        else ties++;
    }
    
    std::cout << "\\nSummary:" << std::endl;
    std::cout << "  NexusSynth wins: " << system_a_wins << " (" 
              << (100.0 * system_a_wins / results.size()) << "%)" << std::endl;
    std::cout << "  moresampler wins: " << system_b_wins << " (" 
              << (100.0 * system_b_wins / results.size()) << "%)" << std::endl;
    std::cout << "  Ties: " << ties << " (" 
              << (100.0 * ties / results.size()) << "%)" << std::endl;
    
    return 0;
}

int main(int argc, char* argv[]) {
    auto args = parse_arguments(argc, argv);
    
    if (args.help || args.command.empty()) {
        print_usage(argv[0]);
        return args.help ? 0 : 1;
    }
    
    try {
        if (args.command == "single") {
            if (args.input_path.empty()) {
                std::cerr << "Error: Input file required for single comparison" << std::endl;
                print_usage(argv[0]);
                return 1;
            }
            return run_single_comparison(args);
        } else if (args.command == "batch") {
            if (args.input_path.empty()) {
                std::cerr << "Error: Input directory required for batch comparison" << std::endl;
                print_usage(argv[0]);
                return 1;
            }
            return run_batch_comparison(args);
        } else {
            std::cerr << "Error: Unknown command: " << args.command << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}