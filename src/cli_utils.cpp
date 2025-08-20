#include "nexussynth/cli_interface.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <thread>
#include <algorithm>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#else
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace nexussynth {
namespace cli {
namespace cli_utils {

std::vector<std::string> split_arguments(const std::string& args_string) {
    std::vector<std::string> result;
    std::istringstream iss(args_string);
    std::string token;
    
    bool in_quotes = false;
    std::string current_token;
    
    for (char c : args_string) {
        if (c == '"' && (current_token.empty() || current_token.back() != '\\')) {
            in_quotes = !in_quotes;
        } else if (c == ' ' && !in_quotes) {
            if (!current_token.empty()) {
                result.push_back(current_token);
                current_token.clear();
            }
        } else {
            current_token += c;
        }
    }
    
    if (!current_token.empty()) {
        result.push_back(current_token);
    }
    
    return result;
}

std::string join_paths(const std::vector<std::string>& paths) {
    if (paths.empty()) return "";
    
    std::ostringstream oss;
    for (size_t i = 0; i < paths.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << paths[i];
    }
    return oss.str();
}

bool is_valid_path(const std::string& path) {
    namespace fs = std::filesystem;
    
    try {
        fs::path p(path);
        return !p.empty() && (fs::exists(p) || fs::exists(p.parent_path()));
    } catch (const std::exception&) {
        return false;
    }
}

bool is_directory(const std::string& path) {
    namespace fs = std::filesystem;
    
    try {
        return fs::exists(path) && fs::is_directory(path);
    } catch (const std::exception&) {
        return false;
    }
}

bool is_utau_voicebank(const std::string& path) {
    namespace fs = std::filesystem;
    
    if (!is_directory(path)) {
        return false;
    }
    
    try {
        fs::path voicebank_path(path);
        
        // Check for oto.ini file
        fs::path oto_path = voicebank_path / "oto.ini";
        if (!fs::exists(oto_path)) {
            return false;
        }
        
        // Check for at least one audio file
        bool has_audio = false;
        std::vector<std::string> audio_extensions = {".wav", ".flac", ".aif", ".aiff"};
        
        for (const auto& entry : fs::directory_iterator(voicebank_path)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (std::find(audio_extensions.begin(), audio_extensions.end(), ext) != audio_extensions.end()) {
                    has_audio = true;
                    break;
                }
            }
        }
        
        return has_audio;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool is_nvm_file(const std::string& path) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(path) || fs::is_directory(path)) {
        return false;
    }
    
    // Check file extension
    fs::path file_path(path);
    std::string extension = file_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension != ".nvm") {
        return false;
    }
    
    // Check file header for NVM magic number
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        
        // NVM magic number: 'NVM1' in little-endian (0x314D564E)
        return magic == 0x314D564E;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string get_absolute_path(const std::string& path) {
    namespace fs = std::filesystem;
    
    try {
        return fs::absolute(path).string();
    } catch (const std::exception&) {
        return path;
    }
}

std::string get_parent_directory(const std::string& path) {
    namespace fs = std::filesystem;
    
    try {
        return fs::path(path).parent_path().string();
    } catch (const std::exception&) {
        return ".";
    }
}

std::string get_filename_without_extension(const std::string& path) {
    namespace fs = std::filesystem;
    
    try {
        return fs::path(path).stem().string();
    } catch (const std::exception&) {
        return path;
    }
}

bool create_directories_recursive(const std::string& path) {
    namespace fs = std::filesystem;
    
    try {
        return fs::create_directories(path);
    } catch (const std::exception&) {
        return false;
    }
}

std::string find_config_file(const std::string& start_path) {
    namespace fs = std::filesystem;
    
    std::vector<std::string> config_names = {
        "nexussynth.json",
        "nexussynth.config.json", 
        ".nexussynth.json",
        "config.json"
    };
    
    fs::path current_path(start_path);
    
    // Search upwards through directory tree
    while (current_path != current_path.root_path()) {
        for (const auto& config_name : config_names) {
            fs::path config_path = current_path / config_name;
            if (fs::exists(config_path) && fs::is_regular_file(config_path)) {
                return config_path.string();
            }
        }
        current_path = current_path.parent_path();
    }
    
    return "";
}

bool validate_config_file(const std::string& config_path) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(config_path) || !fs::is_regular_file(config_path)) {
        return false;
    }
    
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return false;
        }
        
        // Basic JSON validation - check for balanced braces
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        int brace_count = 0;
        bool in_string = false;
        bool escaped = false;
        
        for (char c : content) {
            if (escaped) {
                escaped = false;
                continue;
            }
            
            if (c == '\\') {
                escaped = true;
                continue;
            }
            
            if (c == '"') {
                in_string = !in_string;
                continue;
            }
            
            if (!in_string) {
                if (c == '{') brace_count++;
                else if (c == '}') brace_count--;
            }
        }
        
        return brace_count == 0;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::string> expand_glob_patterns(const std::vector<std::string>& patterns) {
    std::vector<std::string> result;
    
    for (const auto& pattern : patterns) {
        // Simple glob expansion - this is a basic implementation
        // A full implementation would use a proper glob library
        if (pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos) {
            // For now, just treat as literal path
            result.push_back(pattern);
        } else {
            result.push_back(pattern);
        }
    }
    
    return result;
}

