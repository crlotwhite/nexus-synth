#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <cstdint>
#include <type_traits>
#include <stdexcept>
#include <functional>
#include <map>
#include "hmm_structures.h"
#include "voice_metadata.h"
#include "gaussian_mixture.h"
#include "context_features.h"

namespace nexussynth {
namespace nvm {

    /**
     * @brief NVM (NexusSynth Voice Model) file format constants
     * 
     * Defines the standard constants and magic numbers used
     * in the binary .nvm file format specification
     */
    namespace constants {
        // File format identification
        constexpr uint32_t MAGIC_NUMBER = 0x314D564E;  // 'NVM1' in little-endian
        constexpr uint32_t CURRENT_VERSION = 0x00010000; // Version 1.0.0
        constexpr uint32_t MIN_SUPPORTED_VERSION = 0x00010000;
        
        // Version management constants
        constexpr uint32_t VERSION_MAJOR_MASK = 0xFFFF0000;
        constexpr uint32_t VERSION_MINOR_MASK = 0x0000FF00;
        constexpr uint32_t VERSION_PATCH_MASK = 0x000000FF;
        constexpr int VERSION_MAJOR_SHIFT = 16;
        constexpr int VERSION_MINOR_SHIFT = 8;
        constexpr int VERSION_PATCH_SHIFT = 0;
        
        // Known version milestones
        constexpr uint32_t VERSION_1_0_0 = 0x00010000;  // Initial release
        constexpr uint32_t VERSION_1_1_0 = 0x00010100;  // Enhanced compression support
        constexpr uint32_t VERSION_1_2_0 = 0x00010200;  // Extended metadata
        constexpr uint32_t VERSION_2_0_0 = 0x00020000;  // Breaking changes (future)
        
        // Chunk type identifiers
        constexpr uint32_t CHUNK_HEADER = 0x52444548;     // 'HEDR'
        constexpr uint32_t CHUNK_METADATA = 0x4154454D;   // 'META'
        constexpr uint32_t CHUNK_INDEX = 0x58444E49;      // 'INDX'
        constexpr uint32_t CHUNK_MODELS = 0x4C444F4D;     // 'MODL'
        constexpr uint32_t CHUNK_CONTEXT = 0x54585443;    // 'CTXT'
        constexpr uint32_t CHUNK_CHECKSUM = 0x4D555348;   // 'HSUM'
        constexpr uint32_t CHUNK_CUSTOM = 0x4D545543;     // 'CUTM'
        
        // Compression types
        constexpr uint32_t COMPRESSION_NONE = 0;
        constexpr uint32_t COMPRESSION_ZLIB = 1;
        constexpr uint32_t COMPRESSION_LZ4 = 2;
        
        // Checksum types
        constexpr uint32_t CHECKSUM_NONE = 0;
        constexpr uint32_t CHECKSUM_CRC32 = 1;
        constexpr uint32_t CHECKSUM_SHA256 = 2;
        
        // Data alignment
        constexpr size_t ALIGNMENT = 8;  // 8-byte alignment for performance
        constexpr size_t HEADER_SIZE = 64; // Fixed header size
        
        // Limits
        constexpr size_t MAX_MODEL_NAME_LENGTH = 256;
        constexpr size_t MAX_MODELS_PER_FILE = 65536;
        constexpr size_t MAX_CHUNK_SIZE = 0x7FFFFFFF; // 2GB max chunk size
    }

    /**
     * @brief Compression stream interface for transparent compression support
     * 
     * Provides unified interface for different compression algorithms
     * with streaming capabilities for large data files
     */
    class CompressionStream {
    public:
        enum class Algorithm {
            None = constants::COMPRESSION_NONE,
            Zlib = constants::COMPRESSION_ZLIB,
            LZ4 = constants::COMPRESSION_LZ4
        };
        
        virtual ~CompressionStream() = default;
        
        // Stream operations
        virtual bool compress(const void* input, size_t input_size, 
                            std::vector<uint8_t>& output) = 0;
        virtual bool decompress(const void* input, size_t input_size,
                              std::vector<uint8_t>& output) = 0;
        
