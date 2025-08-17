#include "nexussynth/nvm_format.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>

namespace nexussynth {
namespace nvm {

// BinaryWriter implementation
BinaryWriter::BinaryWriter(std::ostream& stream) : stream_(stream) {
}

void BinaryWriter::write_uint8(uint8_t value) {
    stream_.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void BinaryWriter::write_uint16(uint16_t value) {
    uint16_t le_value = to_little_endian(value);
    stream_.write(reinterpret_cast<const char*>(&le_value), sizeof(le_value));
}

void BinaryWriter::write_uint32(uint32_t value) {
    uint32_t le_value = to_little_endian(value);
    stream_.write(reinterpret_cast<const char*>(&le_value), sizeof(le_value));
}

void BinaryWriter::write_uint64(uint64_t value) {
    uint64_t le_value = to_little_endian(value);
    stream_.write(reinterpret_cast<const char*>(&le_value), sizeof(le_value));
}

void BinaryWriter::write_int8(int8_t value) {
    write_uint8(static_cast<uint8_t>(value));
}

void BinaryWriter::write_int16(int16_t value) {
    write_uint16(static_cast<uint16_t>(value));
}

void BinaryWriter::write_int32(int32_t value) {
    write_uint32(static_cast<uint32_t>(value));
}

void BinaryWriter::write_int64(int64_t value) {
    write_uint64(static_cast<uint64_t>(value));
}

void BinaryWriter::write_float(float value) {
    static_assert(sizeof(float) == sizeof(uint32_t), "Float size assumption failed");
    union { float f; uint32_t i; } converter;
    converter.f = value;
    write_uint32(converter.i);
}

void BinaryWriter::write_double(double value) {
    static_assert(sizeof(double) == sizeof(uint64_t), "Double size assumption failed");
    union { double d; uint64_t i; } converter;
    converter.d = value;
    write_uint64(converter.i);
}

void BinaryWriter::write_string(const std::string& str) {
    write_uint32(static_cast<uint32_t>(str.length()));
    if (!str.empty()) {
        stream_.write(str.c_str(), str.length());
    }
    align_to(constants::ALIGNMENT);
}

void BinaryWriter::write_fixed_string(const std::string& str, size_t length) {
    std::string padded = str;
    padded.resize(length, '\0');
    stream_.write(padded.c_str(), length);
}

void BinaryWriter::write_bytes(const void* data, size_t size) {
    stream_.write(static_cast<const char*>(data), size);
}

void BinaryWriter::write_padding(size_t alignment) {
    align_to(alignment);
}

void BinaryWriter::write_eigen_vector(const Eigen::VectorXd& vec) {
    write_uint32(static_cast<uint32_t>(vec.size()));
    for (int i = 0; i < vec.size(); ++i) {
        write_double(vec[i]);
    }
}

void BinaryWriter::write_eigen_matrix(const Eigen::MatrixXd& mat) {
    write_uint32(static_cast<uint32_t>(mat.rows()));
    write_uint32(static_cast<uint32_t>(mat.cols()));
    for (int i = 0; i < mat.rows(); ++i) {
        for (int j = 0; j < mat.cols(); ++j) {
            write_double(mat(i, j));
        }
    }
}

size_t BinaryWriter::position() const {
    return static_cast<size_t>(stream_.tellp());
}

void BinaryWriter::align_to(size_t alignment) {
    size_t pos = position();
    size_t padding = (alignment - (pos % alignment)) % alignment;
    for (size_t i = 0; i < padding; ++i) {
        write_uint8(0);
    }
}

template<typename T>
T BinaryWriter::to_little_endian(T value) const {
    // Assume little-endian host for now - could be enhanced with proper detection
    return value;
}

// BinaryReader implementation
BinaryReader::BinaryReader(std::istream& stream) : stream_(stream) {
}

uint8_t BinaryReader::read_uint8() {
    uint8_t value;
    stream_.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

uint16_t BinaryReader::read_uint16() {
    uint16_t value;
    stream_.read(reinterpret_cast<char*>(&value), sizeof(value));
    return from_little_endian(value);
}

uint32_t BinaryReader::read_uint32() {
    uint32_t value;
    stream_.read(reinterpret_cast<char*>(&value), sizeof(value));
    return from_little_endian(value);
}

uint64_t BinaryReader::read_uint64() {
    uint64_t value;
    stream_.read(reinterpret_cast<char*>(&value), sizeof(value));
    return from_little_endian(value);
}

int8_t BinaryReader::read_int8() {
    return static_cast<int8_t>(read_uint8());
}

int16_t BinaryReader::read_int16() {
    return static_cast<int16_t>(read_uint16());
}

int32_t BinaryReader::read_int32() {
    return static_cast<int32_t>(read_uint32());
}

int64_t BinaryReader::read_int64() {
    return static_cast<int64_t>(read_uint64());
}

float BinaryReader::read_float() {
    union { float f; uint32_t i; } converter;
    converter.i = read_uint32();
    return converter.f;
}

double BinaryReader::read_double() {
    union { double d; uint64_t i; } converter;
    converter.i = read_uint64();
    return converter.d;
}

std::string BinaryReader::read_string() {
    uint32_t length = read_uint32();
    std::string str;
    if (length > 0) {
        str.resize(length);
        stream_.read(&str[0], length);
    }
    align_to(constants::ALIGNMENT);
    return str;
}

std::string BinaryReader::read_fixed_string(size_t length) {
    std::string str(length, '\0');
    stream_.read(&str[0], length);
    // Remove null padding
    size_t null_pos = str.find('\0');
    if (null_pos != std::string::npos) {
        str.resize(null_pos);
    }
    return str;
}

void BinaryReader::read_bytes(void* data, size_t size) {
    stream_.read(static_cast<char*>(data), size);
}

void BinaryReader::skip_padding(size_t alignment) {
    align_to(alignment);
}

Eigen::VectorXd BinaryReader::read_eigen_vector() {
    uint32_t size = read_uint32();
    Eigen::VectorXd vec(size);
    for (uint32_t i = 0; i < size; ++i) {
        vec[i] = read_double();
    }
    return vec;
}

Eigen::MatrixXd BinaryReader::read_eigen_matrix() {
    uint32_t rows = read_uint32();
    uint32_t cols = read_uint32();
    Eigen::MatrixXd mat(rows, cols);
    for (uint32_t i = 0; i < rows; ++i) {
        for (uint32_t j = 0; j < cols; ++j) {
            mat(i, j) = read_double();
        }
    }
    return mat;
}

size_t BinaryReader::position() const {
    return static_cast<size_t>(stream_.tellg());
}

void BinaryReader::align_to(size_t alignment) {
    size_t pos = position();
    size_t padding = (alignment - (pos % alignment)) % alignment;
    stream_.seekg(padding, std::ios::cur);
}

bool BinaryReader::eof() const {
    return stream_.eof();
}

template<typename T>
T BinaryReader::from_little_endian(T value) const {
    // Assume little-endian host for now - could be enhanced with proper detection
    return value;
}

// ChunkHeader implementation
void ChunkHeader::write(BinaryWriter& writer) const {
    writer.write_uint32(type);
    writer.write_uint32(size);
    writer.write_uint32(version);
    writer.write_uint32(flags);
}

void ChunkHeader::read(BinaryReader& reader) {
    type = reader.read_uint32();
    size = reader.read_uint32();
    version = reader.read_uint32();
    flags = reader.read_uint32();
}

// FileHeader implementation
FileHeader::FileHeader() {
    magic = constants::MAGIC_NUMBER;
    version = constants::CURRENT_VERSION;
    num_chunks = 0;
    header_size = HEADER_SIZE;
    file_size = 0;
    models_offset = 0;
    metadata_offset = 0;
    index_offset = 0;
    creation_time = NvmFile::current_timestamp();
    checksum_type = 0;
    compression_type = 0;
    std::memset(reserved, 0, sizeof(reserved));
}

void FileHeader::write(BinaryWriter& writer) const {
    writer.write_uint32(magic);
    writer.write_uint32(version);
    writer.write_uint32(num_chunks);
    writer.write_uint32(header_size);
    writer.write_uint64(file_size);
    writer.write_uint64(models_offset);
    writer.write_uint64(metadata_offset);
    writer.write_uint64(index_offset);
    writer.write_uint64(creation_time);
    writer.write_uint32(checksum_type);
    writer.write_uint32(compression_type);
    writer.write_bytes(reserved, sizeof(reserved));
}

void FileHeader::read(BinaryReader& reader) {
    magic = reader.read_uint32();
    version = reader.read_uint32();
    num_chunks = reader.read_uint32();
    header_size = reader.read_uint32();
    file_size = reader.read_uint64();
    models_offset = reader.read_uint64();
    metadata_offset = reader.read_uint64();
    index_offset = reader.read_uint64();
    creation_time = reader.read_uint64();
    checksum_type = reader.read_uint32();
    compression_type = reader.read_uint32();
    reader.read_bytes(reserved, sizeof(reserved));
}

bool FileHeader::is_valid() const {
    return magic == constants::MAGIC_NUMBER && 
           version >= constants::MIN_SUPPORTED_VERSION &&
           header_size == HEADER_SIZE;
}

// IndexEntry implementation
void IndexEntry::write(BinaryWriter& writer) const {
    writer.write_string(model_name);
    writer.write_uint64(offset);
    writer.write_uint32(size);
    writer.write_uint32(context_hash);
}

void IndexEntry::read(BinaryReader& reader) {
    model_name = reader.read_string();
    offset = reader.read_uint64();
    size = reader.read_uint32();
    context_hash = reader.read_uint32();
}

// SerializedModel implementation
void SerializedModel::write(BinaryWriter& writer) const {
    // Write model metadata
    writer.write_string(model_name);
    writer.write_uint32(model_id);
    
    // Write context features
    writer.write_string(context.left_phoneme);
    writer.write_string(context.current_phoneme);
    writer.write_string(context.right_phoneme);
    
    writer.write_int32(context.position_in_syllable);
    writer.write_int32(context.syllable_length);
    writer.write_int32(context.position_in_word);
    writer.write_int32(context.word_length);
    writer.write_double(context.pitch_cents);
    writer.write_double(context.note_duration_ms);
    writer.write_string(context.lyric);
    writer.write_double(context.tempo_bpm);
    writer.write_int32(context.beat_position);
    
    // Write number of states
    writer.write_uint32(static_cast<uint32_t>(states.size()));
    
    // Write each state
    for (const auto& state : states) {
        writer.write_int32(state.state_id);
        
        // Write transition probabilities
        writer.write_double(state.transition.self_loop_prob);
        writer.write_double(state.transition.next_state_prob);
        writer.write_double(state.transition.exit_prob);
        
        // Write GMM parameters
        const auto& gmm = state.output_distribution;
        writer.write_uint32(static_cast<uint32_t>(gmm.num_components()));
        writer.write_int32(gmm.dimension());
        
        // Write mixture weights
        for (size_t i = 0; i < gmm.num_components(); ++i) {
            writer.write_double(gmm.weight(i));
        }
        
        // Write Gaussian components
        for (size_t i = 0; i < gmm.num_components(); ++i) {
            const auto& component = gmm.component(i);
            writer.write_eigen_vector(component.mean());
            writer.write_eigen_matrix(component.covariance());
            writer.write_double(component.weight());
        }
    }
}

void SerializedModel::read(BinaryReader& reader) {
    // Read model metadata
    model_name = reader.read_string();
    model_id = reader.read_uint32();
    
    // Read context features
    context.left_phoneme = reader.read_string();
    context.current_phoneme = reader.read_string();
    context.right_phoneme = reader.read_string();
    
    context.position_in_syllable = reader.read_int32();
    context.syllable_length = reader.read_int32();
    context.position_in_word = reader.read_int32();
    context.word_length = reader.read_int32();
    context.pitch_cents = reader.read_double();
    context.note_duration_ms = reader.read_double();
    context.lyric = reader.read_string();
    context.tempo_bpm = reader.read_double();
    context.beat_position = reader.read_int32();
    
    // Read number of states
    uint32_t num_states = reader.read_uint32();
    states.resize(num_states);
    
    // Read each state
    for (auto& state : states) {
        state.state_id = reader.read_int32();
        
        // Read transition probabilities
        state.transition.self_loop_prob = reader.read_double();
        state.transition.next_state_prob = reader.read_double();
        state.transition.exit_prob = reader.read_double();
        
        // Read GMM parameters
        uint32_t num_components = reader.read_uint32();
        int32_t dimension = reader.read_int32();
        
        // Create new GMM
        state.output_distribution = hmm::GaussianMixture(num_components, dimension);
        
        // Read mixture weights
        std::vector<double> weights(num_components);
        for (size_t i = 0; i < num_components; ++i) {
            weights[i] = reader.read_double();
        }
        state.output_distribution.set_weights(weights);
        
        // Read Gaussian components
        for (size_t i = 0; i < num_components; ++i) {
            Eigen::VectorXd mean = reader.read_eigen_vector();
            Eigen::MatrixXd covariance = reader.read_eigen_matrix();
            double weight = reader.read_double();
            
            state.output_distribution.component(i).set_parameters(mean, covariance, weight);
        }
    }
}

SerializedModel SerializedModel::from_phoneme_hmm(const hmm::PhonemeHmm& hmm) {
    SerializedModel serialized;
    serialized.context = hmm.context;
    serialized.model_name = hmm.model_name;
    serialized.states = hmm.states;
    return serialized;
}

hmm::PhonemeHmm SerializedModel::to_phoneme_hmm() const {
    hmm::PhonemeHmm hmm;
    hmm.context = context;
    hmm.model_name = model_name;
    hmm.states = states;
    return hmm;
}

size_t SerializedModel::calculate_size() const {
    // Rough estimate - could be more precise
    size_t size = 0;
    size += model_name.length() + 4;  // String + length
    size += 4;  // model_id
    size += 200;  // Context features (estimated)
    size += 4;  // Number of states
    
    for (const auto& state : states) {
        size += 4;  // state_id
        size += 24; // Transition probabilities
        size += 8;  // GMM metadata
        
        const auto& gmm = state.output_distribution;
        size += gmm.num_components() * 8;  // Weights
        
        for (size_t i = 0; i < gmm.num_components(); ++i) {
            const auto& comp = gmm.component(i);
            size += comp.dimension() * 8;  // Mean vector
            size += comp.dimension() * comp.dimension() * 8;  // Covariance matrix
            size += 8;  // Component weight
        }
    }
    
    return size;
}

// NvmFile implementation
NvmFile::NvmFile() 
    : is_open_(false), is_dirty_(false), compression_enabled_(false), checksum_enabled_(false) {
}

NvmFile::~NvmFile() {
    close();
}

bool NvmFile::create(const std::string& filename) {
    filename_ = filename;
    is_open_ = true;
    is_dirty_ = true;
    
    // Initialize with default header
    header_ = FileHeader();
    models_.clear();
    index_.clear();
    
    return true;
}

bool NvmFile::open(const std::string& filename) {
    return read_file(filename);
}

bool NvmFile::save() {
    if (!is_open_ || filename_.empty()) {
        return false;
    }
    return write_file(filename_);
}

bool NvmFile::save_as(const std::string& filename) {
    filename_ = filename;
    return save();
}

void NvmFile::close() {
    if (is_open_ && is_dirty_) {
        // Could auto-save here if desired
    }
    
    is_open_ = false;
    is_dirty_ = false;
    filename_.clear();
    models_.clear();
    index_.clear();
}

bool NvmFile::add_model(const hmm::PhonemeHmm& model) {
    if (!is_open_) return false;
    
    SerializedModel serialized = SerializedModel::from_phoneme_hmm(model);
    serialized.model_id = static_cast<uint32_t>(models_.size());
    
    models_[model.model_name] = serialized;
    is_dirty_ = true;
    update_index();
    
    return true;
}

bool NvmFile::remove_model(const std::string& model_name) {
    if (!is_open_) return false;
    
    auto it = models_.find(model_name);
    if (it == models_.end()) return false;
    
    models_.erase(it);
    is_dirty_ = true;
    update_index();
    
    return true;
}

bool NvmFile::has_model(const std::string& model_name) const {
    return models_.find(model_name) != models_.end();
}

hmm::PhonemeHmm NvmFile::get_model(const std::string& model_name) const {
    auto it = models_.find(model_name);
    if (it != models_.end()) {
        return it->second.to_phoneme_hmm();
    }
    return hmm::PhonemeHmm();  // Empty model if not found
}

std::vector<std::string> NvmFile::get_model_names() const {
    std::vector<std::string> names;
    names.reserve(models_.size());
    
    for (const auto& pair : models_) {
        names.push_back(pair.first);
    }
    
    std::sort(names.begin(), names.end());
    return names;
}

size_t NvmFile::get_model_count() const {
    return models_.size();
}

void NvmFile::set_metadata(const metadata::VoiceMetadata& metadata) {
    metadata_ = metadata;
    is_dirty_ = true;
}

size_t NvmFile::file_size() const {
    return static_cast<size_t>(header_.file_size);
}

bool NvmFile::verify_integrity() const {
    // Simplified integrity check - could be enhanced
    return header_.is_valid() && !models_.empty();
}

bool NvmFile::is_version_compatible(uint32_t version) const {
    return version >= constants::MIN_SUPPORTED_VERSION;
}

NvmFile::FileStats NvmFile::get_statistics() const {
    FileStats stats = {};
    stats.total_models = models_.size();
    
    for (const auto& pair : models_) {
        const auto& model = pair.second;
        stats.total_states += model.states.size();
        
        for (const auto& state : model.states) {
            stats.total_gaussians += state.output_distribution.num_components();
        }
    }
    
    stats.uncompressed_size = file_size();
    stats.compressed_size = stats.uncompressed_size;  // Simplified
    stats.compression_ratio = 1.0;
    
    return stats;
}

// Private methods
bool NvmFile::read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    
    BinaryReader reader(file);
    
    // Read file header
    header_.read(reader);
    if (!header_.is_valid()) return false;
    
    // Read chunks based on offsets in header
    // This is a simplified implementation
    filename_ = filename;
    is_open_ = true;
    is_dirty_ = false;
    
    return true;
}

bool NvmFile::write_file(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    
    BinaryWriter writer(file);
    
    // Update header information
    header_.num_chunks = 3;  // Metadata, index, models
    header_.creation_time = NvmFile::current_timestamp();
    
    // Write file header
    header_.write(writer);
    
    // Write chunks
    write_metadata_chunk(writer);
    write_index_chunk(writer);
    write_models_chunk(writer);
    
    is_dirty_ = false;
    return true;
}

void NvmFile::update_index() {
    index_.clear();
    index_.reserve(models_.size());
    
    for (const auto& pair : models_) {
        const auto& model = pair.second;
        IndexEntry entry;
        entry.model_name = model.model_name;
        entry.size = static_cast<uint32_t>(model.calculate_size());
        entry.context_hash = hash_string(model.context.current_phoneme);
        index_.push_back(entry);
    }
}

void NvmFile::write_metadata_chunk(BinaryWriter& writer) {
    // Write metadata as JSON
    std::string json = metadata_.to_json();
    writer.write_string(json);
}

void NvmFile::write_index_chunk(BinaryWriter& writer) {
    writer.write_uint32(static_cast<uint32_t>(index_.size()));
    for (const auto& entry : index_) {
        entry.write(writer);
    }
}

void NvmFile::write_models_chunk(BinaryWriter& writer) {
    writer.write_uint32(static_cast<uint32_t>(models_.size()));
    for (const auto& pair : models_) {
        pair.second.write(writer);
    }
}

uint32_t NvmFile::hash_string(const std::string& str) {
    // Simple hash function - could be enhanced
    std::hash<std::string> hasher;
    return static_cast<uint32_t>(hasher(str));
}

uint64_t NvmFile::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
}

// Validation functions
namespace validation {

bool is_valid_nvm_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    
    BinaryReader reader(file);
    FileHeader header;
    header.read(reader);
    
    return header.is_valid();
}

bool check_file_integrity(const std::string& filename) {
    NvmFile file;
    if (!file.open(filename)) return false;
    
    return file.verify_integrity();
}

bool is_version_supported(uint32_t version) {
    return version >= constants::MIN_SUPPORTED_VERSION;
}

std::string version_to_string(uint32_t version) {
    uint8_t major = (version >> 16) & 0xFF;
    uint8_t minor = (version >> 8) & 0xFF;
    uint8_t patch = version & 0xFF;
    
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

} // namespace validation

} // namespace nvm
} // namespace nexussynth