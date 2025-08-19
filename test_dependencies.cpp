// 의존성 라이브러리 테스트 파일
#include <iostream>

// WORLD vocoder
#include "world/codec.h"
#include "world/harvest.h"

// Eigen3 
#include "Eigen/Dense"

// AsmJit
#include "asmjit/asmjit.h"

// cJSON
#include "cJSON.h"

int main() {
    std::cout << "=== NexusSynth 의존성 테스트 ===" << std::endl;
    
    // Eigen3 테스트
    Eigen::Vector3d vec(1.0, 2.0, 3.0);
    std::cout << "✓ Eigen3: Vector created successfully" << std::endl;
    
    // cJSON 테스트 (헤더 포함만 확인)
    std::cout << "✓ cJSON: Headers included successfully" << std::endl;
    
    // WORLD 테스트 (기본적인 헤더 포함 확인)
    std::cout << "✓ WORLD: Headers included successfully" << std::endl;
    
    // AsmJit 테스트 (기본적인 헤더 포함 확인)
    std::cout << "✓ AsmJit: Headers included successfully" << std::endl;
    
    std::cout << "모든 의존성이 성공적으로 로드되었습니다!" << std::endl;
    return 0;
}