        // Factory method
        static std::unique_ptr<CompressionStream> create(Algorithm algorithm);
        
        // Utility methods
        static Algorithm from_uint32(uint32_t value);
        static uint32_t to_uint32(Algorithm algorithm);
    };

    /**
     * @brief Zlib compression implementation
     */
    class ZlibCompressionStream : public CompressionStream {
    public:
        ZlibCompressionStream(int compression_level = 6);
        
        bool compress(const void* input, size_t input_size, 
                     std::vector<uint8_t>& output) override;
        bool decompress(const void* input, size_t input_size,
                       std::vector<uint8_t>& output) override;
        
    private:
        int compression_level_;
    };

    /**
     * @brief Checksum calculation interface for data integrity verification
     * 
     * Supports multiple checksum algorithms for ensuring file integrity
     * during storage and transmission
     */
    class ChecksumCalculator {
    public:
        enum class Algorithm {
            None = constants::CHECKSUM_NONE,
            CRC32 = constants::CHECKSUM_CRC32,
            SHA256 = constants::CHECKSUM_SHA256
        };
        
        virtual ~ChecksumCalculator() = default;
        
        // Checksum operations
        virtual void reset() = 0;
        virtual void update(const void* data, size_t size) = 0;
        virtual std::vector<uint8_t> finalize() = 0;
        
        // Convenience methods
        std::vector<uint8_t> calculate(const void* data, size_t size);
        std::string to_hex_string(const std::vector<uint8_t>& checksum);
        
        // Factory method
        static std::unique_ptr<ChecksumCalculator> create(Algorithm algorithm);
        
        // Utility methods
        static Algorithm from_uint32(uint32_t value);
        static uint32_t to_uint32(Algorithm algorithm);
        static size_t checksum_size(Algorithm algorithm);
    };

    /**
     * @brief CRC32 checksum implementation
     */
    class Crc32Calculator : public ChecksumCalculator {
    public:
        Crc32Calculator();
        
        void reset() override;
        void update(const void* data, size_t size) override;
        std::vector<uint8_t> finalize() override;
        
    private:
        uint32_t crc_;
        static const uint32_t CRC32_TABLE[256];
        
        static uint32_t calculate_crc32(const void* data, size_t size, uint32_t initial_crc = 0xFFFFFFFF);
    };

    /**
     * @brief SHA256 checksum implementation
     */
    class Sha256Calculator : public ChecksumCalculator {
    public:
        Sha256Calculator();
        
        void reset() override;
        void update(const void* data, size_t size) override;
        std::vector<uint8_t> finalize() override;
        
    private:
        struct Sha256Context;
        std::unique_ptr<Sha256Context> context_;
    };

    /**
     * @brief Binary data writer with endianness control
     * 
     * Provides standardized little-endian binary writing
     * with proper alignment and type safety
     */
    class BinaryWriter {
    public:
        explicit BinaryWriter(std::ostream& stream);
        
        // Basic type writing (always little-endian)
        void write_uint8(uint8_t value);
        void write_uint16(uint16_t value);
        void write_uint32(uint32_t value);
        void write_uint64(uint64_t value);
        void write_int8(int8_t value);
        void write_int16(int16_t value);
        void write_int32(int32_t value);
        void write_int64(int64_t value);
        void write_float(float value);
        void write_double(double value);
        
        // String writing with length prefix
        void write_string(const std::string& str);
        void write_fixed_string(const std::string& str, size_t length);
        
        // Binary data writing
        void write_bytes(const void* data, size_t size);
        void write_padding(size_t alignment = constants::ALIGNMENT);
        
        // Vector writing
        template<typename T>
        void write_vector(const std::vector<T>& vec);
        
        // Eigen matrix writing
        void write_eigen_vector(const Eigen::VectorXd& vec);
        void write_eigen_matrix(const Eigen::MatrixXd& mat);
        
        // Position and alignment
        size_t position() const;
        void align_to(size_t alignment);
        
    private:
        std::ostream& stream_;
        
        // Endianness conversion helpers
        template<typename T>
        T to_little_endian(T value) const;
    };