std::string center_text(const std::string& text, size_t width) {
    if (text.length() >= width) {
        return text;
    }
    
    size_t padding = (width - text.length()) / 2;
    return std::string(padding, ' ') + text + std::string(width - text.length() - padding, ' ');
}

std::string format_table_row(const std::vector<std::string>& columns, 
                            const std::vector<size_t>& widths) {
    if (columns.size() != widths.size()) {
        return "";
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) oss << " | ";
        
        std::string cell = columns[i];
        if (cell.length() > widths[i]) {
            cell = cell.substr(0, widths[i] - 3) + "...";
        }
        
        oss << std::left << std::setw(widths[i]) << cell;
    }
    
    return oss.str();
}

void print_table(const std::vector<std::vector<std::string>>& data,
                 const std::vector<std::string>& headers) {
    if (data.empty()) return;
    
    // Calculate column widths
    std::vector<size_t> widths(data[0].size(), 0);
    
    // Check headers
    if (!headers.empty()) {
        for (size_t i = 0; i < headers.size() && i < widths.size(); ++i) {
            widths[i] = std::max(widths[i], headers[i].length());
        }
    }
    
    // Check data
    for (const auto& row : data) {
        for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].length());
        }
    }
    
    // Print headers
    if (!headers.empty()) {
        std::cout << format_table_row(headers, widths) << std::endl;
        
        // Print separator
        for (size_t i = 0; i < widths.size(); ++i) {
            if (i > 0) std::cout << "-+-";
            std::cout << std::string(widths[i], '-');
        }
        std::cout << std::endl;
    }
    
    // Print data
    for (const auto& row : data) {
        std::cout << format_table_row(row, widths) << std::endl;
    }
}

std::string get_error_suggestion(int exit_code) {
    switch (exit_code) {
        case 1:
            return "Check command arguments and try again";
        case 2:
            return "Verify that input files exist and are accessible";
        case 3:
            return "Check system resources (memory, disk space) and permissions";
        case 4:
            return "Review error messages above for specific issues";
        case 5:
            return "This appears to be a software bug - please report it";
        default:
            return "Run with --verbose for more detailed information";
    }
}

std::string format_error_context(const std::string& operation, 
                                const std::string& file_path,
                                const std::string& error_message) {
    std::ostringstream oss;
    oss << "Error during " << operation;
    if (!file_path.empty()) {
        oss << " of '" << file_path << "'";
    }
    oss << ": " << error_message;
    return oss.str();
}

std::string get_system_info() {
    std::ostringstream oss;
    
    oss << "System Information:\n";
    
    // Operating System
#ifdef _WIN32
    oss << "OS: Windows\n";
#elif __APPLE__
    oss << "OS: macOS\n";
#elif __linux__
    oss << "OS: Linux\n";
#else
    oss << "OS: Unknown\n";
#endif
    
    // CPU information
    oss << "CPU Cores: " << std::thread::hardware_concurrency() << "\n";
    
    // Memory information
    size_t available_memory = get_available_memory_mb();
    if (available_memory > 0) {
        oss << "Available Memory: " << available_memory << " MB\n";
    }
    
    // Optimal thread count
    oss << "Recommended Threads: " << get_optimal_thread_count() << "\n";
    
    return oss.str();
}

std::string get_dependency_versions() {
    std::ostringstream oss;
    
    oss << "NexusSynth Core: v1.0.0\n";
    oss << "C++ Standard: " << __cplusplus << "\n";
    
    // Would include actual dependency versions here
    oss << "WORLD Vocoder: v0.3.0 (estimated)\n";
    oss << "Eigen3: v3.4+ (estimated)\n";
    oss << "AsmJit: v1.0+ (estimated)\n";
    oss << "cJSON: v1.7+ (estimated)\n";
    oss << "zlib: v1.2+ (estimated)\n";
    
#ifdef NEXUSSYNTH_OPENMP_ENABLED
    oss << "OpenMP: Enabled\n";
#else
    oss << "OpenMP: Disabled\n";
#endif
    
    return oss.str();
}

size_t get_available_memory_mb() {
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    
    if (GlobalMemoryStatusEx(&statex)) {
        return static_cast<size_t>(statex.ullAvailPhys / (1024 * 1024));
    }
    
#elif __APPLE__
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t host_count = sizeof(vm_stat) / sizeof(natural_t);
    
    if (host_page_size(mach_host_self(), &page_size) == KERN_SUCCESS &&
        host_statistics64(mach_host_self(), HOST_VM_INFO, 
                         (host_info64_t)&vm_stat, &host_count) == KERN_SUCCESS) {
        
        uint64_t free_memory = (vm_stat.free_count + vm_stat.inactive_count) * page_size;
        return static_cast<size_t>(free_memory / (1024 * 1024));
    }
    
#elif __linux__
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<size_t>(info.freeram * info.mem_unit / (1024 * 1024));
    }
#endif
    
    return 0; // Unknown
}

int get_optimal_thread_count() {
    int cpu_count = static_cast<int>(std::thread::hardware_concurrency());
    
    if (cpu_count <= 0) {
        return 2; // Fallback
    }
    
    // For I/O heavy tasks like voice bank processing, 
    // we can use slightly more threads than CPU cores
    if (cpu_count <= 4) {
        return cpu_count;
    } else if (cpu_count <= 8) {
        return cpu_count + 2;
    } else {
        return cpu_count + 4;
    }
}

} // namespace cli_utils
} // namespace cli
} // namespace nexussynth