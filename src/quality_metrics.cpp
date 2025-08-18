#include "nexussynth/quality_metrics.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>

namespace NexusSynth {

QualityEvaluator::QualityEvaluator() {
}

QualityEvaluator::~QualityEvaluator() {
}

double QualityEvaluator::calculate_mcd(
    const std::vector<std::vector<double>>& reference_mfcc,
    const std::vector<std::vector<double>>& synthesized_mfcc,
    int ignore_c0
) {
    if (reference_mfcc.empty() || synthesized_mfcc.empty()) {
        return -1.0; // 오류 표시
    }
    
    if (reference_mfcc[0].size() != synthesized_mfcc[0].size()) {
        return -1.0; // 차원 불일치
    }
    
    // 시퀀스 길이 정렬
    auto ref_copy = reference_mfcc;
    auto syn_copy = synthesized_mfcc;
    align_sequences(ref_copy, syn_copy);
    
    double total_distortion = 0.0;
    int valid_frames = 0;
    int mfcc_dim = static_cast<int>(ref_copy[0].size()) - ignore_c0;
    
    for (size_t frame = 0; frame < ref_copy.size() && frame < syn_copy.size(); ++frame) {
        double frame_distortion = 0.0;
        
        // C0 계수를 제외하고 계산 (일반적으로 ignore_c0 = 1)
        for (int dim = ignore_c0; dim < static_cast<int>(ref_copy[frame].size()); ++dim) {
            double diff = ref_copy[frame][dim] - syn_copy[frame][dim];
            frame_distortion += diff * diff;
        }
        
        if (frame_distortion >= 0) {  // 유효한 프레임
            total_distortion += std::sqrt(frame_distortion);
            valid_frames++;
        }
    }
    
    if (valid_frames == 0) {
        return -1.0;
    }
    
    // MCD는 일반적으로 10/ln(10) 스케일링 팩터를 사용 (약 4.343)
    double mcd = (10.0 / std::log(10.0)) * 2.0 * (total_distortion / valid_frames);
    return mcd;
}

double QualityEvaluator::calculate_f0_rmse(
    const std::vector<double>& reference_f0,
    const std::vector<double>& synthesized_f0,
    double unvoiced_threshold
) {
    if (reference_f0.empty() || synthesized_f0.empty()) {
        return -1.0;
    }
    
    // 길이 정렬
    auto ref_f0 = reference_f0;
    auto syn_f0 = synthesized_f0;
    align_f0_sequences(ref_f0, syn_f0);
    
    double sum_squared_error = 0.0;
    int valid_frames = 0;
    
    for (size_t i = 0; i < std::min(ref_f0.size(), syn_f0.size()); ++i) {
        bool ref_voiced = is_voiced_frame(ref_f0[i], unvoiced_threshold);
        bool syn_voiced = is_voiced_frame(syn_f0[i], unvoiced_threshold);
        
        // 둘 다 유성음인 경우만 계산
        if (ref_voiced && syn_voiced) {
            double error = ref_f0[i] - syn_f0[i];
            sum_squared_error += error * error;
            valid_frames++;
        }
    }
    
    if (valid_frames == 0) {
        return -1.0;
    }
    
    return std::sqrt(sum_squared_error / valid_frames);
}

double QualityEvaluator::calculate_spectral_correlation(
    const std::vector<std::vector<double>>& reference_spectrum,
    const std::vector<std::vector<double>>& synthesized_spectrum
) {
    if (reference_spectrum.empty() || synthesized_spectrum.empty()) {
        return -1.0;
    }
    
    // 프레임별 상관관계 계산 후 평균
    double total_correlation = 0.0;
    int valid_frames = 0;
    
    size_t min_frames = std::min(reference_spectrum.size(), synthesized_spectrum.size());
    
    for (size_t frame = 0; frame < min_frames; ++frame) {
        if (reference_spectrum[frame].size() != synthesized_spectrum[frame].size()) {
            continue;
        }
        
        double correlation = compute_correlation(reference_spectrum[frame], synthesized_spectrum[frame]);
        if (correlation >= -1.0 && correlation <= 1.0) {  // 유효한 상관관계 값
            total_correlation += correlation;
            valid_frames++;
        }
    }
    
    return valid_frames > 0 ? total_correlation / valid_frames : -1.0;
}

QualityMetrics QualityEvaluator::evaluate_synthesis_quality(
    const std::vector<std::vector<double>>& reference_mfcc,
    const std::vector<std::vector<double>>& synthesized_mfcc,
    const std::vector<double>& reference_f0,
    const std::vector<double>& synthesized_f0
) {
    QualityMetrics metrics;
    
    // MCD 계산
    metrics.mcd_score = calculate_mcd(reference_mfcc, synthesized_mfcc, 1);
    
    // F0 RMSE 계산
    metrics.f0_rmse = calculate_f0_rmse(reference_f0, synthesized_f0, 50.0);
    
    // 스펙트럼 상관관계 계산
    metrics.spectral_correlation = calculate_spectral_correlation(reference_mfcc, synthesized_mfcc);
    
    // 프레임 통계
    metrics.total_frames = std::min(reference_mfcc.size(), synthesized_mfcc.size());
    metrics.valid_frames = metrics.total_frames;  // 일단 모든 프레임을 유효한 것으로 가정
    
    return metrics;
}

ValidationReport QualityEvaluator::run_validation_suite(
    const std::string& reference_dir,
    const std::string& synthesized_dir,
    const std::string& phoneme_alignment_file
) {
    ValidationReport report;
    
    // 현재 시간 스탬프 생성
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    report.report_timestamp = oss.str();
    report.model_version = "NexusSynth-HMM-v1.0";
    
    // TODO: 실제 파일 검색 및 평가 로직 구현
    // 현재는 기본 구조만 제공
    
    return report;
}

std::vector<std::vector<double>> QualityEvaluator::extract_mfcc_from_spectrum(
    const std::vector<std::vector<double>>& spectrum,
    int num_coefficients,
    double sample_rate
) {
    std::vector<std::vector<double>> mfcc_features;
    
    for (const auto& frame_spectrum : spectrum) {
        if (frame_spectrum.empty()) continue;
        
        // 로그 스펙트럼 계산
        std::vector<double> log_spectrum;
        log_spectrum.reserve(frame_spectrum.size());
        
        for (double magnitude : frame_spectrum) {
            // 로그 변환 (작은 값 보호)
            double log_val = std::log(std::max(magnitude, 1e-10));
            log_spectrum.push_back(log_val);
        }
        
        // DCT 변환으로 MFCC 계수 계산
        std::vector<double> mfcc_coeffs = compute_dct(log_spectrum, num_coefficients);
        mfcc_features.push_back(mfcc_coeffs);
    }
    
    return mfcc_features;
}

void QualityEvaluator::align_sequences(
    std::vector<std::vector<double>>& seq1,
    std::vector<std::vector<double>>& seq2
) {
    if (seq1.empty() || seq2.empty()) return;
    
    size_t min_length = std::min(seq1.size(), seq2.size());
    
    // 간단한 길이 조정 (더 긴 시퀀스를 잘라냄)
    if (seq1.size() > min_length) {
        seq1.resize(min_length);
    }
    if (seq2.size() > min_length) {
        seq2.resize(min_length);
    }
}

void QualityEvaluator::align_f0_sequences(
    std::vector<double>& f0_1,
    std::vector<double>& f0_2
) {
    if (f0_1.empty() || f0_2.empty()) return;
    
    size_t min_length = std::min(f0_1.size(), f0_2.size());
    
    if (f0_1.size() > min_length) {
        f0_1.resize(min_length);
    }
    if (f0_2.size() > min_length) {
        f0_2.resize(min_length);
    }
}

std::vector<double> QualityEvaluator::compute_dct(const std::vector<double>& log_spectrum, int num_coefficients) {
    std::vector<double> dct_coeffs(num_coefficients, 0.0);
    
    if (log_spectrum.empty()) return dct_coeffs;
    
    int N = static_cast<int>(log_spectrum.size());
    
    for (int k = 0; k < num_coefficients; ++k) {
        for (int n = 0; n < N; ++n) {
            double angle = M_PI * k * (2.0 * n + 1) / (2.0 * N);
            dct_coeffs[k] += log_spectrum[n] * std::cos(angle);
        }
        
        // 정규화
        if (k == 0) {
            dct_coeffs[k] *= std::sqrt(1.0 / N);
        } else {
            dct_coeffs[k] *= std::sqrt(2.0 / N);
        }
    }
    
    return dct_coeffs;
}

bool QualityEvaluator::is_voiced_frame(double f0_value, double threshold) {
    return f0_value > threshold && std::isfinite(f0_value);
}

double QualityEvaluator::compute_mean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / values.size();
}

