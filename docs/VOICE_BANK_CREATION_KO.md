# NexusSynth 보이스 뱅크 생성 가이드

이 가이드는 NexusSynth를 위한 고품질 보이스 뱅크를 생성하는 방법을 설명합니다. NexusSynth는 기존 UTAU 보이스 뱅크와 호환되지만, 최적화된 .nvm 형식으로 변환하여 더 나은 성능과 품질을 제공합니다.

## 목차

1. [개요](#개요)
2. [필요한 소프트웨어](#필요한-소프트웨어)
3. [보이스 뱅크 준비](#보이스-뱅크-준비)
4. [NVM 형식 변환](#nvm-형식-변환)
5. [품질 검증](#품질-검증)
6. [최적화 팁](#최적화-팁)
7. [문제 해결](#문제-해결)

## 개요

NexusSynth는 두 가지 방식으로 보이스 뱅크를 사용할 수 있습니다:

### 1. 기존 UTAU 보이스 뱅크 사용
- **장점**: 바로 사용 가능, 기존 라이브러리 활용
- **단점**: 실시간 처리 필요, 상대적으로 느린 속도

### 2. NVM 최적화 형식 사용 (권장)
- **장점**: 빠른 합성 속도, 고품질 출력, 메모리 효율성
- **단점**: 사전 변환 작업 필요

## 필요한 소프트웨어

### 필수 요구사항
- **NexusSynth**: 최신 버전 ([릴리스 페이지](https://github.com/nexussynth/releases))
- **CMake**: 버전 3.16 이상
- **C++ 컴파일러**: 
  - Windows: Visual Studio 2019 이상
  - macOS: Xcode Command Line Tools
  - Linux: GCC 9.0 이상 또는 Clang 10 이상

### 권장 도구
- **UTAU**: 기존 보이스 뱅크 테스트용
- **Audacity**: 오디오 편집 및 검증
- **Oremo**: 녹음용 (보이스 뱅크 직접 제작 시)

## 보이스 뱅크 준비

### 1. UTAU 보이스 뱅크 구조 확인

표준 UTAU 보이스 뱅크는 다음과 같은 구조를 가져야 합니다:

```
MyVoicebank/
├── oto.ini              # 음소 설정 파일 (필수)
├── character.txt        # 캐릭터 정보 (선택)
├── readme.txt          # 사용법 및 규약 (권장)
├── a.wav               # 음성 샘플들
├── ka.wav
├── sa.wav
└── ...
```

### 2. oto.ini 파일 검증

oto.ini 파일의 형식이 올바른지 확인하세요:

```ini
filename.wav=alias,offset,consonant,cutoff,preutterance,overlap
```

**예시:**
```ini
a.wav=a,100.5,50.2,0,150.3,25.1
ka.wav=ka,120.0,45.5,0,135.2,30.0
```

### 3. 오디오 파일 품질 확인

NexusSynth에서 최적의 결과를 얻으려면:

- **샘플링 레이트**: 44.1kHz 이상 (48kHz 권장)
- **비트 뎁스**: 16비트 이상 (24비트 권장)  
- **형식**: WAV 파일 (무손실)
- **노이즈**: 최소화된 깨끗한 녹음
- **볼륨**: 일정하고 적절한 레벨 (-12dB ~ -6dB 피크)

## NVM 형식 변환

### 1. 명령줄 도구 사용

가장 간단한 방법은 제공된 변환 스크립트를 사용하는 것입니다:

#### Windows
```batch
scripts\convert_voicebank.bat "C:\path\to\your\voicebank" "C:\output\directory"
```

#### macOS/Linux
```bash
./scripts/convert_voicebank.sh "/path/to/your/voicebank" "/output/directory"
```

### 2. NexusSynth CLI 직접 사용

더 세밀한 제어가 필요한 경우:

```bash
# 기본 변환
./nexussynth convert \
    --input "/path/to/voicebank" \
    --output "/path/to/output.nvm" \
    --format nvm

# 고품질 변환 (시간이 더 오래 걸림)
./nexussynth convert \
    --input "/path/to/voicebank" \
    --output "/path/to/output.nvm" \
    --format nvm \
    --quality high \
    --analysis-depth detailed

# 배치 처리 (여러 보이스 뱅크)
./nexussynth batch-convert \
    --config scripts/nexussynth_batch.json \
    --parallel 4
```

### 3. 변환 설정 커스터마이징

`nexussynth_batch.json` 파일을 수정하여 변환 옵션을 조정할 수 있습니다:

```json
{
  "conversion_config": {
    "quality_mode": "high",
    "analysis_settings": {
      "world_analysis": {
        "frame_period": 5.0,
        "f0_floor": 71.0,
        "f0_ceiling": 800.0
      },
      "hmm_training": {
        "max_iterations": 100,
        "convergence_threshold": 0.001,
        "context_features": ["phoneme", "position", "duration"]
      }
    },
    "output_settings": {
      "compression_level": 3,
      "include_metadata": true,
      "generate_preview": true
    }
  },
  "voice_banks": [
    {
      "name": "MyVoice",
      "input_path": "/path/to/input/MyVoice",
      "output_path": "/path/to/output/MyVoice.nvm",
      "character_info": {
        "name": "내 목소리",
        "author": "제작자명",
        "version": "1.0",
        "description": "설명"
      }
    }
  ]
}
```

## 변환 과정 상세 설명

### 1. 음성 분석 단계 (WORLD Vocoder)

```
[진행률] 음성 분석 중...
├── F0 추출 (기본 주파수) ████████████ 100%
├── 스펙트럴 엔벨로프 분석 ████████████ 100%  
├── 비주기성 분석 ████████████ 100%
└── 파라미터 검증 ████████████ 100%
```

### 2. HMM 모델 훈련

```
[진행률] HMM 모델 훈련 중...
├── 컨텍스트 특징 추출 ████████████ 100%
├── 모델 초기화 ████████████ 100%
├── EM 알고리즘 실행 (50회 반복) ████████████ 100%
└── 모델 최적화 ████████████ 100%
```

### 3. NVM 파일 생성

```
[진행률] NVM 파일 생성 중...
├── 데이터 압축 ████████████ 100%
├── 메타데이터 생성 ████████████ 100%
├── 무결성 체크섬 계산 ████████████ 100%
└── 파일 출력 ████████████ 100%

✅ 변환 완료! 
   입력: MyVoice (127 samples, 15.3 MB)
   출력: MyVoice.nvm (8.7 MB, 압축률: 43%)
   처리 시간: 3분 42초
```

## 품질 검증

### 1. 자동 품질 검사

변환 후 자동으로 실행되는 검증:

```bash
# 품질 검증 실행
./nexussynth validate --input MyVoice.nvm --report quality_report.html

# 검증 결과 예시
품질 점수: 94.2/100
├── 음질 (Audio Quality): 96.8/100
├── 포먼트 보존 (Formant Preservation): 92.1/100  
├── 연결 품질 (Transition Quality): 93.7/100
└── 표현력 (Expressiveness): 94.1/100
```

### 2. A/B 비교 테스트

기존 UTAU와 NexusSynth 결과를 비교:

```bash
./ab_comparison_tool \
    --original-voicebank "/path/to/original" \
    --nexus-voicebank "/path/to/converted.nvm" \
    --test-phrases "테스트용 가사.txt" \
    --output comparison_report.html
```

### 3. 수동 검증 체크리스트

- [ ] **기본 음소**: あ、か、さ、た、な 등이 자연스럽게 발음되는가?
- [ ] **CV 연결**: か+あ → かあ 같은 연결이 매끄러운가?
- [ ] **VCV 연결**: あ+か+あ → あかあ 연결이 자연스러운가?
- [ ] **피치 변화**: 높은 음과 낮은 음에서 음질이 유지되는가?
- [ ] **길이 조절**: 긴 음표와 짧은 음표가 적절히 표현되는가?
- [ ] **표현 플래그**: g, bre, bri 등의 효과가 적용되는가?

## 최적화 팁

### 1. 녹음 품질 최적화

```markdown
📋 **녹음 체크리스트**
- [ ] 일정한 거리에서 녹음 (마이크로부터 15-20cm)
- [ ] 동일한 볼륨 레벨 유지
- [ ] 배경 소음 최소화
- [ ] 입모양과 혀 위치 일정하게 유지
- [ ] 같은 감정/톤으로 녹음
```

### 2. oto.ini 설정 최적화

```ini
# 좋은 oto.ini 설정 예시
a.wav=a,100,50,0,150,25     # 적절한 preutterance와 overlap
ka.wav=ka,120,60,0,140,30   # 자음 길이를 충분히 확보

# 피해야 할 설정
bad.wav=bad,50,10,0,60,5    # preutterance 너무 짧음
bad2.wav=bad2,200,200,0,300,100  # overlap 너무 김
```

### 3. 변환 파라미터 튜닝

용도별 권장 설정:

#### 고품질 아카이브용
```json
{
  "quality_mode": "highest",
  "analysis_depth": "detailed",
  "compression_level": 1,
  "world_analysis": {
    "frame_period": 2.5
  }
}
```

#### 실시간 사용용
```json
{
  "quality_mode": "balanced", 
  "analysis_depth": "standard",
  "compression_level": 5,
  "world_analysis": {
    "frame_period": 5.0
  }
}
```

#### 배포용 (파일 크기 중요)
```json
{
  "quality_mode": "efficient",
  "analysis_depth": "fast", 
  "compression_level": 9,
  "world_analysis": {
    "frame_period": 10.0
  }
}
```

### 4. 성능 최적화

```bash
# 멀티코어 활용
export OMP_NUM_THREADS=8
export OMP_PROC_BIND=true

# 메모리 최적화
export NEXUS_MEMORY_LIMIT=4GB
export NEXUS_BUFFER_SIZE=1024

# 임시 파일 위치 (SSD 권장)
export NEXUS_TEMP_DIR="/path/to/fast/storage"
```

## 문제 해결

### 일반적인 오류와 해결법

#### 1. "oto.ini 파일을 찾을 수 없음"
```
오류: oto.ini file not found in /path/to/voicebank/
해결: 
- oto.ini 파일이 보이스 뱅크 루트 디렉터리에 있는지 확인
- 파일명이 정확한지 확인 (대소문자 구분)
- 파일 권한 확인
```

#### 2. "오디오 파일 로드 실패"
```
오류: Failed to load audio file: sample.wav
해결:
- WAV 파일 형식 확인 (PCM 16/24비트)
- 파일 손상 여부 확인
- 경로에 한글/특수문자 있는지 확인
```

#### 3. "메모리 부족"
```
오류: Out of memory during conversion
해결:
- 사용 가능한 RAM 확인 (8GB 이상 권장)
- NEXUS_MEMORY_LIMIT 환경변수 설정
- 작은 단위로 분할 처리
```

#### 4. "변환 속도가 너무 느림"
```
문제: 변환에 몇 시간씩 걸림
해결:
- quality_mode를 "fast"로 변경
- 멀티코어 처리 활성화 (OMP_NUM_THREADS)
- SSD 사용 권장
- 불필요한 백그라운드 프로그램 종료
```

### 디버그 정보 수집

문제가 지속되는 경우:

```bash
# 상세한 로그와 함께 변환 실행
./nexussynth convert \
    --input "/path/to/voicebank" \
    --output "/path/to/output.nvm" \
    --log-level debug \
    --log-file debug.log

# 시스템 정보 수집
./nexussynth system-info > system_info.txt

# 보이스 뱅크 분석 정보
./nexussynth analyze-voicebank "/path/to/voicebank" > analysis.txt
```

## 추가 리소스

### 공식 문서
- [UTAU 연동 가이드](UTAU_INTEGRATION_KO.md)
- [엔진 사용법](ENGINE_USAGE_KO.md)
- [빌드 가이드](BUILD_GUIDE_KO.md)

### 커뮤니티
- **GitHub Issues**: 버그 리포트 및 기능 요청
- **Discord**: 실시간 지원 및 커뮤니티 토론
- **YouTube**: 비디오 튜토리얼

### 예제 파일
- [샘플 보이스 뱅크](examples/sample_voicebank/)
- [변환 설정 템플릿](scripts/nexussynth_batch.json)
- [품질 검증 스크립트](tests/integration/ab_comparison/)

---

**💡 팁**: 처음 보이스 뱅크를 변환할 때는 작은 테스트 세트로 시작해서 결과를 확인한 후 전체 변환을 진행하는 것을 권장합니다.

**📞 지원**: 문제가 발생하면 GitHub Issues에 다음 정보와 함께 문의해주세요:
- 운영체제 및 버전
- NexusSynth 버전  
- 오류 메시지 전문
- 변환하려는 보이스 뱅크 정보

*이 가이드는 NexusSynth 개발팀에서 관리됩니다. 최신 버전은 [GitHub 저장소](https://github.com/nexussynth)에서 확인할 수 있습니다.*