    /**
     * @brief Binary data reader with endianness control
     * 
     * Provides standardized little-endian binary reading
     * with proper alignment and type safety
     */
    class BinaryReader {
    public:
        explicit BinaryReader(std::istream& stream);
        
        // Basic type reading (always little-endian)
        uint8_t read_uint8();
        uint16_t read_uint16();
        uint32_t read_uint32();
        uint64_t read_uint64();
        int8_t read_int8();
        int16_t read_int16();
        int32_t read_int32();
        int64_t read_int64();
        float read_float();
        double read_double();
        
        // String reading
        std::string read_string();
        std::string read_fixed_string(size_t length);
        
        // Binary data reading
        void read_bytes(void* data, size_t size);
        void skip_padding(size_t alignment = constants::ALIGNMENT);
        
        // Vector reading
        template<typename T>
        std::vector<T> read_vector();
        
        // Eigen matrix reading
        Eigen::VectorXd read_eigen_vector();
        Eigen::MatrixXd read_eigen_matrix();
        
        // Position and validation
        size_t position() const;
        void align_to(size_t alignment);
        bool eof() const;
        
    private:
        std::istream& stream_;
        
        // Endianness conversion helpers
        template<typename T>
        T from_little_endian(T value) const;
    };

    /**
     * @brief NVM file chunk header
     * 
     * Standard chunk header used throughout the .nvm file format
     * for extensible and backward-compatible data organization
     */
    struct ChunkHeader {
        uint32_t type;          // Chunk type identifier
        uint32_t size;          // Chunk data size (excluding header)
        uint32_t version;       // Chunk format version
        uint32_t flags;         // Chunk-specific flags
        
        ChunkHeader() : type(0), size(0), version(0), flags(0) {}
        ChunkHeader(uint32_t t, uint32_t s, uint32_t v = 1, uint32_t f = 0)
            : type(t), size(s), version(v), flags(f) {}
        
        void write(BinaryWriter& writer) const;
        void read(BinaryReader& reader);
        
        static constexpr size_t HEADER_SIZE = 16;
    };

    /**
     * @brief NVM file header structure
     * 
     * Fixed-size header at the beginning of every .nvm file
     * containing format identification and global file information
     */
    struct FileHeader {
        uint32_t magic;             // Magic number ('NVM1')
        uint32_t version;           // File format version
        uint32_t num_chunks;        // Number of chunks in file
        uint32_t header_size;       // Size of this header
        uint64_t file_size;         // Total file size in bytes
        uint64_t models_offset;     // Offset to models chunk
        uint64_t metadata_offset;   // Offset to metadata chunk
        uint64_t index_offset;      // Offset to index chunk
        uint64_t creation_time;     // Unix timestamp of creation
        uint32_t checksum_type;     // Checksum algorithm (0=none, 1=CRC32, 2=SHA256)
        uint32_t compression_type;  // Compression (0=none, 1=zlib, 2=lz4)
        uint8_t reserved[8];        // Reserved for future use
        
        FileHeader();
        
        void write(BinaryWriter& writer) const;
        void read(BinaryReader& reader);
        bool is_valid() const;
        
        static constexpr size_t HEADER_SIZE = constants::HEADER_SIZE;
    };

    /**
     * @brief Model index entry for fast lookup
     * 
     * Maps model names to file offsets for efficient
     * random access to specific models in large files
     */
    struct IndexEntry {
        std::string model_name;     // Unique model identifier
        uint64_t offset;            // File offset to model data
        uint32_t size;              // Size of model data
        uint32_t context_hash;      // Hash of context features
        
        IndexEntry() : offset(0), size(0), context_hash(0) {}
        IndexEntry(const std::string& name, uint64_t off, uint32_t sz, uint32_t hash = 0)
            : model_name(name), offset(off), size(sz), context_hash(hash) {}
        
        void write(BinaryWriter& writer) const;
        void read(BinaryReader& reader);
    };

