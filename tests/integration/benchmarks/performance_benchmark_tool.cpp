#include "performance_benchmark.h"
#include "benchmark_data_collector.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>

using namespace nexussynth::integration_test;

void print_usage(const char* program_name) {
    std::cout << "NexusSynth Performance Benchmark Tool\n";
    std::cout << "====================================\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS] COMMAND\n\n";
    std::cout << "Commands:\n";
    std::cout << "  single <benchmark_name>              - Run single benchmark\n";
    std::cout << "  suite <suite_name>                   - Run benchmark suite\n";
    std::cout << "  custom <config_file>                 - Run custom benchmark configuration\n";
    std::cout << "  compare <baseline_file>              - Compare against baseline results\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help                          - Show this help message\n";
    std::cout << "  -o, --output <path>                 - Output directory for results\n";
    std::cout << "  -c, --config <file>                 - Configuration file path\n";
    std::cout << "  -i, --iterations <num>              - Number of benchmark iterations (default: 50)\n";
    std::cout << "  -w, --warmup <num>                  - Number of warmup iterations (default: 5)\n";
    std::cout << "  -t, --threads <num>                 - Number of concurrent threads (default: auto)\n";
    std::cout << "  -v, --verbose                       - Enable verbose output\n";
    std::cout << "  --json                              - Generate JSON report\n";
    std::cout << "  --csv                               - Generate CSV report\n";
    std::cout << "  --html                              - Generate HTML report\n";
    std::cout << "  --no-outliers                       - Disable outlier detection\n\n";
    std::cout << "Benchmark Suites:\n";
    std::cout << "  basic       - Basic performance tests (timing, memory)\n";
    std::cout << "  quality     - Quality-focused benchmarks (formant, pitch)\n";
    std::cout << "  scalability - Multi-threaded scalability tests\n";
    std::cout << "  full        - Complete benchmark suite\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " single phoneme_synthesis\n";
    std::cout << "  " << program_name << " suite full --html --csv -o ./reports/\n";
    std::cout << "  " << program_name << " compare baseline.json -v\n";
    std::cout << "  " << program_name << " custom my_benchmarks.json --iterations 100\n";
}

struct CommandLineArgs {
    std::string command;
    std::string target;
    std::string output_path = "./benchmark_results";
    std::string config_path;
    std::string baseline_path;
    int iterations = 50;
    int warmup_iterations = 5;
    int thread_count = 0; // 0 means auto-detect
    bool verbose = false;
    bool generate_json = false;
    bool generate_csv = false;
    bool generate_html = false;
    bool enable_outlier_detection = true;
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
        } else if (arg == "--json") {
            args.generate_json = true;
        } else if (arg == "--csv") {
            args.generate_csv = true;
        } else if (arg == "--html") {
            args.generate_html = true;
        } else if (arg == "--no-outliers") {
            args.enable_outlier_detection = false;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                args.output_path = argv[++i];
            }
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                args.config_path = argv[++i];
            }
        } else if (arg == "-i" || arg == "--iterations") {
            if (i + 1 < argc) {
                args.iterations = std::atoi(argv[++i]);
            }
        } else if (arg == "-w" || arg == "--warmup") {
            if (i + 1 < argc) {
                args.warmup_iterations = std::atoi(argv[++i]);
            }
        } else if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) {
                args.thread_count = std::atoi(argv[++i]);
            }
        } else if (args.command.empty()) {
            args.command = arg;
        } else if (args.target.empty()) {
            args.target = arg;
        } else if (args.baseline_path.empty() && args.command == "compare") {
            args.baseline_path = args.target;
            args.target = "";
        }
    }
    
    // Enable at least one output format by default
    if (!args.generate_json && !args.generate_csv && !args.generate_html) {
        args.generate_json = true;
        args.generate_csv = true;
    }
    
    return args;
}

BenchmarkConfig create_benchmark_config(const CommandLineArgs& args) {
    BenchmarkConfig config;
    
    config.measurement_iterations = args.iterations;
    config.warmup_iterations = args.warmup_iterations;
    config.enable_outlier_detection = args.enable_outlier_detection;
    
    if (args.thread_count > 0) {
        config.concurrent_threads = args.thread_count;
    }
    
    return config;
}

// Mock benchmark functions for demonstration
void mock_phoneme_synthesis() {
    // Simulate synthesis workload
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Simulate some CPU work
    volatile double result = 0.0;
    for (int i = 0; i < 100000; ++i) {
        result += std::sin(i * 0.001);
    }
}

void mock_pitch_shift() {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    
    // Simulate memory allocation
    std::vector<float> buffer(44100, 0.5f); // 1 second of audio
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = std::sin(i * 0.001);
    }
}

void mock_voice_bank_loading() {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Simulate file I/O and memory allocation
    std::vector<std::vector<float>> voice_data;
    for (int i = 0; i < 10; ++i) {
        voice_data.emplace_back(22050, static_cast<float>(i) * 0.1f);
    }
}

