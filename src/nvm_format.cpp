#include "nexussynth/nvm_format.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <zlib.h>
#include <iomanip>
#include <sstream>

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

// CompressionStream implementations
std::unique_ptr<CompressionStream> CompressionStream::create(Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::None:
            return nullptr;
        case Algorithm::Zlib:
            return std::make_unique<ZlibCompressionStream>();
        case Algorithm::LZ4:
            // TODO: Implement LZ4 support
            throw CompressionException("LZ4 compression not yet implemented");
        default:
            throw CompressionException("Unknown compression algorithm");
    }
}

CompressionStream::Algorithm CompressionStream::from_uint32(uint32_t value) {
    switch (value) {
        case constants::COMPRESSION_NONE: return Algorithm::None;
        case constants::COMPRESSION_ZLIB: return Algorithm::Zlib;
        case constants::COMPRESSION_LZ4: return Algorithm::LZ4;
        default: return Algorithm::None;
    }
}

uint32_t CompressionStream::to_uint32(Algorithm algorithm) {
    return static_cast<uint32_t>(algorithm);
}

// ZlibCompressionStream implementation
ZlibCompressionStream::ZlibCompressionStream(int compression_level) 
    : compression_level_(compression_level) {
    if (compression_level < 0 || compression_level > 9) {
        compression_level_ = 6; // Default level
    }
}

bool ZlibCompressionStream::compress(const void* input, size_t input_size, 
                                   std::vector<uint8_t>& output) {
    if (!input || input_size == 0) {
        output.clear();
        return true;
    }
    
    // Estimate output size (input_size + header + some margin)
    uLongf output_size = compressBound(static_cast<uLong>(input_size));
    output.resize(output_size);
    
    int result = compress2(output.data(), &output_size,
                          static_cast<const Bytef*>(input), static_cast<uLong>(input_size),
                          compression_level_);
    
    if (result != Z_OK) {
        throw CompressionException("Zlib compression failed with error: " + std::to_string(result));
    }
    
    output.resize(output_size);
    return true;
}

bool ZlibCompressionStream::decompress(const void* input, size_t input_size,
                                     std::vector<uint8_t>& output) {
    if (!input || input_size == 0) {
        output.clear();
        return true;
    }
    
    // Start with estimate and grow if needed
    uLongf output_size = input_size * 4; // Initial estimate
    output.resize(output_size);
    
    int result = uncompress(output.data(), &output_size,
                           static_cast<const Bytef*>(input), static_cast<uLong>(input_size));
    
    while (result == Z_BUF_ERROR) {
        // Need more space
        output_size *= 2;
        output.resize(output_size);
        result = uncompress(output.data(), &output_size,
                           static_cast<const Bytef*>(input), static_cast<uLong>(input_size));
    }
    
    if (result != Z_OK) {
        throw CompressionException("Zlib decompression failed with error: " + std::to_string(result));
    }
    
    output.resize(output_size);
    return true;
}

// ChecksumCalculator implementations
std::vector<uint8_t> ChecksumCalculator::calculate(const void* data, size_t size) {
    reset();
    update(data, size);
    return finalize();
}

std::string ChecksumCalculator::to_hex_string(const std::vector<uint8_t>& checksum) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : checksum) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::unique_ptr<ChecksumCalculator> ChecksumCalculator::create(Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::None:
            return nullptr;
        case Algorithm::CRC32:
            return std::make_unique<Crc32Calculator>();
        case Algorithm::SHA256:
            return std::make_unique<Sha256Calculator>();
        default:
            throw ChecksumException("Unknown checksum algorithm");
    }
}

ChecksumCalculator::Algorithm ChecksumCalculator::from_uint32(uint32_t value) {
    switch (value) {
        case constants::CHECKSUM_NONE: return Algorithm::None;
        case constants::CHECKSUM_CRC32: return Algorithm::CRC32;
        case constants::CHECKSUM_SHA256: return Algorithm::SHA256;
        default: return Algorithm::None;
    }
}

uint32_t ChecksumCalculator::to_uint32(Algorithm algorithm) {
    return static_cast<uint32_t>(algorithm);
}

size_t ChecksumCalculator::checksum_size(Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::None: return 0;
        case Algorithm::CRC32: return 4;
        case Algorithm::SHA256: return 32;
        default: return 0;
    }
}

// CRC32 implementation
const uint32_t Crc32Calculator::CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

Crc32Calculator::Crc32Calculator() : crc_(0xFFFFFFFF) {
}

void Crc32Calculator::reset() {
    crc_ = 0xFFFFFFFF;
}