    /**
     * @brief Serialized HMM model representation
     * 
     * Complete binary serialization format for phoneme HMM models
     * including states, transitions, and context information
     */
    struct SerializedModel {
        hmm::ContextFeature context;  // Context-dependent features
        std::vector<hmm::HmmState> states;  // HMM states with GMM distributions
        std::string model_name;             // Unique model identifier
        uint32_t model_id;                 // Numeric ID for fast lookup
        
        SerializedModel() : model_id(0) {}
        
        void write(BinaryWriter& writer) const;
        void read(BinaryReader& reader);
        
        // Convert to/from runtime HMM structures
        static SerializedModel from_phoneme_hmm(const hmm::PhonemeHmm& hmm);
        hmm::PhonemeHmm to_phoneme_hmm() const;
        
        size_t calculate_size() const;
    };

    /**
     * @brief Complete NVM file format handler
     * 
     * High-level interface for reading and writing .nvm files
     * with support for compression, checksums, and version management
     */
    class NvmFile {
    public:
        NvmFile();
        ~NvmFile();
        
        // File operations
        bool create(const std::string& filename);
        bool open(const std::string& filename);
        bool save();
        bool save_as(const std::string& filename);
        void close();
        
        // Model management
        bool add_model(const hmm::PhonemeHmm& model);
        bool remove_model(const std::string& model_name);
        bool has_model(const std::string& model_name) const;
        
        hmm::PhonemeHmm get_model(const std::string& model_name) const;
        std::vector<std::string> get_model_names() const;
        size_t get_model_count() const;
        
        // Batch operations
        bool add_models(const std::vector<hmm::PhonemeHmm>& models);
        std::vector<hmm::PhonemeHmm> get_all_models() const;
        
        // Metadata operations
        metadata::VoiceMetadata& get_metadata() { return metadata_; }
        const metadata::VoiceMetadata& get_metadata() const { return metadata_; }
        void set_metadata(const metadata::VoiceMetadata& metadata);
        
        // File properties
        bool is_open() const { return is_open_; }
        const std::string& filename() const { return filename_; }
        size_t file_size() const;
        
        // Compression and integrity
        void set_compression(bool enabled) { compression_enabled_ = enabled; }
        void set_compression_algorithm(CompressionStream::Algorithm algorithm) { compression_algorithm_ = algorithm; }
        void set_checksum(bool enabled) { checksum_enabled_ = enabled; }
        void set_checksum_algorithm(ChecksumCalculator::Algorithm algorithm) { checksum_algorithm_ = algorithm; }
        bool verify_integrity() const;
        bool verify_checksums() const;
        
        // Version compatibility
        bool is_version_compatible(uint32_t version) const;
        uint32_t get_file_version() const { return header_.version; }
        
        // Statistics
        struct FileStats {
            size_t total_models;
            size_t total_states;
            size_t total_gaussians;
            size_t compressed_size;
            size_t uncompressed_size;
            double compression_ratio;
        };
        
        FileStats get_statistics() const;
        
        // Compression and checksum helpers (public for testing)
        bool compress_data(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) const;
        bool decompress_data(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) const;
        std::vector<uint8_t> calculate_data_checksum(const void* data, size_t size) const;
        bool verify_data_checksum(const void* data, size_t size, const std::vector<uint8_t>& expected_checksum) const;
        
    private:
        FileHeader header_;
        metadata::VoiceMetadata metadata_;
        std::unordered_map<std::string, SerializedModel> models_;
        std::vector<IndexEntry> index_;
        
        std::string filename_;
        bool is_open_;
        bool is_dirty_;
        bool compression_enabled_;
        bool checksum_enabled_;
        CompressionStream::Algorithm compression_algorithm_;
        ChecksumCalculator::Algorithm checksum_algorithm_;
        
        // Internal operations
        bool read_file(const std::string& filename);
        bool write_file(const std::string& filename);
        
        void read_chunk(BinaryReader& reader, const ChunkHeader& header);
        void write_chunk(BinaryWriter& writer, uint32_t type, const std::function<void()>& write_data);
        
        void read_metadata_chunk(BinaryReader& reader);
        void read_index_chunk(BinaryReader& reader);
        void read_models_chunk(BinaryReader& reader);
        
