#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <cstdint>
#include <type_traits>
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
        
        // Chunk type identifiers
        constexpr uint32_t CHUNK_HEADER = 0x52444548;     // 'HEDR'
        constexpr uint32_t CHUNK_METADATA = 0x4154454D;   // 'META'
        constexpr uint32_t CHUNK_INDEX = 0x58444E49;      // 'INDX'
        constexpr uint32_t CHUNK_MODELS = 0x4C444F4D;     // 'MODL'
        constexpr uint32_t CHUNK_CONTEXT = 0x54585443;    // 'CTXT'
        constexpr uint32_t CHUNK_CHECKSUM = 0x4D555348;   // 'HSUM'
        constexpr uint32_t CHUNK_CUSTOM = 0x4D545543;     // 'CUTM'
        
        // Data alignment
        constexpr size_t ALIGNMENT = 8;  // 8-byte alignment for performance
        constexpr size_t HEADER_SIZE = 64; // Fixed header size
        
        // Limits
        constexpr size_t MAX_MODEL_NAME_LENGTH = 256;
        constexpr size_t MAX_MODELS_PER_FILE = 65536;
        constexpr size_t MAX_CHUNK_SIZE = 0x7FFFFFFF; // 2GB max chunk size
    }

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
        void set_checksum(bool enabled) { checksum_enabled_ = enabled; }
        bool verify_integrity() const;
        
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
        
        // Version compatibility
        bool is_version_supported(uint32_t version);
        std::string version_to_string(uint32_t version);
        uint32_t version_from_string(const std::string& version_str);
        
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