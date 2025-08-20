# NexusSynth 빌드 가이드

이 가이드는 NexusSynth를 소스 코드에서 컴파일하는 방법을 설명합니다. 모든 주요 플랫폼(Windows, macOS, Linux)에서의 빌드 과정을 단계별로 다룹니다.

## 목차

1. [빌드 요구사항](#빌드-요구사항)
2. [환경 설정](#환경-설정)
3. [의존성 설치](#의존성-설치)  
4. [빌드 과정](#빌드-과정)
5. [고급 빌드 옵션](#고급-빌드-옵션)
6. [문제 해결](#문제-해결)
7. [개발 환경 설정](#개발-환경-설정)

## 빌드 요구사항

### 공통 요구사항

#### 필수 도구
- **CMake**: 3.16 이상 (3.20 이상 권장)
- **Git**: 2.20 이상 (소스 코드 다운로드용)
- **C++17 호환 컴파일러**

#### 시스템 요구사항
- **메모리**: 4GB 이상 (8GB 권장)
- **디스크 공간**: 10GB 이상 (빌드 및 의존성 포함)
- **네트워크**: 인터넷 연결 (의존성 자동 다운로드)

### 플랫폼별 요구사항

#### Windows
- **Visual Studio**: 2019 16.8 이상 또는 2022 (권장)
- **Windows SDK**: 10.0.18362 이상
- **PowerShell**: 5.1 이상

#### macOS
- **Xcode**: 12.0 이상 (Command Line Tools 포함)
- **macOS**: 10.15 Catalina 이상
- **Homebrew**: 패키지 관리용 (권장)

#### Linux
- **GCC**: 9.0 이상 또는 **Clang**: 10 이상
- **Ubuntu**: 18.04 이상 / **CentOS**: 8 이상
- **기본 개발 도구**: build-essential, git

## 환경 설정

### 1. 소스 코드 다운로드

```bash
# GitHub에서 복제
git clone https://github.com/nexussynth/nexus-synth.git
cd nexus-synth

# 또는 특정 릴리스 버전
git clone --branch v1.0.0 https://github.com/nexussynth/nexus-synth.git
cd nexus-synth
```

### 2. 빌드 디렉터리 생성

```bash
# 빌드 디렉터리 생성 (소스와 분리)
mkdir build
cd build
```

## 의존성 설치

NexusSynth는 CPM(CMake Package Manager)을 사용해 의존성을 자동으로 관리합니다. 첫 빌드 시 다음 라이브러리들이 자동으로 다운로드됩니다:

### 주요 의존성
- **WORLD Vocoder**: 음성 분석 및 합성
- **Eigen3**: 선형대수 연산
- **AsmJit**: 실시간 JIT 컴파일
- **cJSON**: JSON 파싱
- **Google Test**: 단위 테스트 (옵션)

### 수동 의존성 설치 (선택사항)

시스템에 이미 설치된 라이브러리를 사용하려면:

#### Windows (vcpkg)
```batch
# vcpkg 설치
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# 의존성 설치
.\vcpkg install eigen3:x64-windows
.\vcpkg install gtest:x64-windows
```

#### macOS (Homebrew)
```bash
# Homebrew 설치 (없는 경우)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 의존성 설치
brew install eigen
brew install cmake
brew install pkg-config
```

#### Ubuntu/Debian
```bash
# 기본 빌드 도구
sudo apt update
sudo apt install build-essential cmake git pkg-config

# 의존성 라이브러리 (선택사항)
sudo apt install libeigen3-dev
sudo apt install libgtest-dev
sudo apt install libfftw3-dev
```

#### CentOS/RHEL
```bash
# EPEL 저장소 활성화
sudo yum install epel-release

# 기본 빌드 도구
sudo yum groupinstall "Development Tools"
sudo yum install cmake3 git

# 의존성 라이브러리
sudo yum install eigen3-devel
sudo yum install gtest-devel
```

## 빌드 과정

### 1. Windows 빌드

#### Visual Studio 2022 사용
```batch
# 프로젝트 생성
cmake .. -G "Visual Studio 17 2022" -A x64

# 빌드 실행
cmake --build . --config Release --parallel

# 또는 Visual Studio에서 열기
start NexusSynth.sln
```

#### 명령줄 빌드 (권장)
```batch
# 환경 설정
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

# CMake 구성
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# 빌드
ninja

# 테스트 (선택사항)
ctest --verbose
```

#### PowerShell 스크립트 사용
```powershell
# 제공된 빌드 스크립트 실행
.\build-windows.bat

# 또는 PowerShell에서
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### 2. macOS 빌드

#### Xcode 프로젝트 생성
```bash
# Xcode 프로젝트 생성
cmake .. -G Xcode

# Xcode에서 빌드
open NexusSynth.xcodeproj
```

#### 명령줄 빌드 (권장)
```bash
# CMake 구성 (Release 빌드)
cmake .. -DCMAKE_BUILD_TYPE=Release

# 병렬 빌드
make -j$(sysctl -n hw.ncpu)

# 또는 Ninja 사용 (더 빠름)
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja

# 테스트
ctest --verbose
```

#### Apple Silicon (M1/M2) 최적화
```bash
# Apple Silicon 최적화 빌드
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_CXX_FLAGS="-mcpu=apple-m1"

make -j$(sysctl -n hw.ncpu)
```

#### Universal Binary 생성 (Intel + Apple Silicon)
```bash
# Universal Binary 빌드
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

make -j$(sysctl -n hw.ncpu)
```

### 3. Linux 빌드

#### 기본 빌드
```bash
# CMake 구성
cmake .. -DCMAKE_BUILD_TYPE=Release

# 병렬 빌드
make -j$(nproc)

# 테스트
ctest --verbose

# 설치 (선택사항)
sudo make install
```

#### Ninja 빌드 시스템 사용 (권장)
```bash
# Ninja 설치 (Ubuntu)
sudo apt install ninja-build

# CMake 구성 및 빌드
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja

# 테스트
ctest --verbose
```

#### 최적화된 빌드
```bash
# 최대 성능 최적화
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -flto" \
    -DCMAKE_C_FLAGS="-O3 -march=native -mtune=native -flto"

make -j$(nproc)
```

## 고급 빌드 옵션

### CMake 빌드 옵션

```bash
# 모든 옵션 확인
cmake .. -LH

# 주요 옵션들
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DNEXUSSYNTH_BUILD_TESTS=ON \
    -DNEXUSSYNTH_BUILD_EXAMPLES=ON \
    -DNEXUSSYNTH_USE_SYSTEM_DEPS=OFF \
    -DNEXUSSYNTH_ENABLE_GPU=ON \
    -DNEXUSSYNTH_ENABLE_OPENMP=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local
```

### 빌드 타입별 특징

#### Debug 빌드
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# 특징:
# - 디버그 심볼 포함
# - 최적화 비활성화  
# - 런타임 검사 활성화
# - 실행 속도 느림
```

#### Release 빌드
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 특징:
# - 최대 성능 최적화
# - 디버그 정보 제거
# - 작은 바이너리 크기
# - 빠른 실행 속도
```

#### RelWithDebInfo 빌드
```bash
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
make -j$(nproc)

# 특징:
# - 최적화 + 디버그 정보
# - 성능 프로파일링 가능
# - 릴리스 수준 성능
# - 디버깅 가능
```

### 컴파일러별 최적화

#### GCC 최적화
```bash
export CXXFLAGS="-O3 -march=native -mtune=native -flto -ffast-math"
export CFLAGS="-O3 -march=native -mtune=native -flto -ffast-math"

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

#### Clang 최적화
```bash
export CXX=clang++
export CC=clang
export CXXFLAGS="-O3 -march=native -flto -ffast-math"

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

#### Intel 컴파일러 최적화 (전문가용)
```bash
source /opt/intel/oneapi/setvars.sh

export CXX=icpx
export CC=icx
export CXXFLAGS="-O3 -xHost -ipo -fast"

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 빌드 결과물

### 생성되는 파일들

빌드 완료 후 다음 파일들이 생성됩니다:

```
build/
├── src/
│   ├── nexussynth              # 메인 실행파일
│   └── libnexussynth.a         # 정적 라이브러리
├── tests/
│   ├── test_*                  # 단위 테스트들  
│   └── integration/
│       ├── ab_comparison_tool  # A/B 비교 도구
│       └── performance_benchmark_tool  # 성능 벤치마크
└── scripts/
    ├── convert_voicebank.*     # 변환 스크립트
    └── nexussynth_batch.json   # 배치 설정 템플릿
```

### 설치 (선택사항)

```bash
# 시스템 전역 설치
sudo make install

# 사용자 지정 경로 설치
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/nexussynth
make install

# 설치된 파일들:
# /usr/local/bin/nexussynth
# /usr/local/lib/libnexussynth.a
# /usr/local/include/nexussynth/
# /usr/local/share/nexussynth/
```

## 문제 해결

### 자주 발생하는 빌드 오류

#### 1. CMake 버전 오류
```
CMake Error: The following variables are used in this project, but they are set to NOTFOUND
```

**해결방법:**
```bash
# CMake 최신 버전 설치
# Ubuntu/Debian
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo apt-key add -
echo 'deb https://apt.kitware.com/ubuntu/ focal main' | sudo tee /etc/apt/sources.list.d/kitware.list
sudo apt update
sudo apt install cmake

# macOS
brew install cmake

# Windows - 공식 사이트에서 다운로드
```

#### 2. 컴파일러 호환성 오류
```
error: 'std::filesystem' is not supported on this platform
```

**해결방법:**
```bash
# 컴파일러 업데이트
# Ubuntu
sudo apt install gcc-10 g++-10
export CXX=g++-10
export CC=gcc-10

# CentOS
sudo yum install centos-release-scl
sudo yum install devtoolset-10
scl enable devtoolset-10 bash
```

#### 3. 메모리 부족 오류
```
c++: internal compiler error: Killed (program cc1plus)
```

**해결방법:**
```bash
# 병렬 빌드 수 줄이기
make -j2  # 2개 코어만 사용

# 스왑 메모리 추가 (Linux)
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

#### 4. 의존성 다운로드 실패
```
CPM: unable to download dependencies
```

**해결방법:**
```bash
# 프록시 설정 (회사 환경)
export HTTP_PROXY=http://proxy.company.com:8080
export HTTPS_PROXY=http://proxy.company.com:8080

# 수동 의존성 설치 사용
cmake .. -DNEXUSSYNTH_USE_SYSTEM_DEPS=ON

# 의존성 캐시 초기화
rm -rf _deps/
```

#### 5. Windows 특화 오류

**Visual Studio 찾기 실패:**
```batch
REM Visual Studio 경로 설정
set CMAKE_GENERATOR="Visual Studio 17 2022"
set CMAKE_GENERATOR_PLATFORM=x64
```

**링크 오류:**
```batch
REM Windows SDK 버전 명시
cmake .. -DCMAKE_SYSTEM_VERSION=10.0.19041.0
```

### 빌드 로그 분석

```bash
# 상세한 빌드 로그 저장
make VERBOSE=1 2>&1 | tee build.log

# 또는 Ninja 사용 시
ninja -v 2>&1 | tee build.log

# 오류만 필터링
make 2>&1 | grep -i error | tee errors.log
```

## 개발 환경 설정

### IDE 설정

#### Visual Studio Code
```json
// .vscode/settings.json
{
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "cmake.configureArgs": [
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DNEXUSSYNTH_BUILD_TESTS=ON"
  ],
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}
```

#### CLion
```cmake
# CMakePresets.json
{
  "version": 3,
  "presets": [
    {
      "name": "debug",
      "displayName": "Debug",
      "generator": "Ninja",
      "binaryDir": "build/debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "NEXUSSYNTH_BUILD_TESTS": "ON"
      }
    }
  ]
}
```

### 코딩 스타일 설정

```bash
# clang-format 설치 및 설정
sudo apt install clang-format

# 코드 포맷팅 실행
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### 지속적 통합 (CI) 설정

로컬에서 CI 환경을 테스트하려면:

```bash
# GitHub Actions 로컬 실행
# act 설치: https://github.com/nektos/act
act -P ubuntu-latest=nektos/act-environments-ubuntu:18.04
```

## 성능 벤치마킹

빌드 후 성능을 확인:

```bash
# 성능 벤치마크 실행
cd build
./tests/integration/performance_benchmark_tool suite comprehensive

# 결과 예시:
# WORLD Analysis: 45.2ms (target: <50ms) ✅
# HMM Training: 128.7ms (target: <150ms) ✅  
# MLPG Generation: 32.1ms (target: <40ms) ✅
# PbP Synthesis: 15.8ms (target: <20ms) ✅
```

## 배포 패키징

### Linux 패키지 생성

```bash
# DEB 패키지 (Ubuntu/Debian)
cmake .. -DCPACK_GENERATOR=DEB
make package

# RPM 패키지 (CentOS/RHEL)
cmake .. -DCPACK_GENERATOR=RPM
make package

# AppImage (Universal Linux)
cmake .. -DCPACK_GENERATOR=External
make package
```

### Windows 배포

```batch
REM NSIS 설치 관리자
cmake .. -DCPACK_GENERATOR=NSIS
cmake --build . --config Release --target package

REM ZIP 아카이브
cmake .. -DCPACK_GENERATOR=ZIP
cmake --build . --config Release --target package
```

### macOS 배포

```bash
# DMG 패키지
cmake .. -DCPACK_GENERATOR=DragNDrop
make package

# Bundle 패키지
cmake .. -DCPACK_GENERATOR=Bundle
make package
```

---

**🚀 다음 단계**: 빌드가 성공했다면 [엔진 사용법 가이드](ENGINE_USAGE_KO.md)를 확인하여 NexusSynth를 사용해보세요.

**🔧 개발 참여**: 기여하고 싶다면 [개발자 가이드](CONTRIBUTING_KO.md)와 [코딩 스타일 가이드](CODING_STYLE_KO.md)를 참조하세요.

**📚 관련 문서**:
- [설치 가이드](INSTALLATION_KO.md) - 미리 컴파일된 바이너리 사용
- [의존성 가이드](DEPENDENCIES.md) - 외부 라이브러리 상세 정보
- [CI/CD 워크플로우](CICD_WORKFLOWS.md) - 자동화된 빌드 시스템

*이 가이드는 NexusSynth 개발팀에서 관리됩니다. 빌드 관련 문제가 있으면 [GitHub Issues](https://github.com/nexussynth/issues)에 신고해주세요.*