        void write_metadata_chunk(BinaryWriter& writer);
        void write_index_chunk(BinaryWriter& writer);
        void write_models_chunk(BinaryWriter& writer);
        
        void update_index();
        uint32_t calculate_checksum() const;
        
    public:
        // Utility functions
        static uint32_t hash_string(const std::string& str);
        static uint64_t current_timestamp();
    };

    /**
     * @brief Semantic version structure for version management
     * 
     * Implements semantic versioning (major.minor.patch) for .nvm files
     * with compatibility checking and migration support
     */
    struct SemanticVersion {
        uint16_t major;
        uint8_t minor;
        uint8_t patch;
        
        SemanticVersion() : major(1), minor(0), patch(0) {}
        SemanticVersion(uint16_t maj, uint8_t min, uint8_t pat) 
            : major(maj), minor(min), patch(pat) {}
        SemanticVersion(uint32_t packed_version) {
            major = (packed_version & constants::VERSION_MAJOR_MASK) >> constants::VERSION_MAJOR_SHIFT;
            minor = (packed_version & constants::VERSION_MINOR_MASK) >> constants::VERSION_MINOR_SHIFT;
            patch = (packed_version & constants::VERSION_PATCH_MASK) >> constants::VERSION_PATCH_SHIFT;
        }
        
        // Conversion methods
        uint32_t to_uint32() const {
            return (static_cast<uint32_t>(major) << constants::VERSION_MAJOR_SHIFT) |
                   (static_cast<uint32_t>(minor) << constants::VERSION_MINOR_SHIFT) |
                   (static_cast<uint32_t>(patch) << constants::VERSION_PATCH_SHIFT);
        }
        
        std::string to_string() const {
            return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
        }
        
        static SemanticVersion from_string(const std::string& version_str);
        
        // Comparison operators
        bool operator==(const SemanticVersion& other) const {
            return major == other.major && minor == other.minor && patch == other.patch;
        }
        
        bool operator!=(const SemanticVersion& other) const {
            return !(*this == other);
        }
        
        bool operator<(const SemanticVersion& other) const {
            if (major != other.major) return major < other.major;
            if (minor != other.minor) return minor < other.minor;
            return patch < other.patch;
        }
        
        bool operator<=(const SemanticVersion& other) const {
            return *this < other || *this == other;
        }
        
        bool operator>(const SemanticVersion& other) const {
            return !(*this <= other);
        }
        
        bool operator>=(const SemanticVersion& other) const {
            return !(*this < other);
        }
        
        // Compatibility checks
        bool is_compatible_with(const SemanticVersion& other) const {
            return major == other.major;  // Same major version = compatible
        }
        
        bool is_backward_compatible_with(const SemanticVersion& older) const {
            return major == older.major && *this >= older;
        }
        
        bool is_forward_compatible_with(const SemanticVersion& newer) const {
            return major == newer.major && *this <= newer;
        }
    };

    /**
     * @brief Version migration interface for handling format changes
     * 
     * Provides standardized interface for migrating data between
     * different versions of the .nvm file format
     */
    class VersionMigrator {
    public:
        virtual ~VersionMigrator() = default;
        
        // Migration capabilities
        virtual bool can_migrate_from(const SemanticVersion& from_version) const = 0;
        virtual bool can_migrate_to(const SemanticVersion& to_version) const = 0;
        
        // Data migration
        virtual std::vector<uint8_t> migrate_chunk_data(
            const std::vector<uint8_t>& input_data,
            uint32_t chunk_type,
            const SemanticVersion& from_version,
            const SemanticVersion& to_version) = 0;
        
        // Header migration
        virtual FileHeader migrate_header(
            const FileHeader& input_header,
            const SemanticVersion& from_version,
            const SemanticVersion& to_version) = 0;
        
        // Factory method
        static std::unique_ptr<VersionMigrator> create_migrator(
            const SemanticVersion& from_version,
            const SemanticVersion& to_version);
    };

