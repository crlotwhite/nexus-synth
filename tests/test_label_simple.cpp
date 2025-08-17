#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cassert>
#include <filesystem>

// Simple test without external dependencies
struct SimpleHMMContext {
    std::string current_phoneme;
    std::string left_phoneme;
    std::string right_phoneme;
    std::string left_left_phoneme;
    std::string right_right_phoneme;
    double pitch_cents;
    double note_duration_ms;
    int position_in_syllable;
    int syllable_length;
    int position_in_word;
    int word_length;
    
    std::string to_hts_label() const {
        return "/A:1_1/B:" + std::to_string(position_in_syllable) + "_" + std::to_string(syllable_length) +
               "/C:" + left_left_phoneme + "-" + left_phoneme + "+" + current_phoneme + 
               "++" + right_phoneme + "+" + right_right_phoneme +
               "/D:" + std::to_string(position_in_word) + "_" + std::to_string(word_length) +
               "/E:1_1/F:3_1/G:" + std::to_string(static_cast<int>(pitch_cents)) + "_" + 
               std::to_string(static_cast<int>(note_duration_ms)) + "/H:120_1/I:0_0_0";
    }
};

struct SimpleTiming {
    std::string phoneme;
    double start_time_ms;
    double end_time_ms;
    double duration_ms;
};

bool generateSimpleLabelFile(const std::vector<SimpleHMMContext>& features,
                           const std::vector<SimpleTiming>& timing,
                           const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    for (size_t i = 0; i < features.size() && i < timing.size(); ++i) {
        // HTS format: start_time end_time hts_label
        // Times in 10ns units (multiply ms by 10000)
        long long start_time = static_cast<long long>(timing[i].start_time_ms * 10000);
        long long end_time = static_cast<long long>(timing[i].end_time_ms * 10000);
        
        file << start_time << " " << end_time << " " << features[i].to_hts_label() << "\n";
    }
    
    file.close();
    return true;
}

bool validateLabelFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    int line_count = 0;
    long long prev_end_time = -1;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        // Parse line: start_time end_time label
        std::istringstream iss(line);
        long long start_time, end_time;
        std::string label;
        
        if (!(iss >> start_time >> end_time) || !std::getline(iss, label)) {
            std::cout << "Failed to parse line: " << line << std::endl;
            return false;
        }
        
        // Validate timing
        if (start_time < 0 || end_time <= start_time) {
            std::cout << "Invalid timing: " << start_time << " to " << end_time << std::endl;
            return false;
        }
        
        // Check continuity
        if (prev_end_time >= 0 && start_time != prev_end_time) {
            std::cout << "Timing gap: " << prev_end_time << " to " << start_time << std::endl;
            return false;
        }
        
        // Validate label format
        if (label.find("/A:") == std::string::npos || label.find("/C:") == std::string::npos) {
            std::cout << "Invalid HTS label format: " << label << std::endl;
            return false;
        }
        
        prev_end_time = end_time;
        line_count++;
    }
    
    std::cout << "Validated " << line_count << " label entries" << std::endl;
    return line_count > 0;
}

int main() {
    std::cout << "Simple Label File Generator Test" << std::endl;
    
    // Create test directory
    std::string test_dir = "test_labels";
    std::filesystem::create_directories(test_dir);
    
    // Create sample data
    std::vector<SimpleHMMContext> features(3);
    std::vector<SimpleTiming> timing(3);
    
    // Feature 1: "ka"
    features[0].current_phoneme = "ka";
    features[0].left_phoneme = "sil";
    features[0].right_phoneme = "sa";
    features[0].left_left_phoneme = "sil";
    features[0].right_right_phoneme = "ki";
    features[0].pitch_cents = 0.0;
    features[0].note_duration_ms = 150.0;
    features[0].position_in_syllable = 1;
    features[0].syllable_length = 1;
    features[0].position_in_word = 1;
    features[0].word_length = 3;
    
    timing[0].phoneme = "ka";
    timing[0].start_time_ms = 0.0;
    timing[0].end_time_ms = 150.0;
    timing[0].duration_ms = 150.0;
    
    // Feature 2: "sa"
    features[1].current_phoneme = "sa";
    features[1].left_phoneme = "ka";
    features[1].right_phoneme = "ki";
    features[1].left_left_phoneme = "sil";
    features[1].right_right_phoneme = "sil";
    features[1].pitch_cents = 200.0;
    features[1].note_duration_ms = 150.0;
    features[1].position_in_syllable = 1;
    features[1].syllable_length = 1;
    features[1].position_in_word = 2;
    features[1].word_length = 3;
    
    timing[1].phoneme = "sa";
    timing[1].start_time_ms = 150.0;
    timing[1].end_time_ms = 300.0;
    timing[1].duration_ms = 150.0;
    
    // Feature 3: "ki"
    features[2].current_phoneme = "ki";
    features[2].left_phoneme = "sa";
    features[2].right_phoneme = "sil";
    features[2].left_left_phoneme = "ka";
    features[2].right_right_phoneme = "sil";
    features[2].pitch_cents = 400.0;
    features[2].note_duration_ms = 150.0;
    features[2].position_in_syllable = 1;
    features[2].syllable_length = 1;
    features[2].position_in_word = 3;
    features[2].word_length = 3;
    
    timing[2].phoneme = "ki";
    timing[2].start_time_ms = 300.0;
    timing[2].end_time_ms = 450.0;
    timing[2].duration_ms = 150.0;
    
    // Test label file generation
    std::string output_file = test_dir + "/test_simple.lab";
    std::cout << "Generating label file: " << output_file << std::endl;
    
    bool success = generateSimpleLabelFile(features, timing, output_file);
    assert(success && "Label file generation failed");
    std::cout << "✓ Label file generated successfully" << std::endl;
    
    // Test validation
    std::cout << "Validating label file..." << std::endl;
    bool valid = validateLabelFile(output_file);
    assert(valid && "Label file validation failed");
    std::cout << "✓ Label file validation passed" << std::endl;
    
    // Display sample content
    std::cout << "\nSample label file content:" << std::endl;
    std::ifstream file(output_file);
    std::string line;
    int line_num = 1;
    while (std::getline(file, line) && line_num <= 3) {
        std::cout << line_num << ": " << line << std::endl;
        line_num++;
    }
    
    // Test file statistics
    file.clear();
    file.seekg(0);
    size_t total_entries = 0;
    double total_duration = 0.0;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        long long start_time, end_time;
        if (iss >> start_time >> end_time) {
            total_duration += (end_time - start_time) / 10000.0; // Convert to ms
            total_entries++;
        }
    }
    
    std::cout << "\nFile Statistics:" << std::endl;
    std::cout << "Total entries: " << total_entries << std::endl;
    std::cout << "Total duration: " << total_duration << " ms" << std::endl;
    std::cout << "Average duration: " << (total_duration / total_entries) << " ms" << std::endl;
    
    // Cleanup
    std::filesystem::remove_all(test_dir);
    
    std::cout << "\n✓ All tests passed! Label file generation is working correctly." << std::endl;
    
    return 0;
}