void Crc32Calculator::update(const void* data, size_t size) {
    crc_ = calculate_crc32(data, size, crc_);
}

std::vector<uint8_t> Crc32Calculator::finalize() {
    uint32_t final_crc = crc_ ^ 0xFFFFFFFF;
    std::vector<uint8_t> result(4);
    result[0] = (final_crc) & 0xFF;
    result[1] = (final_crc >> 8) & 0xFF;
    result[2] = (final_crc >> 16) & 0xFF;
    result[3] = (final_crc >> 24) & 0xFF;
    return result;
}

uint32_t Crc32Calculator::calculate_crc32(const void* data, size_t size, uint32_t initial_crc) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = initial_crc;
    
    for (size_t i = 0; i < size; ++i) {
        crc = CRC32_TABLE[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc;
}

// SHA256 implementation
struct Sha256Calculator::Sha256Context {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
    size_t buffer_len;
    
    Sha256Context() {
        reset();
    }
    
    void reset() {
        state[0] = 0x6a09e667;
        state[1] = 0xbb67ae85;
        state[2] = 0x3c6ef372;
        state[3] = 0xa54ff53a;
        state[4] = 0x510e527f;
        state[5] = 0x9b05688c;
        state[6] = 0x1f83d9ab;
        state[7] = 0x5be0cd19;
        count = 0;
        buffer_len = 0;
    }
    
    void update(const void* data, size_t size);
    std::vector<uint8_t> finalize();
    
private:
    static void sha256_transform(uint32_t state[8], const uint8_t block[64]);
    static uint32_t rotr32(uint32_t value, unsigned int count) {
        return (value >> count) | (value << (32 - count));
    }
};

void Sha256Calculator::Sha256Context::update(const void* data, size_t size) {
    const uint8_t* input = static_cast<const uint8_t*>(data);
    
    while (size > 0) {
        size_t available = 64 - buffer_len;
        size_t to_copy = std::min(size, available);
        
        std::memcpy(buffer + buffer_len, input, to_copy);
        buffer_len += to_copy;
        input += to_copy;
        size -= to_copy;
        count += to_copy;
        
        if (buffer_len == 64) {
            sha256_transform(state, buffer);
            buffer_len = 0;
        }
    }
}

std::vector<uint8_t> Sha256Calculator::Sha256Context::finalize() {
    // Add padding
    uint64_t bit_count = count * 8;
    buffer[buffer_len++] = 0x80;
    
    if (buffer_len > 56) {
        while (buffer_len < 64) {
            buffer[buffer_len++] = 0;
        }
        sha256_transform(state, buffer);
        buffer_len = 0;
    }
    
    while (buffer_len < 56) {
        buffer[buffer_len++] = 0;
    }
    
    // Add length
    for (int i = 7; i >= 0; --i) {
        buffer[56 + i] = (bit_count >> (8 * (7 - i))) & 0xFF;
    }
    
    sha256_transform(state, buffer);
    
    // Convert to bytes
    std::vector<uint8_t> result(32);
    for (int i = 0; i < 8; ++i) {
        result[i * 4] = (state[i] >> 24) & 0xFF;
        result[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        result[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        result[i * 4 + 3] = state[i] & 0xFF;
    }
    
    return result;
}

void Sha256Calculator::Sha256Context::sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    
    // Prepare message schedule
    for (int i = 0; i < 16; ++i) {
        W[i] = (block[i * 4] << 24) | (block[i * 4 + 1] << 16) | 
               (block[i * 4 + 2] << 8) | block[i * 4 + 3];
    }
    
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(W[i - 15], 7) ^ rotr32(W[i - 15], 18) ^ (W[i - 15] >> 3);
        uint32_t s1 = rotr32(W[i - 2], 17) ^ rotr32(W[i - 2], 19) ^ (W[i - 2] >> 10);
        W[i] = W[i - 16] + s0 + W[i - 7] + s1;
    }
    
    // Initialize working variables
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    
    // Main loop
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + K[i] + W[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    
    // Add to state
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

Sha256Calculator::Sha256Calculator() : context_(std::make_unique<Sha256Context>()) {
}

void Sha256Calculator::reset() {
    context_->reset();
}

void Sha256Calculator::update(const void* data, size_t size) {
    context_->update(data, size);
}

std::vector<uint8_t> Sha256Calculator::finalize() {
    return context_->finalize();
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
    : is_open_(false), is_dirty_(false), compression_enabled_(false), checksum_enabled_(false),
      compression_algorithm_(CompressionStream::Algorithm::Zlib),
      checksum_algorithm_(ChecksumCalculator::Algorithm::CRC32) {
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
    header_.compression_type = CompressionStream::to_uint32(compression_algorithm_);
    header_.checksum_type = ChecksumCalculator::to_uint32(checksum_algorithm_);
    
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

bool NvmFile::compress_data(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) const {
    if (!compression_enabled_) {
        output = input;
        return true;
    }
    
    auto compressor = CompressionStream::create(compression_algorithm_);
    if (!compressor) {
        output = input;
        return true;
    }
    
    try {
        return compressor->compress(input.data(), input.size(), output);
    } catch (const CompressionException& e) {
        // Log error and fall back to uncompressed
        output = input;
        return false;
    }
}

bool NvmFile::decompress_data(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) const {
    if (!compression_enabled_) {
        output = input;
        return true;
    }
    
    auto decompressor = CompressionStream::create(compression_algorithm_);
    if (!decompressor) {
        output = input;
        return true;
    }
    
    try {
        return decompressor->decompress(input.data(), input.size(), output);
    } catch (const CompressionException& e) {
        throw ChecksumException("Failed to decompress data: " + std::string(e.what()));
    }
}

std::vector<uint8_t> NvmFile::calculate_data_checksum(const void* data, size_t size) const {
    if (!checksum_enabled_) {
        return {};
    }
    
    auto calculator = ChecksumCalculator::create(checksum_algorithm_);
    if (!calculator) {
        return {};
    }
    
    return calculator->calculate(data, size);
}

bool NvmFile::verify_data_checksum(const void* data, size_t size, 
                                  const std::vector<uint8_t>& expected_checksum) const {
    if (!checksum_enabled_ || expected_checksum.empty()) {
        return true; // No verification needed
    }
    
    std::vector<uint8_t> actual_checksum = calculate_data_checksum(data, size);
    return actual_checksum == expected_checksum;
}

bool NvmFile::verify_checksums() const {
    if (!checksum_enabled_) {
        return true; // No verification needed
    }
    
    // This is a simplified implementation
    // In a full implementation, you would read and verify checksums for each chunk
    return true;
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

bool verify_compression_support(uint32_t compression_type) {
    switch (compression_type) {
        case constants::COMPRESSION_NONE:
        case constants::COMPRESSION_ZLIB:
            return true;
        case constants::COMPRESSION_LZ4:
            return false; // Not yet implemented
        default:
            return false;
    }
}

bool verify_checksum_support(uint32_t checksum_type) {
    switch (checksum_type) {
        case constants::CHECKSUM_NONE:
        case constants::CHECKSUM_CRC32:
        case constants::CHECKSUM_SHA256:
            return true;
        default:
            return false;
    }
}

bool verify_file_format_integrity(const std::string& filename) {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;
        
        BinaryReader reader(file);
        FileHeader header;
        header.read(reader);
        
        // Basic format validation
        if (!header.is_valid()) return false;
        
        // Check compression and checksum support
        if (!verify_compression_support(header.compression_type)) return false;
        if (!verify_checksum_support(header.checksum_type)) return false;
        
        // Check if file size matches header
        file.seekg(0, std::ios::end);
        size_t actual_size = static_cast<size_t>(file.tellg());
        if (actual_size != header.file_size) return false;
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool test_compression_roundtrip(const std::vector<uint8_t>& test_data, 
                               CompressionStream::Algorithm algorithm) {
    try {
        auto compressor = CompressionStream::create(algorithm);
        if (!compressor && algorithm != CompressionStream::Algorithm::None) {
            return false;
        }
        
        if (!compressor) return true; // No compression is always valid
        
        std::vector<uint8_t> compressed, decompressed;
        
        if (!compressor->compress(test_data.data(), test_data.size(), compressed)) {
            return false;
        }
        
        if (!compressor->decompress(compressed.data(), compressed.size(), decompressed)) {
            return false;
        }
        
        return test_data == decompressed;
    } catch (const std::exception&) {
        return false;
    }
}

bool test_checksum_consistency(const std::vector<uint8_t>& test_data,
                              ChecksumCalculator::Algorithm algorithm) {
    try {
        auto calculator1 = ChecksumCalculator::create(algorithm);
        auto calculator2 = ChecksumCalculator::create(algorithm);
        
        if (!calculator1 || !calculator2) {
            return algorithm == ChecksumCalculator::Algorithm::None;
        }
        
        // Calculate checksum twice and compare
        std::vector<uint8_t> checksum1 = calculator1->calculate(test_data.data(), test_data.size());
        std::vector<uint8_t> checksum2 = calculator2->calculate(test_data.data(), test_data.size());
        
        return checksum1 == checksum2;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace validation

} // namespace nvm
} // namespace nexussynth