    /**
     * @brief Backward compatibility matrix for version management
     * 
     * Manages compatibility information and migration strategies
     * between different versions of the .nvm format
     */
    class CompatibilityMatrix {
    public:
        struct CompatibilityInfo {
            bool fully_compatible;      // No migration needed
            bool backward_compatible;   // Can read newer files with older code
            bool forward_compatible;    // Can read older files with newer code
            bool migration_available;   // Migration path exists
            std::string notes;          // Additional compatibility notes
        };
        
        CompatibilityMatrix();
        
        // Compatibility checking
        CompatibilityInfo check_compatibility(
            const SemanticVersion& current_version,
            const SemanticVersion& target_version) const;
        
        // Migration planning
        std::vector<SemanticVersion> get_migration_path(
            const SemanticVersion& from_version,
            const SemanticVersion& to_version) const;
        
        bool is_migration_safe(
            const SemanticVersion& from_version,
            const SemanticVersion& to_version) const;
        
        // Deprecated field management
        std::vector<std::string> get_deprecated_fields(const SemanticVersion& version) const;
        std::vector<std::string> get_removed_fields(const SemanticVersion& version) const;
        std::vector<std::string> get_added_fields(const SemanticVersion& version) const;
        
        // Version registration
        void register_version(const SemanticVersion& version, const CompatibilityInfo& info);
        
    private:
        std::map<std::pair<SemanticVersion, SemanticVersion>, CompatibilityInfo> compatibility_map_;
        std::map<SemanticVersion, std::vector<std::string>> deprecated_fields_;
        std::map<SemanticVersion, std::vector<std::string>> removed_fields_;
        std::map<SemanticVersion, std::vector<std::string>> added_fields_;
        
        void initialize_default_compatibility();
    };

    /**
     * @brief Deprecated field handler for graceful format evolution
     * 
     * Manages deprecated fields and provides strategies for handling
     * them during file reading/writing operations
     */
    class DeprecatedFieldHandler {
    public:
        enum class DeprecationStrategy {
            Ignore,      // Skip deprecated fields silently
            Warn,        // Log warnings but continue processing
            Error,       // Treat as error and fail
            Preserve,    // Keep deprecated fields for compatibility
            Convert      // Convert to new format automatically
        };
        
        DeprecatedFieldHandler(DeprecationStrategy strategy = DeprecationStrategy::Warn);
        
        // Field handling
        bool should_read_field(const std::string& field_name, const SemanticVersion& version) const;
        bool should_write_field(const std::string& field_name, const SemanticVersion& version) const;
        
        // Warning and error handling
        void handle_deprecated_field(const std::string& field_name, const SemanticVersion& version) const;
        void handle_removed_field(const std::string& field_name, const SemanticVersion& version) const;
        
        // Strategy management
        void set_strategy(DeprecationStrategy strategy) { strategy_ = strategy; }
        DeprecationStrategy get_strategy() const { return strategy_; }
        
    private:
        DeprecationStrategy strategy_;
        mutable std::vector<std::string> warned_fields_;  // Track warned fields to avoid spam
    };

    /**
     * @brief Version management utility class
     * 
     * Provides high-level version management functionality for .nvm files
     * including automatic upgrades, downgrades, and compatibility checking
     */
    class VersionManager {
    public:
        VersionManager();
        
        // Version detection
        static SemanticVersion detect_file_version(const std::string& filename);
        static SemanticVersion get_current_version() { return SemanticVersion(constants::CURRENT_VERSION); }
        static SemanticVersion get_minimum_supported_version() { return SemanticVersion(constants::MIN_SUPPORTED_VERSION); }
        
        // Compatibility checking
        bool is_version_supported(const SemanticVersion& version) const;
        bool can_read_version(const SemanticVersion& version) const;
        bool can_write_version(const SemanticVersion& version) const;
        
        // Automatic conversion
        bool upgrade_file(const std::string& filename, const SemanticVersion& target_version);
        bool downgrade_file(const std::string& filename, const SemanticVersion& target_version);
        bool convert_file(const std::string& input_filename, const std::string& output_filename, const SemanticVersion& target_version);
        
