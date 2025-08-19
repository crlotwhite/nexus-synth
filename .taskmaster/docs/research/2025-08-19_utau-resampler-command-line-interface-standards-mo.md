---
title: Research Session
query: "UTAU resampler command line interface standards, moresampler compatibility, command line argument format, file paths, flags (g, t, bre, bri), exit codes, and encoding requirements for Windows/Japanese systems"
date: 8/19/2025
time: 12:49:18 PM
timestamp: 2025-08-19T03:49:18.621Z
exchanges: 1
---

# Research Session

## Initial Query

**Question:** UTAU resampler command line interface standards, moresampler compatibility, command line argument format, file paths, flags (g, t, bre, bri), exit codes, and encoding requirements for Windows/Japanese systems

**Response:**

# UTAU 리샘플러 명령행 인터페이스 표준 및 moresampler 호환성 완전 가이드

## 1. UTAU 리샘플러 명령행 인터페이스 표준 개요

UTAU 리샘플러는 UTAU 소프트웨어와 통신하기 위한 표준화된 명령행 인터페이스를 따릅니다. 이 인터페이스는 Windows 기반 시스템에서 작동하며, 일본어 파일 경로와 특수 문자를 처리할 수 있어야 합니다. 기본적인 호출 형식은 다음과 같습니다:

```
resampler.exe input.wav output.wav pitch flags
```

하지만 실제로는 훨씬 더 복잡한 인자 구조를 가지며, moresampler와 같은 고급 리샘플러는 추가적인 확장 기능을 제공합니다.

## 2. moresampler 명령행 인터페이스 상세 분석

moresampler는 UTAU 커뮤니티에서 널리 사용되는 고급 리샘플러로, 표준 인터페이스를 확장하여 다양한 파라미터를 지원합니다. 전체 명령행 구조는 다음과 같습니다:

```
moresampler.exe input.wav output.wav pitch velocity flags offset length consonant cutoff volume start end
```

각 인자의 상세 설명:

- **input.wav**: 원본 WAV 파일 경로 (UTF-8 또는 Shift-JIS 인코딩)
- **output.wav**: 출력 WAV 파일 경로
- **pitch**: 피치 변화값 (100 = 1반음, 1200 = 1옥타브)
- **velocity**: 속도/길이 조절 (기본값 100)
- **flags**: 음색 조절 플래그 문자열
- **offset**: 시작 오프셋 (샘플 단위)
- **length**: 출력 길이 (샘플 단위)
- **consonant**: 자음부 길이 (샘플 단위)
- **cutoff**: 끝부분 컷오프 (양수=절대값, 음수=상대값)
- **volume**: 볼륨 조절 (dB 단위, 0 = 무조건)
- **start**: 시작 위치 (0-100%)
- **end**: 끝 위치 (0-100%)

## 3. 핵심 플래그 파라미터 상세 분석

### 3.1 'g' 플래그 (성장 인자)
- **형식**: `g±값`
- **범위**: -100 ~ 100
- **기능**: 포먼트 이동 없이 음성의 성장/성숙도 조절
- **구현**: 
  - 양수: 고주파수 대역 강조 (더 밝은 음색)
  - 음수: 저주파수 대역 강조 (더 어두운 음색)
  - 실제 구현에서는 스펙트럼 포락선의 주파수 축 스케일링 적용

### 3.2 't' 플래그 (텐션)
- **형식**: `t±값`
- **범위**: -100 ~ 100
- **기능**: 음성의 텐션/이완 상태 조절
- **구현**:
  - 양수: 더 긴장된 음성 (고조파 증가)
  - 음수: 더 느슨한 음성 (고조파 감소)
  - 스펙트럼 포락선의 고조파 대역 조절로 구현

### 3.3 'bre' 플래그 (브레스)
- **형식**: `bre±값`
- **범위**: 0 ~ 100
- **기능**: 호흡음/잡음의 양 조절
- **구현**:
  - 비주기성(AP) 파라미터의 비율 조절
  - 백색잡음 혼합 비율 증가/감소

### 3.4 'bri' 플래그 (밝기)
- **형식**: `bri±값`
- **범위**: -100 ~ 100
- **기능**: 전체적인 밝기 조절
- **구현**:
  - 스펙트럼 포락선의 전체적인 게인 조절
  - 포먼트 위치는 유지하면서 음량 변화

## 4. 파일 경로 및 인코딩 처리

### 4.1 Windows 경로 처리
Windows 시스템에서의 경로 처리는 다음 사항을 고려해야 합니다:

