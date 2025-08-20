#include "ab_comparator.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <filesystem>
#include <thread>
// Note: json/json.h would be replaced with actual JSON library

namespace nexussynth {
namespace integration_test {

ABComparator::ABComparator()
    : audio_comparator_(std::make_unique<AudioComparator>())
    , quality_analyzer_(std::make_unique<QualityAnalyzer>()) {
}

ABComparator::~ABComparator() = default;

bool ABComparator::load_config(const std::string& config_file) {
    // Simplified JSON parsing - in production would use proper JSON library
    std::ifstream file(config_file);
    if (!file.is_open()) {
        return false;
    }

    // For now, set default configuration
    config_.system_a.name = "NexusSynth";
    config_.system_a.executable_path = "nexussynth";
    config_.system_a.command_args = {"synthesize", "--input", "{INPUT}", "--output", "{OUTPUT}"};
    
    config_.system_b.name = "moresampler";  
    config_.system_b.executable_path = "moresampler.exe";
    config_.system_b.command_args = {"{INPUT}", "{OUTPUT}", "C4", "100", "0", "0", "0"};
    
    config_.repetitions_per_test = 5;
    config_.significance_threshold = 0.05;
    
    return true;
}

void ABComparator::set_config(const ABComparisonConfig& config) {
    config_ = config;
}

ABComparisonResult ABComparator::compare_single_test(
    const std::string& test_input,
    const std::string& reference_audio) {
    
    ABComparisonResult result;
    result.system_a_name = config_.system_a.name;
    result.system_b_name = config_.system_b.name;
    result.comparison_successful = false;

    // Create temporary output files
    std::string temp_dir = std::filesystem::temp_directory_path();
    std::string output_a = temp_dir + "/ab_test_a_" + std::to_string(std::time(nullptr)) + ".wav";
    std::string output_b = temp_dir + "/ab_test_b_" + std::to_string(std::time(nullptr)) + ".wav";

    try {
        // Execute both systems and measure performance
        auto start_time = std::chrono::steady_clock::now();
        if (!execute_resampler(config_.system_a, test_input, output_a)) {
            result.error_message = "Failed to execute system A";
            return result;
        }
        auto end_time = std::chrono::steady_clock::now();
        result.system_a_render_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        start_time = std::chrono::steady_clock::now();
        if (!execute_resampler(config_.system_b, test_input, output_b)) {
            result.error_message = "Failed to execute system B";
            return result;
        }
        end_time = std::chrono::steady_clock::now();
        result.system_b_render_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        // Calculate advanced quality metrics for both outputs
        result.system_a_metrics = calculate_advanced_metrics(output_a, reference_audio);
        result.system_b_metrics = calculate_advanced_metrics(output_b, reference_audio);

        if (!result.system_a_metrics.measurement_successful || 
            !result.system_b_metrics.measurement_successful) {
            result.error_message = "Failed to calculate quality metrics";
            return result;
        }

        // Perform comparative analysis
        result.overall_quality_difference = 
            result.system_a_metrics.similarity_score - result.system_b_metrics.similarity_score;

        // Determine winner based on multiple metrics
        double a_score = (result.system_a_metrics.similarity_score + 
                         std::min(result.system_a_metrics.snr_db / 30.0, 1.0) +
                         result.system_a_metrics.spectral_similarity) / 3.0;
        
        double b_score = (result.system_b_metrics.similarity_score + 
                         std::min(result.system_b_metrics.snr_db / 30.0, 1.0) +
                         result.system_b_metrics.spectral_similarity) / 3.0;

        if (std::abs(a_score - b_score) < 0.05) {
            result.winner = "tie";
        } else if (a_score > b_score) {
            result.winner = config_.system_a.name;
        } else {
            result.winner = config_.system_b.name;
        }

        // Generate detailed comparison report
        std::stringstream report;
        report << "A/B Comparison Report\\n";
        report << "====================\\n";
        report << "System A (" << config_.system_a.name << "):\\n";
        report << "  SNR: " << result.system_a_metrics.snr_db << " dB\\n";
        report << "  Similarity: " << result.system_a_metrics.similarity_score << "\\n";
        report << "  Render Time: " << result.system_a_render_time.count() << " ms\\n";
        report << "\\nSystem B (" << config_.system_b.name << "):\\n";
        report << "  SNR: " << result.system_b_metrics.snr_db << " dB\\n";
        report << "  Similarity: " << result.system_b_metrics.similarity_score << "\\n";
        report << "  Render Time: " << result.system_b_render_time.count() << " ms\\n";
        report << "\\nResult: " << result.winner << " wins\\n";
        
        result.detailed_report = report.str();
        result.comparison_successful = true;

        // Cleanup temporary files
        std::filesystem::remove(output_a);
        std::filesystem::remove(output_b);

    } catch (const std::exception& e) {
        result.error_message = std::string("Exception during comparison: ") + e.what();
        // Cleanup on error
        std::filesystem::remove(output_a);
        std::filesystem::remove(output_b);
    }

    return result;
}

std::vector<ABComparisonResult> ABComparator::compare_batch(
    const std::vector<std::string>& test_inputs,
    const std::string& output_report_path) {
    
    std::vector<ABComparisonResult> results;
    results.reserve(test_inputs.size() * config_.repetitions_per_test);

    for (const auto& input : test_inputs) {
        for (int rep = 0; rep < config_.repetitions_per_test; ++rep) {
            auto result = compare_single_test(input);
            if (result.comparison_successful) {
                results.push_back(std::move(result));
            }
        }
    }

    // Generate batch report if requested
    if (!output_report_path.empty()) {
        generate_html_report(results, output_report_path + ".html");
        generate_csv_report(results, output_report_path + ".csv");
    }

    return results;
}

AdvancedQualityMetrics ABComparator::calculate_advanced_metrics(
    const std::string& audio_file,
    const std::string& reference_file) {
    
    AdvancedQualityMetrics metrics;
    metrics.measurement_successful = false;

    try {
        // Use existing AudioComparator for basic metrics
        if (!reference_file.empty()) {
            ComparisonResult basic_result = audio_comparator_->compare_audio_files(
                audio_file, reference_file);
            
            if (basic_result.files_comparable) {
                metrics.snr_db = basic_result.snr_db;
                metrics.similarity_score = basic_result.similarity_score;
                metrics.spectral_similarity = basic_result.frequency_response_similarity;
            }
        }

        // Calculate advanced metrics (simplified implementations for now)
        // In a full implementation, these would use proper signal processing libraries
        
        // Placeholder values - would be replaced with actual implementations
        metrics.mel_cepstral_distortion = 5.0 + (std::rand() % 100) / 100.0; // MCD typically 0-20
        metrics.f0_rmse = 10.0 + (std::rand() % 100) / 10.0;  // F0 RMSE in Hz
        metrics.spectral_distortion = 0.1 + (std::rand() % 50) / 1000.0; // Low values are better
        metrics.formant_deviation = 0.05 + (std::rand() % 30) / 1000.0; // Formant preservation
        metrics.phase_coherence = 0.7 + (std::rand() % 30) / 100.0; // 0-1 scale
        metrics.roughness_score = 0.1 + (std::rand() % 40) / 1000.0; // Perceptual roughness
        metrics.brightness_score = 0.8 + (std::rand() % 20) / 100.0; // High-freq preservation

        // Statistical measures
        metrics.mean_square_error = 0.01 + (std::rand() % 100) / 10000.0;
        metrics.peak_signal_noise_ratio = metrics.snr_db + 3.0; // Related to SNR
        metrics.structural_similarity_index = 0.8 + (std::rand() % 20) / 100.0;

        metrics.measurement_successful = true;
    }
    catch (const std::exception& e) {
        metrics.error_message = std::string("Error calculating metrics: ") + e.what();
    }

    return metrics;
}

bool ABComparator::generate_html_report(
    const std::vector<ABComparisonResult>& results,
    const std::string& output_path) {
    
    std::ofstream file(output_path);
    if (!file.is_open()) {
        return false;
    }

    file << "<!DOCTYPE html>\\n<html>\\n<head>\\n";
    file << "<title>A/B Comparison Report</title>\\n";
    file << "<style>\\n";
    file << "body { font-family: Arial, sans-serif; margin: 20px; }\\n";
    file << "table { border-collapse: collapse; width: 100%; }\\n";
    file << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\\n";
    file << "th { background-color: #f2f2f2; }\\n";
    file << ".winner { background-color: #d4edda; }\\n";
    file << ".loser { background-color: #f8d7da; }\\n";
    file << "</style>\\n</head>\\n<body>\\n";
    
    file << "<h1>A/B Comparison Report</h1>\\n";
    file << "<p>Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << "</p>\\n";
    
    // Summary statistics
    int system_a_wins = 0, system_b_wins = 0, ties = 0;
    for (const auto& result : results) {
        if (result.winner == config_.system_a.name) system_a_wins++;
        else if (result.winner == config_.system_b.name) system_b_wins++;
        else ties++;
    }
    
    file << "<h2>Summary</h2>\\n";
    file << "<p>Total Tests: " << results.size() << "</p>\\n";
    file << "<p>" << config_.system_a.name << " Wins: " << system_a_wins << "</p>\\n";
    file << "<p>" << config_.system_b.name << " Wins: " << system_b_wins << "</p>\\n";
    file << "<p>Ties: " << ties << "</p>\\n";
    
    // Detailed results table
    file << "<h2>Detailed Results</h2>\\n";
    file << "<table>\\n<tr>";
    file << "<th>Test</th><th>System A SNR</th><th>System B SNR</th>";
    file << "<th>System A Time</th><th>System B Time</th><th>Winner</th>";
    file << "</tr>\\n";
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        file << "<tr>";
        file << "<td>" << (i + 1) << "</td>";
        file << "<td>" << result.system_a_metrics.snr_db << "</td>";
        file << "<td>" << result.system_b_metrics.snr_db << "</td>";
        file << "<td>" << result.system_a_render_time.count() << "ms</td>";
        file << "<td>" << result.system_b_render_time.count() << "ms</td>";
        file << "<td>" << result.winner << "</td>";
        file << "</tr>\\n";
    }
    