double QualityEvaluator::compute_std(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;
    
    double mean = compute_mean(values);
    double sum_squared_diff = 0.0;
    
    for (double value : values) {
        double diff = value - mean;
        sum_squared_diff += diff * diff;
    }
    
    return std::sqrt(sum_squared_diff / (values.size() - 1));
}

double QualityEvaluator::compute_correlation(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.empty()) {
        return -2.0;  // 오류 표시 (유효 범위 [-1, 1] 밖의 값)
    }
    
    double mean_x = compute_mean(x);
    double mean_y = compute_mean(y);
    
    double numerator = 0.0;
    double sum_sq_x = 0.0;
    double sum_sq_y = 0.0;
    
    for (size_t i = 0; i < x.size(); ++i) {
        double diff_x = x[i] - mean_x;
        double diff_y = y[i] - mean_y;
        
        numerator += diff_x * diff_y;
        sum_sq_x += diff_x * diff_x;
        sum_sq_y += diff_y * diff_y;
    }
    
    double denominator = std::sqrt(sum_sq_x * sum_sq_y);
    if (denominator < 1e-10) {
        return 0.0;  // 분모가 너무 작으면 0 반환
    }
    
    return numerator / denominator;
}

void ValidationReport::save_to_json(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filepath);
    }
    
    file << "{\n";
    file << "  \"timestamp\": \"" << report_timestamp << "\",\n";
    file << "  \"model_version\": \"" << model_version << "\",\n";
    file << "  \"overall_metrics\": {\n";
    file << "    \"mcd_score\": " << overall_metrics.mcd_score << ",\n";
    file << "    \"f0_rmse\": " << overall_metrics.f0_rmse << ",\n";
    file << "    \"spectral_correlation\": " << overall_metrics.spectral_correlation << ",\n";
    file << "    \"total_frames\": " << overall_metrics.total_frames << ",\n";
    file << "    \"valid_frames\": " << overall_metrics.valid_frames << "\n";
    file << "  }\n";
    file << "}\n";
    
    file.close();
}

