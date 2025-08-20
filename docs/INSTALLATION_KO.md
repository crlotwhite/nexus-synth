# NexusSynth 설치 가이드

이 가이드는 일반 사용자를 위한 NexusSynth 설치 방법을 설명합니다. 미리 컴파일된 바이너리를 사용하여 빠르고 쉽게 설치할 수 있습니다.

## 목차

1. [시스템 요구사항](#시스템-요구사항)
2. [설치 방법](#설치-방법)
3. [설치 확인](#설치-확인)
4. [첫 번째 실행](#첫-번째-실행)
5. [설정 및 구성](#설정-및-구성)
6. [업데이트 및 제거](#업데이트-및-제거)
7. [문제 해결](#문제-해결)

## 시스템 요구사항

### 최소 요구사항
- **운영체제**: Windows 10 (1903 이상), macOS 10.15, Ubuntu 18.04
- **프로세서**: Intel/AMD 64비트, 4코어 이상
- **메모리**: 4GB RAM
- **저장공간**: 2GB 이상
- **기타**: 인터넷 연결 (초기 다운로드용)

### 권장 요구사항
- **운영체제**: Windows 11, macOS 12 이상, Ubuntu 20.04 이상
- **프로세서**: 8코어 이상, 3.0GHz
- **메모리**: 8GB RAM 이상 (16GB 권장)
- **저장공간**: 10GB 이상 (SSD 권장)
- **그래픽**: GPU 가속 지원 (CUDA/OpenCL) - 선택사항

## 설치 방법

### Windows 설치

#### 방법 1: 자동 설치 프로그램 (권장)

1. **설치 파일 다운로드**
   ```
   https://github.com/nexussynth/nexus-synth/releases/latest/download/NexusSynth-Windows-Installer.exe
   ```

2. **설치 프로그램 실행**
   - 다운로드한 `NexusSynth-Windows-Installer.exe` 파일을 더블클릭
   - Windows SmartScreen 경고가 나타나면 "추가 정보" → "실행" 클릭
   - 설치 마법사를 따라 진행

3. **설치 옵션 선택**
   ```
   ✅ NexusSynth 엔진 (필수)
   ✅ 명령줄 도구 (CLI)
   ✅ 예제 보이스뱅크
   ✅ 문서 및 튜토리얼
   ☐ 개발자 도구 (고급 사용자용)
   ☐ GPU 가속 지원 (CUDA/OpenCL)
   ```

4. **설치 위치 확인**
   - 기본값: `C:\Program Files\NexusSynth\`
   - 필요시 경로 변경 가능

5. **설치 완료**
   - 바탕화면에 바로가기 생성
   - 시작 메뉴에 등록
   - 환경 변수 자동 설정

#### 방법 2: 수동 압축파일 설치

1. **압축파일 다운로드**
   ```
   https://github.com/nexussynth/nexus-synth/releases/latest/download/NexusSynth-Windows-x64.zip
   ```

2. **압축 해제**
   ```batch
   # PowerShell에서
   Expand-Archive -Path NexusSynth-Windows-x64.zip -DestinationPath C:\NexusSynth
   ```

3. **환경 변수 설정**
   - `시스템 속성` → `고급` → `환경 변수`
   - `Path`에 `C:\NexusSynth\bin` 추가

4. **바로가기 생성**
   ```batch
   # 배치 파일로 바로가기 생성
   echo @echo off > %USERPROFILE%\Desktop\NexusSynth.bat
   echo cd /d "C:\NexusSynth" >> %USERPROFILE%\Desktop\NexusSynth.bat
   echo nexussynth.exe %* >> %USERPROFILE%\Desktop\NexusSynth.bat
   ```

### macOS 설치

#### 방법 1: DMG 설치 패키지 (권장)

1. **DMG 파일 다운로드**
   ```
   https://github.com/nexussynth/nexus-synth/releases/latest/download/NexusSynth-macOS.dmg
   ```

2. **DMG 마운트 및 설치**
   ```bash
   # 터미널에서 자동 설치
   curl -LO https://github.com/nexussynth/nexus-synth/releases/latest/download/NexusSynth-macOS.dmg
   hdiutil mount NexusSynth-macOS.dmg
   cp -R "/Volumes/NexusSynth/NexusSynth.app" /Applications/
   hdiutil unmount /Volumes/NexusSynth
   ```

3. **보안 설정**
   - `시스템 환경설정` → `보안 및 개인정보 보호`
   - "확인되지 않은 개발자" 경고 시 "열기" 허용

#### 방법 2: Homebrew 설치

1. **Homebrew 설치** (없는 경우)
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

2. **NexusSynth 설치**
   ```bash
   # 공식 tap 추가
   brew tap nexussynth/tap
   
   # NexusSynth 설치
   brew install nexussynth
   ```

3. **설치 확인**
   ```bash
   nexussynth --version
   ```

#### 방법 3: 수동 압축파일 설치

1. **압축파일 다운로드**
   ```bash
   curl -LO https://github.com/nexussynth/nexus-synth/releases/latest/download/NexusSynth-macOS.tar.gz
   ```

2. **압축 해제 및 설치**
   ```bash
   tar -xzf NexusSynth-macOS.tar.gz
   sudo mv nexussynth /usr/local/bin/
   sudo chmod +x /usr/local/bin/nexussynth
   ```

### Linux 설치

#### Ubuntu/Debian

##### 방법 1: DEB 패키지 (권장)
```bash
# DEB 패키지 다운로드 및 설치
wget https://github.com/nexussynth/nexus-synth/releases/latest/download/nexussynth_1.0.0_amd64.deb
sudo dpkg -i nexussynth_1.0.0_amd64.deb

# 의존성 문제 해결 (필요시)
sudo apt-get install -f
```

##### 방법 2: APT 저장소
```bash
# 공식 저장소 추가
curl -fsSL https://nexussynth.com/gpg-key | sudo apt-key add -
echo "deb https://apt.nexussynth.com stable main" | sudo tee /etc/apt/sources.list.d/nexussynth.list

# 패키지 설치
sudo apt update
sudo apt install nexussynth
```

#### CentOS/RHEL/Fedora

##### 방법 1: RPM 패키지
```bash
# RPM 패키지 다운로드 및 설치
wget https://github.com/nexussynth/nexus-synth/releases/latest/download/nexussynth-1.0.0-1.x86_64.rpm
sudo rpm -ivh nexussynth-1.0.0-1.x86_64.rpm

# 또는 yum/dnf 사용
sudo yum localinstall nexussynth-1.0.0-1.x86_64.rpm
```

##### 방법 2: 수동 압축파일 설치
```bash
# 압축파일 다운로드
wget https://github.com/nexussynth/nexus-synth/releases/latest/download/NexusSynth-Linux-x64.tar.gz

# 압축 해제 및 설치
tar -xzf NexusSynth-Linux-x64.tar.gz
sudo cp nexussynth/bin/* /usr/local/bin/
sudo cp -r nexussynth/share/* /usr/local/share/
sudo ldconfig
```

#### Arch Linux

```bash
# AUR 패키지 설치 (yay 사용)
yay -S nexussynth-bin

# 또는 수동 빌드
git clone https://aur.archlinux.org/nexussynth.git
cd nexussynth
makepkg -si
```

## 설치 확인

설치가 완료되면 다음 명령어로 확인하세요:

### 버전 확인
```bash
# 터미널/명령 프롬프트에서
nexussynth --version

# 예상 출력:
# NexusSynth v1.0.0 - Next-generation vocal synthesis engine
# Copyright (c) 2025 NexusSynth Team
```

### 시스템 호환성 체크
```bash
nexussynth system-check

# 예상 출력:
# ✅ 시스템 호환성: OK
# ✅ 의존성 라이브러리: OK
# ✅ 멀티스레딩 지원: OK (8 cores detected)
# ✅ 메모리: OK (16GB available)
# ☐ GPU 가속: Not available (optional)
```

### 설치된 구성요소 확인
```bash
nexussynth --list-components

# 예상 출력:
# 📦 설치된 구성요소:
# ✅ 핵심 엔진 (nexussynth-core)
# ✅ WORLD Vocoder (world-vocoder)
# ✅ HMM 엔진 (hmm-engine)
# ✅ 명령줄 인터페이스 (nexussynth-cli)
# ✅ 변환 도구 (conversion-tools)
# ✅ 예제 데이터 (sample-data)
```

## 첫 번째 실행

### 도움말 확인
```bash
nexussynth --help

# 기본 명령어들:
# synthesize    - 음성 합성 수행
# convert       - 보이스뱅크 변환
# validate      - 보이스뱅크 검증
# system-info   - 시스템 정보 출력
```

### 간단한 테스트
```bash
# 내장된 테스트 음성으로 간단한 합성 테스트
nexussynth test-synthesis

# 예상 출력:
# 🎵 테스트 합성 시작...
# ✅ WORLD 분석: OK
# ✅ HMM 모델링: OK  
# ✅ 음성 합성: OK
# 🎉 테스트 완료! 파일 저장: test_output.wav
```

### 예제 보이스뱅크 테스트
```bash
# 설치된 예제 보이스뱅크 목록 확인
nexussynth list-examples

# 예제 보이스뱅크로 합성 테스트
nexussynth synthesize \
    --voicebank "examples/sample_voice" \
    --phoneme "a" \
    --note "C4" \
    --length 1.0 \
    --output "my_first_synthesis.wav"
```

## 설정 및 구성

### 기본 설정 파일 생성
```bash
# 사용자 설정 디렉터리 생성
nexussynth init-config

# 설정 파일 위치:
# Windows: %APPDATA%\NexusSynth\config.json
# macOS: ~/Library/Application Support/NexusSynth/config.json  
# Linux: ~/.config/nexussynth/config.json
```

### 기본 설정 예시
```json
{
  "nexussynth_config": {
    "version": "1.0",
    "default_settings": {
      "quality": "balanced",
      "sample_rate": 44100,
      "bit_depth": 16,
      "threads": "auto",
      "memory_limit": "4GB"
    },
    "paths": {
      "voicebank_directory": "~/NexusSynth/voicebanks",
      "output_directory": "~/NexusSynth/output",
      "temp_directory": "/tmp/nexussynth"
    },
    "advanced": {
      "enable_gpu": false,
      "cache_size": "1GB",
      "log_level": "info"
    }
  }
}
```

### 환경 변수 (선택사항)
```bash
# ~/.bashrc 또는 ~/.zshrc에 추가 (Linux/macOS)
export NEXUS_CONFIG_DIR="$HOME/.config/nexussynth"
export NEXUS_VOICEBANK_DIR="$HOME/NexusSynth/voicebanks"
export NEXUS_OUTPUT_DIR="$HOME/NexusSynth/output"
export NEXUS_THREADS=8
export NEXUS_MEMORY_LIMIT="6GB"

# Windows 환경 변수 설정
setx NEXUS_CONFIG_DIR "%APPDATA%\NexusSynth"
setx NEXUS_VOICEBANK_DIR "%USERPROFILE%\NexusSynth\voicebanks"
```

## 업데이트 및 제거

### 업데이트

#### 자동 업데이트 확인
```bash
# 업데이트 확인
nexussynth check-updates

# 사용 가능한 업데이트가 있는 경우:
# 🔄 새 버전 사용 가능: v1.0.1
# 📝 변경사항:
#   - 성능 향상
#   - 버그 수정
#   - 새로운 보이스뱅크 지원
# 
# 업데이트하려면: nexussynth update
```

#### 수동 업데이트

**Windows:**
```batch
REM 새 설치 프로그램 다운로드 및 실행
REM 기존 설정은 자동으로 보존됩니다
NexusSynth-Windows-Installer.exe
```

**macOS:**
```bash
# Homebrew 사용시
brew upgrade nexussynth

# 수동 설치시 - 새 DMG 다운로드 후 재설치
```

**Linux:**
```bash
# APT 사용시 (Ubuntu/Debian)
sudo apt update && sudo apt upgrade nexussynth

# 수동 설치시 - 새 패키지 다운로드 후 설치
```

### 제거

#### Windows
```batch
REM 제어판 > 프로그램 추가/제거에서 "NexusSynth" 제거
REM 또는 PowerShell에서:
Get-WmiObject -Class Win32_Product -Filter "Name='NexusSynth'" | ForEach-Object { $_.Uninstall() }

REM 사용자 데이터 삭제 (선택사항)
rmdir /s "%APPDATA%\NexusSynth"
```

#### macOS
```bash
# Homebrew 사용시
brew uninstall nexussynth

# 수동 설치시
sudo rm -rf /Applications/NexusSynth.app
sudo rm -f /usr/local/bin/nexussynth

# 사용자 데이터 삭제 (선택사항)
rm -rf "~/Library/Application Support/NexusSynth"
```

#### Linux
```bash
# APT 사용시 (Ubuntu/Debian)
sudo apt remove nexussynth
sudo apt autoremove

# RPM 사용시 (CentOS/RHEL)
sudo rpm -e nexussynth

# 사용자 데이터 삭제 (선택사항)
rm -rf ~/.config/nexussynth
```

## 문제 해결

### 자주 발생하는 설치 문제

#### 1. Windows SmartScreen 경고
```
문제: "Windows에서 PC를 보호했습니다" 메시지
해결방법:
1. "추가 정보" 클릭
2. "실행" 버튼 클릭
3. 또는 설치 파일 우클릭 → 속성 → 보안 탭 → "차단 해제" 체크
```

#### 2. macOS Gatekeeper 차단
```
문제: "확인되지 않은 개발자" 오류
해결방법:
1. 시스템 환경설정 → 보안 및 개인정보 보호
2. 일반 탭에서 "열기" 버튼 클릭
3. 또는 터미널에서: sudo spctl --master-disable
```

#### 3. Linux 권한 오류
```
문제: Permission denied 오류
해결방법:
1. sudo를 사용하여 설치: sudo dpkg -i package.deb
2. 실행 권한 설정: chmod +x nexussynth
3. 사용자를 audio 그룹에 추가: sudo usermod -a -G audio $USER
```

#### 4. 의존성 라이브러리 누락
```
문제: 공유 라이브러리를 찾을 수 없음
해결방법:
# Ubuntu/Debian
sudo apt install libstdc++6 libgcc-s1 libc6

# CentOS/RHEL
sudo yum install libstdc++ libgcc glibc

# 라이브러리 경로 갱신
sudo ldconfig
```

#### 5. 메모리 부족
```
문제: 설치 중 메모리 부족 오류
해결방법:
1. 다른 프로그램 종료
2. 가상 메모리 늘리기
3. 설치 후 불필요한 구성요소 제거
```

### 진단 도구

```bash
# 상세한 시스템 정보 수집
nexussynth diagnostic --output diagnostic_report.txt

# 설치 검증
nexussynth validate-installation

# 성능 테스트
nexussynth benchmark --quick
```

### 로그 파일 위치

**Windows:**
- `%APPDATA%\NexusSynth\logs\`
- `C:\ProgramData\NexusSynth\logs\`

**macOS:**
- `~/Library/Logs/NexusSynth/`
- `/var/log/nexussynth/`

**Linux:**
- `~/.local/share/nexussynth/logs/`
- `/var/log/nexussynth/`

### 지원 요청

설치 문제가 해결되지 않는 경우:

1. **GitHub Issues**: [문제 신고](https://github.com/nexussynth/nexus-synth/issues)
2. **Discord 커뮤니티**: 실시간 지원
3. **이메일**: support@nexussynth.com

**문제 신고시 포함할 정보:**
- 운영체제 및 버전
- 설치 방법 (자동/수동)
- 오류 메시지 전문
- 진단 보고서 (`nexussynth diagnostic` 결과)

---

**🎉 설치 완료!** 

이제 [엔진 사용법 가이드](ENGINE_USAGE_KO.md)를 확인하여 NexusSynth를 사용해보세요.

**📚 관련 문서:**
- [보이스뱅크 생성 가이드](VOICE_BANK_CREATION_KO.md)
- [UTAU 연동 가이드](UTAU_INTEGRATION_KO.md)
- [문제 해결 FAQ](TROUBLESHOOTING_FAQ.md)

*이 가이드는 NexusSynth 개발팀에서 관리됩니다. 최신 설치 정보는 [공식 웹사이트](https://nexussynth.com)에서 확인하세요.*