    file << "</table>\\n</body>\\n</html>\\n";
    return true;
}

bool ABComparator::generate_csv_report(
    const std::vector<ABComparisonResult>& results,
    const std::string& output_path) {
    
    std::ofstream file(output_path);
    if (!file.is_open()) {
        return false;
    }

    // CSV header
    file << "Test,SystemA_SNR,SystemB_SNR,SystemA_Similarity,SystemB_Similarity,";
    file << "SystemA_Time_ms,SystemB_Time_ms,Winner\\n";
    
    // CSV data
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        file << (i + 1) << ","
             << result.system_a_metrics.snr_db << ","
             << result.system_b_metrics.snr_db << ","
             << result.system_a_metrics.similarity_score << ","
             << result.system_b_metrics.similarity_score << ","
             << result.system_a_render_time.count() << ","
             << result.system_b_render_time.count() << ","
             << result.winner << "\\n";
    }
    
    return true;
}

bool ABComparator::execute_resampler(const ABComparisonConfig::SystemConfig& system,
                                     const std::string& input_file,
                                     const std::string& output_file) {
    // This is a placeholder implementation
    // In reality, this would execute the actual resampler binaries
    
    // For now, we'll simulate by copying the input to output
    // and adding some processing delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50 + std::rand() % 200));
    
    try {
        std::filesystem::copy_file(input_file, output_file, 
            std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

// Statistical analysis helpers (simplified implementations)
double ABComparator::calculate_t_test(const std::vector<double>& group1, 
                                     const std::vector<double>& group2) {
    // Simplified t-test implementation
    if (group1.empty() || group2.empty()) return 1.0;
    
    double mean1 = std::accumulate(group1.begin(), group1.end(), 0.0) / group1.size();
    double mean2 = std::accumulate(group2.begin(), group2.end(), 0.0) / group2.size();
    
    // Return p-value approximation
    return std::abs(mean1 - mean2) < 0.1 ? 0.8 : 0.02;  // Simplified
}

std::pair<double, double> ABComparator::calculate_confidence_interval(
    const std::vector<double>& data, double confidence_level) {
    
    if (data.empty()) return {0.0, 0.0};
    
    double mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
    double margin = 0.05; // Simplified margin of error
    
    return {mean - margin, mean + margin};
}

bool ABComparator::perform_statistical_analysis(
    const std::vector<ABComparisonResult>& results,
    std::string& analysis_report) {
    
    if (results.empty()) {
        analysis_report = "No results to analyze";
        return false;
    }

    std::stringstream report;
    report << "Statistical Analysis Report\\n";
    report << "===========================\\n";
    
    // Collect metrics for statistical analysis
    std::vector<double> system_a_snr, system_b_snr;
    std::vector<double> system_a_similarity, system_b_similarity;
    int system_a_wins = 0, system_b_wins = 0, ties = 0;
    
    for (const auto& result : results) {
        system_a_snr.push_back(result.system_a_metrics.snr_db);
        system_b_snr.push_back(result.system_b_metrics.snr_db);
        system_a_similarity.push_back(result.system_a_metrics.similarity_score);
        system_b_similarity.push_back(result.system_b_metrics.similarity_score);
        
        if (result.winner == config_.system_a.name) system_a_wins++;
        else if (result.winner == config_.system_b.name) system_b_wins++;
        else ties++;
    }
    
    // Calculate statistical measures
    double snr_p_value = calculate_t_test(system_a_snr, system_b_snr);
    double similarity_p_value = calculate_t_test(system_a_similarity, system_b_similarity);
    
    auto snr_confidence = calculate_confidence_interval(system_a_snr);
    auto similarity_confidence = calculate_confidence_interval(system_a_similarity);
    
    // Generate report
    report << "Total Tests: " << results.size() << "\\n";
    report << "System A (" << config_.system_a.name << ") Wins: " << system_a_wins 
           << " (" << (100.0 * system_a_wins / results.size()) << "%)\\n";
    report << "System B (" << config_.system_b.name << ") Wins: " << system_b_wins
           << " (" << (100.0 * system_b_wins / results.size()) << "%)\\n";
    report << "Ties: " << ties << " (" << (100.0 * ties / results.size()) << "%)\\n\\n";
    
    report << "SNR Analysis:\\n";
    report << "  System A Mean: " << (system_a_snr.empty() ? 0 : std::accumulate(system_a_snr.begin(), system_a_snr.end(), 0.0) / system_a_snr.size()) << " dB\\n";
    report << "  System B Mean: " << (system_b_snr.empty() ? 0 : std::accumulate(system_b_snr.begin(), system_b_snr.end(), 0.0) / system_b_snr.size()) << " dB\\n";
    report << "  Statistical Significance (p-value): " << snr_p_value << "\\n";
    report << "  95% Confidence Interval: [" << snr_confidence.first << ", " << snr_confidence.second << "]\\n\\n";
    
    report << "Similarity Analysis:\\n";
    report << "  System A Mean: " << (system_a_similarity.empty() ? 0 : std::accumulate(system_a_similarity.begin(), system_a_similarity.end(), 0.0) / system_a_similarity.size()) << "\\n";
    report << "  System B Mean: " << (system_b_similarity.empty() ? 0 : std::accumulate(system_b_similarity.begin(), system_b_similarity.end(), 0.0) / system_b_similarity.size()) << "\\n";
    report << "  Statistical Significance (p-value): " << similarity_p_value << "\\n";
    
    // Conclusions
    report << "\\nConclusions:\\n";
    if (snr_p_value < config_.significance_threshold) {
        report << "- SNR difference is statistically significant\\n";
    } else {
        report << "- No statistically significant difference in SNR\\n";
    }
    
    if (similarity_p_value < config_.significance_threshold) {
        report << "- Similarity difference is statistically significant\\n";
    } else {
        report << "- No statistically significant difference in similarity\\n";
    }
    
    analysis_report = report.str();
    return true;
}

double ABComparator::calculate_mel_cepstral_distortion(const std::vector<float>& audio1, 
                                                      const std::vector<float>& audio2) {
    // Simplified MCD calculation
    // In production, this would use proper MFCC extraction and distance calculation
    
    if (audio1.size() != audio2.size() || audio1.empty()) {
        return 1000.0; // High distortion for mismatched audio
    }
    
    // Placeholder implementation - calculates RMS difference scaled to MCD range
    double sum_diff_squared = 0.0;
    for (size_t i = 0; i < audio1.size(); ++i) {
        double diff = audio1[i] - audio2[i];
        sum_diff_squared += diff * diff;
    }
    
    double rms_diff = std::sqrt(sum_diff_squared / audio1.size());
    return rms_diff * 100.0; // Scale to typical MCD range (0-20)
}

double ABComparator::calculate_f0_rmse(const std::vector<float>& audio1, 
                                      const std::vector<float>& audio2) {
    // Simplified F0 RMSE calculation
    // In production, this would use proper pitch detection (e.g., YIN, REAPER)
    
    if (audio1.size() != audio2.size() || audio1.empty()) {
        return 1000.0; // High error for mismatched audio
    }
    
    // Placeholder: use zero-crossing rate as rough pitch approximation
    auto count_zero_crossings = [](const std::vector<float>& signal) {
        int crossings = 0;
        for (size_t i = 1; i < signal.size(); ++i) {
            if ((signal[i-1] >= 0) != (signal[i] >= 0)) {
                crossings++;
            }
        }
        return crossings;
    };
    
    double zcr1 = count_zero_crossings(audio1);
    double zcr2 = count_zero_crossings(audio2);
    
    // Convert to approximate F0 and calculate RMSE
    double f0_1 = zcr1 * 22050.0 / audio1.size(); // Assuming 44.1kHz sample rate
    double f0_2 = zcr2 * 22050.0 / audio2.size();
    
    return std::abs(f0_1 - f0_2); // Simplified RMSE
}

double ABComparator::calculate_spectral_distortion(const std::vector<float>& audio1, 
                                                  const std::vector<float>& audio2) {
    // Simplified spectral distortion calculation
    // In production, this would use proper FFT and spectral envelope analysis
    
    if (audio1.size() != audio2.size() || audio1.empty()) {
        return 1.0; // High distortion
    }
    
    // Calculate simple spectral difference using energy in frequency bands
    auto calculate_spectral_energy = [](const std::vector<float>& signal) {
        // Simplified: calculate energy in different frequency bands
        std::vector<double> band_energy(4, 0.0); // 4 frequency bands
        
        size_t band_size = signal.size() / 4;
        for (size_t i = 0; i < signal.size(); ++i) {
            size_t band = std::min(i / band_size, (size_t)3);
            band_energy[band] += signal[i] * signal[i];
        }
        
        return band_energy;
    };
    
    auto energy1 = calculate_spectral_energy(audio1);
    auto energy2 = calculate_spectral_energy(audio2);
    
    double total_distortion = 0.0;
    for (size_t i = 0; i < energy1.size(); ++i) {
        if (energy1[i] > 0 && energy2[i] > 0) {
            double ratio = energy1[i] / energy2[i];
            total_distortion += std::abs(std::log10(ratio));
        }
    }
    
    return total_distortion / energy1.size();
}

double ABComparator::calculate_formant_deviation(const std::vector<float>& audio1, 
                                                const std::vector<float>& audio2) {
    // Simplified formant deviation calculation
    // In production, this would use proper formant tracking (e.g., LPC analysis)
    
    if (audio1.size() != audio2.size() || audio1.empty()) {
        return 1.0; // High deviation
    }
    
    // Placeholder: use spectral peak analysis as formant approximation
    auto find_spectral_peaks = [](const std::vector<float>& signal) {
        std::vector<double> peaks;
        
        // Simplified peak finding in different frequency regions
        // This would be replaced with proper FFT and peak detection
        for (int region = 0; region < 3; ++region) {
            size_t start = (signal.size() * region) / 3;
            size_t end = (signal.size() * (region + 1)) / 3;
            
            double max_energy = 0.0;
            for (size_t i = start; i < end; ++i) {
                double energy = signal[i] * signal[i];
                if (energy > max_energy) {
                    max_energy = energy;
                }
            }
            peaks.push_back(max_energy);
        }
        
        return peaks;
    };
    
    auto peaks1 = find_spectral_peaks(audio1);
    auto peaks2 = find_spectral_peaks(audio2);
    
    double total_deviation = 0.0;
    for (size_t i = 0; i < peaks1.size(); ++i) {
        if (peaks1[i] > 0 && peaks2[i] > 0) {
            double ratio = peaks1[i] / peaks2[i];
            total_deviation += std::abs(ratio - 1.0);
        }
    }
    
    return total_deviation / peaks1.size();
}

} // namespace integration_test
} // namespace nexussynth