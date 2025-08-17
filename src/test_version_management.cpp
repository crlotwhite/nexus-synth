#include "nexussynth/nvm_format.h"
#include <iostream>
#include <vector>
#include <string>

using namespace nexussynth::nvm;

int main() {
    std::cout << "Testing version management and compatibility system...\n";
    
    // Test SemanticVersion parsing and comparison
    std::cout << "\n=== Testing SemanticVersion ===\n";
    try {
        SemanticVersion v1_0_0 = SemanticVersion::from_string("1.0.0");
        SemanticVersion v1_1_0 = SemanticVersion::from_string("1.1.0");
        SemanticVersion v1_2_0 = SemanticVersion::from_string("1.2.0");
        SemanticVersion v2_0_0 = SemanticVersion::from_string("2.0.0");
        
        std::cout << "Version 1.0.0: " << v1_0_0.to_string() << " (0x" << std::hex << v1_0_0.to_uint32() << std::dec << ")\n";
        std::cout << "Version 1.1.0: " << v1_1_0.to_string() << " (0x" << std::hex << v1_1_0.to_uint32() << std::dec << ")\n";
        std::cout << "Version 1.2.0: " << v1_2_0.to_string() << " (0x" << std::hex << v1_2_0.to_uint32() << std::dec << ")\n";
        std::cout << "Version 2.0.0: " << v2_0_0.to_string() << " (0x" << std::hex << v2_0_0.to_uint32() << std::dec << ")\n";
        
        // Test comparisons
        std::cout << "v1_0_0 < v1_1_0: " << (v1_0_0 < v1_1_0 ? "true" : "false") << "\n";
        std::cout << "v1_1_0 < v2_0_0: " << (v1_1_0 < v2_0_0 ? "true" : "false") << "\n";
        std::cout << "v1_0_0 compatible with v1_1_0: " << (v1_0_0.is_compatible_with(v1_1_0) ? "true" : "false") << "\n";
        std::cout << "v1_0_0 compatible with v2_0_0: " << (v1_0_0.is_compatible_with(v2_0_0) ? "true" : "false") << "\n";
        
    } catch (const std::exception& e) {
        std::cout << "SemanticVersion test failed: " << e.what() << "\n";
    }
    
    // Test CompatibilityMatrix
    std::cout << "\n=== Testing CompatibilityMatrix ===\n";
    try {
        CompatibilityMatrix matrix;
        
        SemanticVersion v1_0_0 = SemanticVersion::from_string("1.0.0");
        SemanticVersion v1_1_0 = SemanticVersion::from_string("1.1.0");
        SemanticVersion v1_2_0 = SemanticVersion::from_string("1.2.0");
        SemanticVersion v2_0_0 = SemanticVersion::from_string("2.0.0");
        
        // Test compatibility checking
        auto info_1_0_to_1_1 = matrix.check_compatibility(v1_0_0, v1_1_0);
        std::cout << "1.0.0 to 1.1.0 compatibility:\n";
        std::cout << "  Fully compatible: " << (info_1_0_to_1_1.fully_compatible ? "yes" : "no") << "\n";
        std::cout << "  Backward compatible: " << (info_1_0_to_1_1.backward_compatible ? "yes" : "no") << "\n";
        std::cout << "  Forward compatible: " << (info_1_0_to_1_1.forward_compatible ? "yes" : "no") << "\n";
        std::cout << "  Migration available: " << (info_1_0_to_1_1.migration_available ? "yes" : "no") << "\n";
        std::cout << "  Notes: " << info_1_0_to_1_1.notes << "\n";
        
        auto info_1_2_to_2_0 = matrix.check_compatibility(v1_2_0, v2_0_0);
        std::cout << "\n1.2.0 to 2.0.0 compatibility:\n";
        std::cout << "  Fully compatible: " << (info_1_2_to_2_0.fully_compatible ? "yes" : "no") << "\n";
        std::cout << "  Backward compatible: " << (info_1_2_to_2_0.backward_compatible ? "yes" : "no") << "\n";
        std::cout << "  Forward compatible: " << (info_1_2_to_2_0.forward_compatible ? "yes" : "no") << "\n";
        std::cout << "  Migration available: " << (info_1_2_to_2_0.migration_available ? "yes" : "no") << "\n";
        std::cout << "  Notes: " << info_1_2_to_2_0.notes << "\n";
        
        // Test migration path
        auto path_1_0_to_2_0 = matrix.get_migration_path(v1_0_0, v2_0_0);
        std::cout << "\nMigration path from 1.0.0 to 2.0.0:\n";
        for (size_t i = 0; i < path_1_0_to_2_0.size(); ++i) {
            std::cout << "  Step " << (i + 1) << ": " << path_1_0_to_2_0[i].to_string() << "\n";
        }
        
        // Test deprecated/removed/added fields
        auto deprecated_1_1 = matrix.get_deprecated_fields(v1_1_0);
        auto removed_2_0 = matrix.get_removed_fields(v2_0_0);
        auto added_1_1 = matrix.get_added_fields(v1_1_0);
        
        std::cout << "\nDeprecated fields in 1.1.0: ";
        for (const auto& field : deprecated_1_1) {
            std::cout << field << " ";
        }
        std::cout << "\n";
        
        std::cout << "Removed fields in 2.0.0: ";
        for (const auto& field : removed_2_0) {
            std::cout << field << " ";
        }
        std::cout << "\n";
        
        std::cout << "Added fields in 1.1.0: ";
        for (const auto& field : added_1_1) {
            std::cout << field << " ";
        }
        std::cout << "\n";
        
    } catch (const std::exception& e) {
        std::cout << "CompatibilityMatrix test failed: " << e.what() << "\n";
    }
    
    // Test DeprecatedFieldHandler
    std::cout << "\n=== Testing DeprecatedFieldHandler ===\n";
    try {
        DeprecatedFieldHandler handler_warn(DeprecatedFieldHandler::DeprecationStrategy::Warn);
        DeprecatedFieldHandler handler_ignore(DeprecatedFieldHandler::DeprecationStrategy::Ignore);
        
        SemanticVersion v1_1_0 = SemanticVersion::from_string("1.1.0");
        
        std::cout << "Testing warning strategy:\n";
        handler_warn.handle_deprecated_field("legacy_compression_flag", v1_1_0);
        
        std::cout << "\nTesting field read/write decisions:\n";
        std::cout << "Should read field (warn): " << (handler_warn.should_read_field("test_field", v1_1_0) ? "yes" : "no") << "\n";
        std::cout << "Should write field (warn): " << (handler_warn.should_write_field("test_field", v1_1_0) ? "yes" : "no") << "\n";
        std::cout << "Should read field (ignore): " << (handler_ignore.should_read_field("test_field", v1_1_0) ? "yes" : "no") << "\n";
        std::cout << "Should write field (ignore): " << (handler_ignore.should_write_field("test_field", v1_1_0) ? "yes" : "no") << "\n";
        
    } catch (const std::exception& e) {
        std::cout << "DeprecatedFieldHandler test failed: " << e.what() << "\n";
    }
    
    // Test VersionManager
    std::cout << "\n=== Testing VersionManager ===\n";
    try {
        VersionManager manager;
        
        SemanticVersion current = VersionManager::get_current_version();
        SemanticVersion minimum = VersionManager::get_minimum_supported_version();
        
        std::cout << "Current version: " << current.to_string() << "\n";
        std::cout << "Minimum supported version: " << minimum.to_string() << "\n";
        
        SemanticVersion v1_0_0 = SemanticVersion::from_string("1.0.0");
        SemanticVersion v1_1_0 = SemanticVersion::from_string("1.1.0");
        SemanticVersion v2_0_0 = SemanticVersion::from_string("2.0.0");
        
        std::cout << "Is 1.0.0 supported: " << (manager.is_version_supported(v1_0_0) ? "yes" : "no") << "\n";
        std::cout << "Can read 1.1.0: " << (manager.can_read_version(v1_1_0) ? "yes" : "no") << "\n";
        std::cout << "Can write 2.0.0: " << (manager.can_write_version(v2_0_0) ? "yes" : "no") << "\n";
        
        // Test data migration
        std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto migrated_data = manager.migrate_data(test_data, v1_0_0, v1_1_0);
        
        std::cout << "Original data size: " << test_data.size() << " bytes\n";
        std::cout << "Migrated data size: " << migrated_data.size() << " bytes\n";
        std::cout << "Data migration 1.0.0->1.1.0: " << (test_data == migrated_data ? "PASSED" : "FAILED") << "\n";
        
    } catch (const std::exception& e) {
        std::cout << "VersionManager test failed: " << e.what() << "\n";
    }
    
    // Test ValidationNamespace enhanced functions
    std::cout << "\n=== Testing Enhanced Validation Functions ===\n";
    try {
        SemanticVersion v1_0_0 = SemanticVersion::from_string("1.0.0");
        SemanticVersion v1_1_0 = SemanticVersion::from_string("1.1.0");
        SemanticVersion v2_0_0 = SemanticVersion::from_string("2.0.0");
        
        bool supported = validation::is_version_supported(v1_0_0);
        std::cout << "Version 1.0.0 supported: " << (supported ? "yes" : "no") << "\n";
        
        bool can_migrate = validation::can_migrate_safely(v1_0_0, v1_1_0);
        std::cout << "Can migrate safely 1.0.0->1.1.0: " << (can_migrate ? "yes" : "no") << "\n";
        
        bool path_valid = validation::validate_migration_path(v1_0_0, v2_0_0);
        std::cout << "Migration path 1.0.0->2.0.0 valid: " << (path_valid ? "yes" : "no") << "\n";
        
        auto risks = validation::check_migration_risks(v1_0_0, v2_0_0);
        std::cout << "Migration risks 1.0.0->2.0.0:\n";
        for (const auto& risk : risks) {
            std::cout << "  - " << risk << "\n";
        }
        
        auto risks_minor = validation::check_migration_risks(v1_0_0, v1_1_0);
        std::cout << "Migration risks 1.0.0->1.1.0:\n";
        if (risks_minor.empty()) {
            std::cout << "  (No risks identified)\n";
        } else {
            for (const auto& risk : risks_minor) {
                std::cout << "  - " << risk << "\n";
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "Enhanced validation test failed: " << e.what() << "\n";
    }
    
    // Test VersionMigrator
    std::cout << "\n=== Testing VersionMigrator ===\n";
    try {
        SemanticVersion v1_0_0 = SemanticVersion::from_string("1.0.0");
        SemanticVersion v1_1_0 = SemanticVersion::from_string("1.1.0");
        
        auto migrator = VersionMigrator::create_migrator(v1_0_0, v1_1_0);
        if (migrator) {
            std::cout << "Migrator created successfully\n";
            std::cout << "Can migrate from 1.0.0: " << (migrator->can_migrate_from(v1_0_0) ? "yes" : "no") << "\n";
            std::cout << "Can migrate to 1.1.0: " << (migrator->can_migrate_to(v1_1_0) ? "yes" : "no") << "\n";
            
            // Test chunk data migration
            std::vector<uint8_t> test_chunk = {0xDE, 0xAD, 0xBE, 0xEF};
            auto migrated_chunk = migrator->migrate_chunk_data(test_chunk, constants::CHUNK_METADATA, v1_0_0, v1_1_0);
            
            std::cout << "Chunk migration test: " << (test_chunk == migrated_chunk ? "PASSED" : "FAILED") << "\n";
            
            // Test header migration
            FileHeader test_header;
            test_header.magic = constants::MAGIC_NUMBER;
            test_header.version = v1_0_0.to_uint32();
            test_header.num_chunks = 5;
            
            auto migrated_header = migrator->migrate_header(test_header, v1_0_0, v1_1_0);
            std::cout << "Header version before migration: " << SemanticVersion(test_header.version).to_string() << "\n";
            std::cout << "Header version after migration: " << SemanticVersion(migrated_header.version).to_string() << "\n";
            std::cout << "Header migration test: " << (migrated_header.version == v1_1_0.to_uint32() ? "PASSED" : "FAILED") << "\n";
            
        } else {
            std::cout << "Failed to create migrator\n";
        }
        
    } catch (const std::exception& e) {
        std::cout << "VersionMigrator test failed: " << e.what() << "\n";
    }
    
    std::cout << "\nAll version management tests completed!\n";
    return 0;
}