#include "nexussynth/nvm_format.h"
#include <iostream>
#include <vector>
#include <string>

using namespace nexussynth::nvm;

int main() {
    std::cout << "Testing compression and checksum functionality...\n";
    
    // Test data
    std::vector<uint8_t> test_data;
    std::string test_string = "Hello, NexusSynth! This is a test of compression and checksum functionality. "
                             "We need enough data to make compression worthwhile, so let's repeat this message. "
                             "Hello, NexusSynth! This is a test of compression and checksum functionality. "
                             "We need enough data to make compression worthwhile, so let's repeat this message. "
                             "Hello, NexusSynth! This is a test of compression and checksum functionality.";
    test_data.assign(test_string.begin(), test_string.end());
    
    std::cout << "Original data size: " << test_data.size() << " bytes\n";
    
    // Test CRC32 checksum
    std::cout << "\n=== Testing CRC32 Checksum ===\n";
    try {
        auto crc32_calc = ChecksumCalculator::create(ChecksumCalculator::Algorithm::CRC32);
        if (crc32_calc) {
            auto checksum = crc32_calc->calculate(test_data.data(), test_data.size());
            std::cout << "CRC32 checksum: " << crc32_calc->to_hex_string(checksum) << "\n";
            
            // Test validation
            bool valid = validation::test_checksum_consistency(test_data, ChecksumCalculator::Algorithm::CRC32);
            std::cout << "CRC32 consistency test: " << (valid ? "PASSED" : "FAILED") << "\n";
        }
    } catch (const std::exception& e) {
        std::cout << "CRC32 test failed: " << e.what() << "\n";
    }
    
    // Test SHA256 checksum
    std::cout << "\n=== Testing SHA256 Checksum ===\n";
    try {
        auto sha256_calc = ChecksumCalculator::create(ChecksumCalculator::Algorithm::SHA256);
        if (sha256_calc) {
            auto checksum = sha256_calc->calculate(test_data.data(), test_data.size());
            std::cout << "SHA256 checksum: " << sha256_calc->to_hex_string(checksum) << "\n";
            
            // Test validation
            bool valid = validation::test_checksum_consistency(test_data, ChecksumCalculator::Algorithm::SHA256);
            std::cout << "SHA256 consistency test: " << (valid ? "PASSED" : "FAILED") << "\n";
        }
    } catch (const std::exception& e) {
        std::cout << "SHA256 test failed: " << e.what() << "\n";
    }
    
    // Test zlib compression
    std::cout << "\n=== Testing Zlib Compression ===\n";
    try {
        auto compressor = CompressionStream::create(CompressionStream::Algorithm::Zlib);
        if (compressor) {
            std::vector<uint8_t> compressed, decompressed;
            
            if (compressor->compress(test_data.data(), test_data.size(), compressed)) {
                std::cout << "Compressed size: " << compressed.size() << " bytes\n";
                std::cout << "Compression ratio: " << (double)compressed.size() / test_data.size() << "\n";
                
                if (compressor->decompress(compressed.data(), compressed.size(), decompressed)) {
                    bool identical = (test_data == decompressed);
                    std::cout << "Decompression test: " << (identical ? "PASSED" : "FAILED") << "\n";
                    
                    // Test validation function
                    bool valid = validation::test_compression_roundtrip(test_data, CompressionStream::Algorithm::Zlib);
                    std::cout << "Compression roundtrip test: " << (valid ? "PASSED" : "FAILED") << "\n";
                } else {
                    std::cout << "Decompression failed\n";
                }
            } else {
                std::cout << "Compression failed\n";
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Compression test failed: " << e.what() << "\n";
    }
    
    // Test NvmFile with compression and checksum
    std::cout << "\n=== Testing NvmFile with Compression/Checksum ===\n";
    try {
        NvmFile nvm_file;
        nvm_file.set_compression(true);
        nvm_file.set_compression_algorithm(CompressionStream::Algorithm::Zlib);
        nvm_file.set_checksum(true);
        nvm_file.set_checksum_algorithm(ChecksumCalculator::Algorithm::CRC32);
        
        // Test helper methods
        std::vector<uint8_t> compressed_data, checksum;
        bool compress_ok = nvm_file.compress_data(test_data, compressed_data);
        checksum = nvm_file.calculate_data_checksum(test_data.data(), test_data.size());
        
        std::cout << "NvmFile compression test: " << (compress_ok ? "PASSED" : "FAILED") << "\n";
        std::cout << "NvmFile checksum size: " << checksum.size() << " bytes\n";
        
        // Test checksum verification
        bool checksum_valid = nvm_file.verify_data_checksum(test_data.data(), test_data.size(), checksum);
        std::cout << "NvmFile checksum verification: " << (checksum_valid ? "PASSED" : "FAILED") << "\n";
        
    } catch (const std::exception& e) {
        std::cout << "NvmFile test failed: " << e.what() << "\n";
    }
    
    std::cout << "\nAll tests completed!\n";
    return 0;
}