        // Migration utilities
        std::vector<uint8_t> migrate_data(
            const std::vector<uint8_t>& input_data,
            const SemanticVersion& from_version,
            const SemanticVersion& to_version);
        
        // Configuration
        void set_deprecation_strategy(DeprecatedFieldHandler::DeprecationStrategy strategy);
        void enable_automatic_migration(bool enabled) { auto_migration_enabled_ = enabled; }
        void set_backup_on_upgrade(bool enabled) { backup_on_upgrade_ = enabled; }
        
    private:
        CompatibilityMatrix compatibility_matrix_;
        DeprecatedFieldHandler deprecated_handler_;
        bool auto_migration_enabled_;
        bool backup_on_upgrade_;
        
        // Internal migration helpers
        bool perform_migration(const std::string& filename, const SemanticVersion& target_version);
        std::string create_backup_filename(const std::string& filename);
    };

    /**
     * @brief Exception classes for compression and checksum errors
     */
    class CompressionException : public std::runtime_error {
    public:
        explicit CompressionException(const std::string& message) 
            : std::runtime_error("Compression error: " + message) {}
    };

    class ChecksumException : public std::runtime_error {
    public:
        explicit ChecksumException(const std::string& message) 
            : std::runtime_error("Checksum verification failed: " + message) {}
    };

    class VersionException : public std::runtime_error {
    public:
        explicit VersionException(const std::string& message) 
            : std::runtime_error("Version error: " + message) {}
    };

    class MigrationException : public std::runtime_error {
    public:
        explicit MigrationException(const std::string& message) 
            : std::runtime_error("Migration error: " + message) {}
    };

    /**
     * @brief NVM format validation utilities
     */
    namespace validation {
        
        // File format validation
        bool is_valid_nvm_file(const std::string& filename);
        bool check_file_integrity(const std::string& filename);
        std::vector<std::string> validate_file_structure(const std::string& filename);
        
        // Model validation
        bool validate_model_data(const SerializedModel& model);
        std::vector<std::string> check_model_consistency(const std::vector<SerializedModel>& models);
        
        // Version compatibility (enhanced)
        bool is_version_supported(uint32_t version);
        bool is_version_supported(const SemanticVersion& version);
        std::string version_to_string(uint32_t version);
        uint32_t version_from_string(const std::string& version_str);
        
        // Version migration validation
        bool validate_migration_path(const SemanticVersion& from_version, const SemanticVersion& to_version);
        bool can_migrate_safely(const SemanticVersion& from_version, const SemanticVersion& to_version);
        std::vector<std::string> check_migration_risks(const SemanticVersion& from_version, const SemanticVersion& to_version);
        
        // Backward compatibility checks
        bool test_backward_compatibility(const std::string& newer_file, const SemanticVersion& older_version);
        bool test_forward_compatibility(const std::string& older_file, const SemanticVersion& newer_version);
        
        // Compression and checksum validation
        bool verify_compression_support(uint32_t compression_type);
        bool verify_checksum_support(uint32_t checksum_type);
        bool verify_file_format_integrity(const std::string& filename);
        bool test_compression_roundtrip(const std::vector<uint8_t>& test_data, 
                                       CompressionStream::Algorithm algorithm);
        bool test_checksum_consistency(const std::vector<uint8_t>& test_data,
                                      ChecksumCalculator::Algorithm algorithm);
        
    } // namespace validation

    /**
     * @brief NVM format conversion utilities
     */
    namespace conversion {
        
        // Import/export to other formats
        bool export_to_htk(const NvmFile& nvm_file, const std::string& output_dir);
        bool import_from_htk(NvmFile& nvm_file, const std::string& input_dir);
        
        // Voice bank conversion
        bool convert_utau_voicebank(const std::string& utau_path, const std::string& nvm_path);
        bool extract_to_utau_voicebank(const NvmFile& nvm_file, const std::string& output_path);
        
        // Model format conversion
        SerializedModel convert_from_legacy_format(const std::vector<uint8_t>& legacy_data);
        std::vector<uint8_t> convert_to_legacy_format(const SerializedModel& model);
        
    } // namespace conversion

} // namespace nvm
} // namespace nexussynth