bool ValidationReport::load_from_json(const std::string& filepath) {
    // TODO: JSON 파싱 구현 (현재는 기본 구조만 제공)
    return false;
}

// 유틸리티 함수들
namespace QualityUtils {

std::vector<std::vector<double>> extract_features_from_wav(
    const std::string& wav_filepath,
    const std::string& feature_type
) {
    // TODO: 실제 WAV 파일 처리 구현
    // 현재는 빈 벡터 반환
    return std::vector<std::vector<double>>();
}

void export_metrics_to_csv(
    const ValidationReport& report,
    const std::string& output_filepath
) {
    std::ofstream file(output_filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file for writing: " + output_filepath);
    }
    
    // CSV 헤더
    file << "metric,value\n";
    file << "mcd_score," << report.overall_metrics.mcd_score << "\n";
    file << "f0_rmse," << report.overall_metrics.f0_rmse << "\n";
    file << "spectral_correlation," << report.overall_metrics.spectral_correlation << "\n";
    file << "total_frames," << report.overall_metrics.total_frames << "\n";
    file << "valid_frames," << report.overall_metrics.valid_frames << "\n";
    file << "validity_ratio," << report.overall_metrics.frame_validity_ratio() << "\n";
    
    file.close();
}

std::string assess_quality_level(const QualityMetrics& metrics) {
    if (!metrics.is_valid()) {
        return "INVALID";
    }
    
    // 일반적인 음성 합성 품질 임계값
    if (metrics.mcd_score < 4.0 && metrics.f0_rmse < 15.0 && metrics.spectral_correlation > 0.9) {
        return "EXCELLENT";
    } else if (metrics.mcd_score < 6.0 && metrics.f0_rmse < 25.0 && metrics.spectral_correlation > 0.8) {
        return "GOOD";
    } else if (metrics.mcd_score < 8.0 && metrics.f0_rmse < 35.0 && metrics.spectral_correlation > 0.7) {
        return "FAIR";
    } else {
        return "POOR";
    }
}

} // namespace QualityUtils
} // namespace NexusSynth