int run_single_benchmark(const CommandLineArgs& args) {
    auto config = create_benchmark_config(args);
    PerformanceBenchmarkFramework framework(config);
    BenchmarkDataCollector collector;
    
    if (args.verbose) {
        std::cout << "Running single benchmark: " << args.target << std::endl;
        std::cout << "Iterations: " << args.iterations << ", Warmup: " << args.warmup_iterations << std::endl;
    }
    
    BenchmarkResult result;
    
    // Select benchmark function based on target
    if (args.target == "phoneme_synthesis") {
        result = framework.run_single_benchmark(mock_phoneme_synthesis, "phoneme_synthesis", "single_phoneme");
    } else if (args.target == "pitch_shift") {
        result = framework.run_single_benchmark(mock_pitch_shift, "pitch_shift", "2x_pitch_up");
    } else if (args.target == "voice_bank_loading") {
        result = framework.run_single_benchmark(mock_voice_bank_loading, "voice_bank_loading", "standard_bank");
    } else {
        std::cerr << "Unknown benchmark target: " << args.target << std::endl;
        return 1;
    }
    
    if (!result.benchmark_successful) {
        std::cerr << "Benchmark failed: " << result.error_message << std::endl;
        return 1;
    }
    
    // Display results
    std::cout << "\nBenchmark Results for " << result.benchmark_name << ":\n";
    std::cout << "  Avg execution time: " << result.avg_execution_time.count() << " ns\n";
    std::cout << "  Min execution time: " << result.min_execution_time.count() << " ns\n";
    std::cout << "  Max execution time: " << result.max_execution_time.count() << " ns\n";
    std::cout << "  Avg memory usage: " << result.avg_memory_usage << " bytes\n";
    std::cout << "  Peak memory usage: " << result.peak_allocation << " bytes\n";
    
    // Save results
    std::vector<BenchmarkResult> results = {result};
    collector.set_output_directory(args.output_path);
    
    if (args.generate_json) collector.enable_format(SerializationFormat::JSON);
    if (args.generate_csv) collector.enable_format(SerializationFormat::CSV);
    
    std::string output_base = args.output_path + "/" + args.target + "_benchmark";
    if (collector.save_results(results, output_base)) {
        std::cout << "Results saved to: " << args.output_path << std::endl;
    } else {
        std::cerr << "Failed to save results" << std::endl;
        return 1;
    }
    
    return 0;
}

int run_benchmark_suite(const CommandLineArgs& args) {
    auto config = create_benchmark_config(args);
    PerformanceBenchmarkFramework framework(config);
    BenchmarkDataCollector collector;
    
    if (args.verbose) {
        std::cout << "Running benchmark suite: " << args.target << std::endl;
    }
    
    std::vector<std::pair<std::function<void()>, std::string>> benchmarks;
    
    // Configure benchmark suite based on target
    if (args.target == "basic" || args.target == "full") {
        benchmarks.emplace_back(mock_phoneme_synthesis, "phoneme_synthesis");
        benchmarks.emplace_back(mock_pitch_shift, "pitch_shift");
        benchmarks.emplace_back(mock_voice_bank_loading, "voice_bank_loading");
    }
    
    if (args.target == "quality" || args.target == "full") {
        // Add quality-focused benchmarks
        benchmarks.emplace_back(mock_phoneme_synthesis, "quality_phoneme_synthesis");
        benchmarks.emplace_back(mock_pitch_shift, "quality_pitch_shift");
    }
    
    if (args.target == "scalability" || args.target == "full") {
        // Add scalability benchmarks (would test different thread counts)
        benchmarks.emplace_back(mock_phoneme_synthesis, "scalability_synthesis");
    }
    
    if (benchmarks.empty()) {
        std::cerr << "Unknown benchmark suite: " << args.target << std::endl;
        return 1;
    }
    
    std::cout << "Running " << benchmarks.size() << " benchmarks..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    auto results = framework.run_benchmark_suite(benchmarks);
    auto end_time = std::chrono::steady_clock::now();
    
    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    // Display summary
    int successful = 0;
    for (const auto& result : results) {
        if (result.benchmark_successful) {
            successful++;
        } else {
            std::cerr << "FAILED: " << result.benchmark_name << " - " << result.error_message << std::endl;
        }
    }
    
    std::cout << "\nSuite completed in " << total_time.count() << " seconds\n";
    std::cout << "Successfully completed: " << successful << "/" << results.size() << " benchmarks\n";
    
    // Save results
    collector.set_output_directory(args.output_path);
    
    if (args.generate_json) collector.enable_format(SerializationFormat::JSON);
    if (args.generate_csv) collector.enable_format(SerializationFormat::CSV);
    
    std::string output_base = args.output_path + "/" + args.target + "_suite_results";
    if (collector.save_results(results, output_base)) {
        std::cout << "Results saved to: " << args.output_path << std::endl;
    } else {
        std::cerr << "Failed to save results" << std::endl;
        return 1;
    }
    
    return successful == results.size() ? 0 : 1;
}

int main(int argc, char* argv[]) {
    auto args = parse_arguments(argc, argv);
    
    if (args.help || args.command.empty()) {
        print_usage(argv[0]);
        return args.help ? 0 : 1;
    }
    
    // Create output directory
    try {
        std::filesystem::create_directories(args.output_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create output directory: " << e.what() << std::endl;
        return 1;
    }
    
    try {
        if (args.command == "single") {
            if (args.target.empty()) {
                std::cerr << "Error: Benchmark name required for single benchmark" << std::endl;
                print_usage(argv[0]);
                return 1;
            }
            return run_single_benchmark(args);
        } else if (args.command == "suite") {
            if (args.target.empty()) {
                args.target = "basic"; // Default suite
            }
            return run_benchmark_suite(args);
        } else if (args.command == "compare") {
            std::cerr << "Comparison functionality not yet implemented" << std::endl;
            return 1;
        } else if (args.command == "custom") {
            std::cerr << "Custom configuration functionality not yet implemented" << std::endl;
            return 1;
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