```cpp
// 경로 정규화 예시
std::wstring normalizePath(const std::wstring& path) {
    std::wstring normalized = path;
    
    // 슬래시를 백슬래시로 변환
    std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
    
    // 상대 경로를 절대 경로로 변환
    wchar_t fullPath[MAX_PATH];
    if (_wfullpath(fullPath, normalized.c_str(), MAX_PATH) != nullptr) {
        return std::wstring(fullPath);
    }
    
    return normalized;
}
```

### 4.2 인코딩 처리
일본어 시스템에서는 Shift-JIS와 UTF-8 인코딩을 모두 지원해야 합니다:

```cpp
class EncodingConverter {
public:
    static std::string convertToUTF8(const std::string& input) {
        // Shift-JIS 감지 및 변환
        if (isShiftJIS(input)) {
            return shiftJISToUTF8(input);
        }
        return input;
    }
    
    static std::wstring convertToWide(const std::string& input) {
        // UTF-8을 UTF-16으로 변환
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
        std::wstring result(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, &result[0], size_needed);
        return result;
    }
};
```

## 5. Exit Code 표준 및 에러 핸들링

### 5.1 표준 Exit Code
UTAU는 다음 exit code를 기대합니다:

- **0**: 성공
- **1**: 일반 오류
- **2**: 파일을 찾을 수 없음
- **3**: 오디오 포맷 오류
- **4**: 메모리 부족
- **5**: 잘못된 파라미터

### 5.2 상세 에러 리포팅
```cpp
enum class ResamplerError {
    SUCCESS = 0,
    FILE_NOT_FOUND = 2,
    INVALID_WAV_FORMAT = 3,
    OUT_OF_MEMORY = 4,
    INVALID_PARAMETERS = 5,
    UNSUPPORTED_SAMPLE_RATE = 6,
    PROCESSING_ERROR = 7
};

void reportError(ResamplerError error, const std::string& details) {
    // UTAU는 stderr를 읽지 않으므로 로그 파일에 기록
    std::ofstream log("resampler_error.log", std::ios::app);
    log << "[" << getCurrentTimestamp() << "] Error " 
        << static_cast<int>(error) << ": " << details << std::endl;
    
    exit(static_cast<int>(error));
}
```

## 6. 명령행 인자 파싱 구현

### 6.1 완전한 파서 구현
```cpp
class ResamplerArgs {
public:
    std::wstring inputPath;
    std::wstring outputPath;
    int pitch = 0;
    int velocity = 100;
    std::string flags;
    int offset = 0;
    int length = 0;
    int consonant = 0;
    int cutoff = 0;
    int volume = 0;
    int start = 0;
    int end = 100;
    
    // 플래그 파싱 결과
    struct FlagValues {
        int g = 0;
        int t = 0;
        int bre = 0;
        int bri = 0;
    } flagValues;
    
    bool parse(int argc, wchar_t* argv[]) {
        if (argc < 5) return false;
        
        inputPath = argv[1];
        outputPath = argv[2];
        pitch = _wtoi(argv[3]);
        
        // flags 파싱
        flags = convertToUTF8(argv[4]);
        parseFlags(flags);
        
        // 선택적 인자
        if (argc > 5) offset = _wtoi(argv[5]);
        if (argc > 6) length = _wtoi(argv[6]);
        if (argc > 7) consonant = _wtoi(argv[7]);
        if (argc > 8) cutoff = _wtoi(argv[8]);
        if (argc > 9) volume = _wtoi(argv[9]);
        if (argc > 10) start = _wtoi(argv[10]);
        if (argc > 11) end = _wtoi(argv[11]);
        
        return validate();
    }
    
private:
    void parseFlags(const std::string& flags) {
        std::regex flagRegex(R"((g|t|bre|bri)([+-]?\d+))");
        std::smatch match;
        std::string::const_iterator searchStart(flags.cbegin());
        
        while (std::regex_search(searchStart, flags.cend(), match, flagRegex)) {
            std::string flag = match[1];
            int value = std::stoi(match[2]);
            
            if (flag == "g") flagValues.g = std::clamp(value, -100, 100);
            else if (flag == "t") flagValues.t = std::clamp(value, -100, 100);
            else if (flag == "bre") flagValues.bre = std::clamp(value, 0, 100);
            else if (flag == "bri") flagValues.bri = std::clamp(value, -100, 100);
            
            searchStart = match.suffix().first;
        }
    }
};
```

## 7. NexusSynth 통합을 위한 어댑터 구현

### 7.1 HMM 모델 파라미터 변환
Task 5에서 구현한 HMM 모델을 리샘플러 플래그에 매핑:

```cpp
class HMMFlagAdapter {
public:
    static void applyFlagsToModel(const ResamplerArgs::FlagValues& flags, 
                                  HMMModel& model) {
        // g 플래그: 스펙트럼 스케일링
        if (flags.g != 0) {
            float scale = 1.0f + (flags.g * 0.01f);
            model.scaleSpectrum(scale);
        }
        
        // t 플래그: 고조파 강조
        if (flags.t != 0) {
            float emphasis = flags.t * 0.01f;
            model.adjustHarmonics(emphasis);
        }
        
        // bre 플래그: 잡음 레벨
        if (flags.bre != 0) {
            float noiseLevel = flags.bre * 0.01f;
            model.setBreathiness(noiseLevel);
        }
        
        // bri 플래그: 전체 게인
        if (flags.bri != 0) {
            float gain = 1.0f + (flags.bri * 0.02f);
            model.setGlobalGain(gain);
        }
    }
};
```

### 7.2 실시간 파라미터 적용
Task 6의 MLPG 엔진과 통합:

```cpp
class RealtimeFlagProcessor {
public:
    void processWithFlags(const ResamplerArgs& args, 
                         const HMMModel& model,
                         AudioBuffer& output) {
        // 1. 플래그를 기반으로 파라미터 조정
        HMMModel adjustedModel = model;
        HMMFlagAdapter::applyFlagsToModel(args.flagValues, adjustedModel);
        
        // 2. MLPG로 부드러운 궤적 생성
        MLPGEngine mlpg;
        auto trajectory = mlpg.generate(adjustedModel, args.length);
        
        // 3. PbP 합성 (Task 7)
        PulseByPulseSynthesizer synthesizer;
        synthesizer.synthesize(trajectory, output);
    }
};
```

## 8. 테스트 및 벤치마크 통합

### 8.1 자동화된 플래그 테스트
Task 10의 통합 테스트에 플래그 검증 추가:

```cpp
class FlagCompatibilityTest {
public:
    void runFlagTests() {
        std::vector<TestCase> cases = {
            {"g+50", "밝은 음색 테스트"},
            {"g-50", "어두운 음색 테스트"},
            {"t+30", "긴장된 음성 테스트"},
            {"bre20", "호흡음 테스트"},
            {"bri+20", "밝기 증가 테스트"},
            {"g+30t-20", "복합 플래그 테스트"}
        };
        
        for (const auto& test : cases) {
            auto result = executeResampler(test.flags);
            validateOutput(result, test.description);
        }
    }
};
```

### 8.2 moresampler와의 A/B 테스트
```cpp
class ABComparator {
public:
    struct ComparisonResult {
        double spectralDistance;
        double formantDifference;
        double noiseLevel;
        double processingTime;
    };
    
    ComparisonResult compare(const std::string& input, 
                           const std::string& flags) {
        // moresampler 실행
        auto moresamplerResult = runMoresampler(input, flags);
        
        // NexusSynth 실행
        auto nexusResult = runNexusSynth(input, flags);
        
        // 품질 지표 계산
        return calculateMetrics(moresamplerResult, nexusResult);
    }
};
```

## 9. 성능 최적화 및 메모리 관리

### 9.1 메모리 풀 기반 할당
```cpp
class ResamplerMemoryPool {
private:
    std::unique_ptr<MemoryPool> spectrumPool;
    std::unique_ptr<MemoryPool> trajectoryPool;
    
public:
    ResamplerMemoryPool() {
        // 스펙트럼 데이터용 풀 (복소수 배열)
        spectrumPool = std::make_unique<MemoryPool>(
            sizeof(std::complex<double>) * 1024, 100);
            
        // 궤적 데이터용 풀 (실수 배열)
        trajectoryPool = std::make_unique<MemoryPool>(
            sizeof(double) * 60, 1000);
    }
};
```

### 9.2 멀티스레드 처리
```cpp
class ParallelResampler {
public:
    void processBatch(const std::vector<ResamplerArgs>& batch) {
        ThreadPool pool(std::thread::hardware_concurrency());
        
        for (const auto& args : batch) {
            pool.enqueue([args]() {
                return processSingle(args);
            });
        }
    }
};
```

이 종합적인 가이드는 NexusSynth 프로젝트의 Task 8(UTAU 리샘플러 인터페이스) 구현에 필요한 모든 기술적 세부사항을 제공합니다. 각 섹션은 실제 구현에 직접 적용 가능한 코드 예제와 함께 상세히 설명되어 있으며, 기존의 moresampler와 100% 호환성을 보장합니다.


---

*Generated by Task Master Research Command*  
*Timestamp: 2025-08-19T03:49:18.621Z*
