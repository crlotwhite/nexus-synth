# NexusSynth 엔진 사용법 가이드

이 가이드는 NexusSynth 합성 엔진을 사용하여 고품질 보컬 합성을 수행하는 방법을 설명합니다. 초보자부터 고급 사용자까지 모든 레벨에 대응하는 포괄적인 사용법을 다룹니다.

## 목차

1. [시작하기](#시작하기)
2. [기본 사용법](#기본-사용법)
3. [고급 기능](#고급-기능)
4. [최적화 설정](#최적화-설정)
5. [문제 해결](#문제-해결)
6. [실전 예제](#실전-예제)

## 시작하기

### 시스템 요구사항

#### 최소 요구사항
- **운영체제**: Windows 10, macOS 10.15, Ubuntu 18.04 이상
- **메모리**: 4GB RAM
- **저장공간**: 2GB 이상
- **CPU**: 4코어 이상 (Intel/AMD 64비트)

#### 권장 요구사항
- **메모리**: 8GB 이상 RAM
- **저장공간**: 10GB 이상 (SSD 권장)
- **CPU**: 8코어 이상, 3.0GHz
- **GPU**: CUDA/OpenCL 지원 (선택사항)

### 설치 확인

설치가 올바르게 되었는지 확인:

```bash
# 버전 확인
./nexussynth --version
NexusSynth v1.0.0 - Next-generation vocal synthesis engine

# 도움말 확인
./nexussynth --help

# 시스템 호환성 체크
./nexussynth system-check
✅ 시스템 호환성: OK
✅ 의존성 라이브러리: OK  
✅ 멀티스레딩 지원: OK (8 cores)
✅ 메모리: OK (16GB available)
```

## 기본 사용법

### 1. 간단한 합성

가장 기본적인 사용법부터 시작해보겠습니다:

```bash
# 단일 음표 합성
./nexussynth synthesize \
    --voicebank "MyVoice.nvm" \
    --phoneme "a" \
    --note "C4" \
    --length 1.0 \
    --output "output.wav"
```

### 2. 멜로디 합성

MIDI 파일이나 UST 파일을 사용한 멜로디 합성:

```bash
# MIDI 파일로부터 합성
./nexussynth synthesize \
    --voicebank "MyVoice.nvm" \
    --midi "melody.mid" \
    --lyrics "lyrics.txt" \
    --output "song.wav"

# UTAU UST 파일 사용
./nexussynth synthesize \
    --voicebank "MyVoice.nvm" \
    --ust "song.ust" \
    --output "song.wav"
```

### 3. 명령줄 인터페이스 (CLI) 옵션

#### 기본 옵션
```bash
./nexussynth synthesize [OPTIONS]

필수 옵션:
  --voicebank PATH     사용할 보이스뱅크 파일 (.nvm 또는 UTAU 폴더)
  --output PATH        출력 오디오 파일 경로
  
입력 옵션 (하나 선택):
  --midi PATH          MIDI 파일 경로
  --ust PATH           UTAU UST 파일 경로
  --phoneme TEXT       단일 음소 (예: "a", "ka", "sa")
  --sequence TEXT      음소 시퀀스 (예: "ka a sa a")
  
음표 옵션:
  --note TEXT          음표 (예: "C4", "G5", "A#3")
  --pitch FLOAT        피치 (Hz, 예: 440.0)
  --length FLOAT       길이 (초, 예: 1.5)
  --tempo FLOAT        템포 (BPM, 기본값: 120)
```

#### 고급 옵션
```bash
품질 옵션:
  --quality MODE       품질 모드 (fast/balanced/high/highest)
  --sample-rate INT    샘플링 레이트 (22050/44100/48000)
  --bit-depth INT      비트 뎁스 (16/24/32)
  
표현 옵션:
  --vibrato DEPTH      비브라토 깊이 (0.0-1.0)
  --breathiness LEVEL  숨소리 레벨 (0.0-1.0)
  --brightness LEVEL   밝기 조절 (-1.0-1.0)
  --gender SHIFT       성별 변화 (-1.0-1.0)
  
성능 옵션:
  --threads INT        사용할 스레드 수
  --memory-limit SIZE  메모리 제한 (예: 4GB)
  --gpu                GPU 가속 사용
```

### 4. 설정 파일 사용

복잡한 설정은 JSON 파일로 관리할 수 있습니다:

```json
{
  "synthesis_config": {
    "voicebank": "/path/to/MyVoice.nvm",
    "quality": "high",
    "sample_rate": 44100,
    "bit_depth": 24,
    
    "expression": {
      "vibrato": 0.3,
      "breathiness": 0.1,
      "brightness": 0.2,
      "gender_shift": 0.0
    },
    
    "performance": {
      "threads": 8,
      "memory_limit": "4GB",
      "use_gpu": true,
      "buffer_size": 1024
    },
    
    "output": {
      "format": "wav",
      "normalize": true,
      "fade_in": 0.05,
      "fade_out": 0.1
    }
  }
}
```

설정 파일 사용:
```bash
./nexussynth synthesize --config synthesis_config.json --ust song.ust
```

## 고급 기능

### 1. 실시간 합성

실시간 스트리밍 합성을 위한 기능:

```bash
# 실시간 모드 시작
./nexussynth realtime \
    --voicebank "MyVoice.nvm" \
    --port 8080 \
    --buffer-size 256 \
    --latency low

# API를 통한 실시간 합성 요청
curl -X POST http://localhost:8080/synthesize \
  -H "Content-Type: application/json" \
  -d '{
    "phoneme": "ka",
    "pitch": 440.0,
    "length": 0.5,
    "expression": {
      "vibrato": 0.2
    }
  }'
```

### 2. 배치 처리

여러 파일을 한 번에 처리:

```bash
# 배치 설정 파일
cat > batch_config.json << EOF
{
  "batch_config": {
    "voicebank": "MyVoice.nvm",
    "parallel_jobs": 4,
    "output_directory": "./output",
    
    "jobs": [
      {
        "name": "song1",
        "input": "song1.ust",
        "output": "song1.wav"
      },
      {
        "name": "song2", 
        "input": "song2.mid",
        "lyrics": "song2_lyrics.txt",
        "output": "song2.wav"
      }
    ]
  }
}
EOF

# 배치 처리 실행
./nexussynth batch --config batch_config.json
```

### 3. 표현력 제어

#### UTAU 호환 플래그
```bash
# 기본 UTAU 플래그들
./nexussynth synthesize \
    --ust song.ust \
    --voicebank MyVoice.nvm \
    --flags "g-10,bre50,bri30" \
    --output song.wav

# 플래그 설명:
# g-10    : 성별 변화 (-100 ~ 100)
# bre50   : 숨소리 (0 ~ 100)  
# bri30   : 밝기 (0 ~ 100)
# cle20   : 선명도 (0 ~ 100)
```

#### NexusSynth 확장 플래그
```bash
# 확장된 표현 제어
./nexussynth synthesize \
    --ust song.ust \
    --voicebank MyVoice.nvm \
    --extended-flags "vib0.3,ten0.2,rough0.1,warm0.4" \
    --output song.wav

# 확장 플래그 설명:
# vib0.3   : 비브라토 강도 (0.0 ~ 1.0)
# ten0.2   : 긴장도 (0.0 ~ 1.0)  
# rough0.1 : 거칠기 (0.0 ~ 1.0)
# warm0.4  : 따뜻함 (0.0 ~ 1.0)
```

### 4. 오디오 후처리

```bash
# 고급 후처리 옵션
./nexussynth synthesize \
    --ust song.ust \
    --voicebank MyVoice.nvm \
    --output song.wav \
    --post-process \
    --normalize \
    --eq-preset "vocal-enhance" \
    --reverb "hall" \
    --compressor "gentle"
```

## 최적화 설정

### 1. 성능 최적화

#### CPU 최적화
```bash
# 환경변수 설정
export OMP_NUM_THREADS=8        # CPU 코어 수에 맞게 조정
export OMP_PROC_BIND=true       # 프로세서 바인딩
export OMP_PLACES=cores         # 코어 배치 전략

# 컴파일러 최적화 (빌드 시)
export CXXFLAGS="-O3 -march=native -mtune=native"
```

#### 메모리 최적화
```bash
# 메모리 설정
export NEXUS_MEMORY_LIMIT=6GB   # 메모리 사용 제한
export NEXUS_CACHE_SIZE=1GB     # 캐시 크기
export NEXUS_BUFFER_SIZE=2048   # 버퍼 크기

# 큰 보이스뱅크를 위한 설정
./nexussynth synthesize \
    --voicebank large_voice.nvm \
    --ust song.ust \
    --memory-efficient \
    --streaming-load \
    --output song.wav
```

#### GPU 가속
```bash
# GPU 사용 가능 여부 확인
./nexussynth gpu-info
Available GPUs:
  0: NVIDIA GeForce RTX 3080 (CUDA 11.8)
  1: AMD Radeon RX 6800 XT (OpenCL 2.1)

# GPU 가속 합성
./nexussynth synthesize \
    --ust song.ust \
    --voicebank MyVoice.nvm \
    --gpu 0 \
    --gpu-memory 4GB \
    --output song.wav
```

### 2. 품질 최적화

#### 품질 모드별 특징
```bash
# Fast 모드 (실시간 용도)
./nexussynth synthesize --quality fast
# - 처리 시간: 1x 실시간
# - 메모리 사용: 낮음
# - 품질: 기본

# Balanced 모드 (일반 용도) 
./nexussynth synthesize --quality balanced
# - 처리 시간: 2-3x 실시간
# - 메모리 사용: 보통
# - 품질: 양호

# High 모드 (고품질 용도)
./nexussynth synthesize --quality high  
# - 처리 시간: 5-10x 실시간
# - 메모리 사용: 높음
# - 품질: 우수

# Highest 모드 (마스터링 용도)
./nexussynth synthesize --quality highest
# - 처리 시간: 20-50x 실시간
# - 메모리 사용: 매우 높음
# - 품질: 최고
```

#### 샘플링 레이트 최적화
```bash
# 용도별 권장 샘플링 레이트
./nexussynth synthesize --sample-rate 22050  # 데모/테스트용
./nexussynth synthesize --sample-rate 44100  # 일반 음악용
./nexussynth synthesize --sample-rate 48000  # 전문/마스터링용
./nexussynth synthesize --sample-rate 96000  # 최고품질용
```

## 문제 해결

### 자주 발생하는 문제들

#### 1. 메모리 부족
```
오류: Out of memory during synthesis
해결방법:
1. 메모리 제한 설정: --memory-limit 2GB
2. 스트리밍 로드 사용: --streaming-load  
3. 품질 모드 낮추기: --quality fast
4. 배치 크기 줄이기: --batch-size 1
```

#### 2. 처리 속도 느림
```
문제: 합성에 너무 오랜 시간이 걸림
해결방법:
1. 멀티스레딩 활성화: --threads 8
2. GPU 가속 사용: --gpu
3. 품질 모드 조정: --quality balanced
4. 불필요한 후처리 비활성화
```

#### 3. 음질 문제
```
문제: 출력 음질이 만족스럽지 않음
해결방법:
1. 보이스뱅크 품질 확인
2. 샘플링 레이트 증가: --sample-rate 48000
3. 비트 뎁스 증가: --bit-depth 24
4. 품질 모드 상승: --quality high
```

### 디버깅 도구

```bash
# 상세한 로그 출력
./nexussynth synthesize \
    --ust song.ust \
    --voicebank MyVoice.nvm \
    --output song.wav \
    --verbose \
    --log-file debug.log \
    --log-level debug

# 성능 프로파일링
./nexussynth synthesize \
    --ust song.ust \
    --voicebank MyVoice.nvm \
    --output song.wav \
    --profile \
    --profile-output profile.json

# 메모리 사용량 모니터링
./nexussynth synthesize \
    --ust song.ust \
    --voicebank MyVoice.nvm \
    --output song.wav \
    --monitor-memory \
    --memory-report memory_usage.txt
```

## 실전 예제

### 예제 1: 단순한 노래 합성

```bash
#!/bin/bash
# simple_song.sh - 간단한 노래 합성 스크립트

VOICEBANK="voices/Sakura.nvm"
INPUT_UST="songs/simple_song.ust"  
OUTPUT="output/simple_song.wav"

echo "🎵 간단한 노래 합성 시작..."

./nexussynth synthesize \
    --voicebank "$VOICEBANK" \
    --ust "$INPUT_UST" \
    --output "$OUTPUT" \
    --quality balanced \
    --sample-rate 44100 \
    --bit-depth 16 \
    --normalize \
    --fade-in 0.1 \
    --fade-out 0.2

echo "✅ 합성 완료: $OUTPUT"
```

### 예제 2: 고품질 마스터링

```bash
#!/bin/bash  
# mastering_quality.sh - 고품질 마스터링용 합성

VOICEBANK="voices/Premium_Voice.nvm"
INPUT_UST="songs/main_vocal.ust"
OUTPUT="output/main_vocal_master.wav"

echo "🎚️ 고품질 마스터링 합성 시작..."

# 최고 품질 설정으로 합성
./nexussynth synthesize \
    --voicebank "$VOICEBANK" \
    --ust "$INPUT_UST" \
    --output "$OUTPUT" \
    --quality highest \
    --sample-rate 96000 \
    --bit-depth 32 \
    --threads 16 \
    --memory-limit 8GB \
    --post-process \
    --eq-preset "vocal-master" \
    --compressor "transparent" \
    --limiter "gentle" \
    --normalize-lufs -23.0

echo "✅ 마스터링 완료: $OUTPUT"

# 스펙트럼 분석 생성
./nexussynth analyze \
    --input "$OUTPUT" \
    --output "analysis/spectrum_analysis.png" \
    --type spectrum

echo "📊 분석 완료: analysis/spectrum_analysis.png"
```

### 예제 3: 배치 처리 워크플로우

```bash
#!/bin/bash
# batch_workflow.sh - 여러 노래 일괄 처리

VOICEBANK="voices/Main_Voice.nvm"
INPUT_DIR="input_songs"
OUTPUT_DIR="output_songs"
QUALITY="high"

echo "🚀 배치 처리 시작..."

# 출력 디렉터리 생성
mkdir -p "$OUTPUT_DIR"

# UST 파일들을 찾아서 처리
find "$INPUT_DIR" -name "*.ust" | while read ust_file; do
    filename=$(basename "$ust_file" .ust)
    output_file="$OUTPUT_DIR/${filename}.wav"
    
    echo "🎵 처리 중: $filename"
    
    ./nexussynth synthesize \
        --voicebank "$VOICEBANK" \
        --ust "$ust_file" \
        --output "$output_file" \
        --quality "$QUALITY" \
        --sample-rate 44100 \
        --normalize \
        --progress-bar
        
    echo "✅ 완료: $output_file"
done

echo "🎉 모든 배치 처리 완료!"
```

### 예제 4: 실시간 합성 서버

```bash
#!/bin/bash
# realtime_server.sh - 실시간 합성 서버 시작

VOICEBANK="voices/Realtime_Voice.nvm"
PORT=8080
BUFFER_SIZE=256

echo "🌐 실시간 합성 서버 시작..."

./nexussynth realtime \
    --voicebank "$VOICEBANK" \
    --port "$PORT" \
    --buffer-size "$BUFFER_SIZE" \
    --latency low \
    --quality fast \
    --threads 4 \
    --log-level info

echo "💻 서버가 http://localhost:$PORT 에서 실행 중..."
```

## API 참조

### REST API (실시간 모드)

```bash
# 서버 상태 확인
curl http://localhost:8080/status

# 단일 음소 합성
curl -X POST http://localhost:8080/synthesize \
  -H "Content-Type: application/json" \
  -d '{
    "phoneme": "ka",
    "pitch": 440.0,
    "length": 1.0
  }'

# 음소 시퀀스 합성  
curl -X POST http://localhost:8080/synthesize-sequence \
  -H "Content-Type: application/json" \
  -d '{
    "phonemes": ["ka", "a", "sa", "a"],
    "pitches": [440.0, 440.0, 493.88, 493.88],
    "lengths": [0.5, 0.5, 0.5, 0.5]
  }'
```

### 명령줄 완전 참조

```bash
./nexussynth --help

NexusSynth v1.0.0 - 차세대 보컬 합성 엔진

사용법:
  nexussynth <command> [options]

명령어:
  synthesize    음성 합성 수행
  convert       보이스뱅크 변환
  batch         배치 처리 
  realtime      실시간 서버 시작
  analyze       오디오 분석
  validate      보이스뱅크 검증
  system-info   시스템 정보 출력
  gpu-info      GPU 정보 출력

전역 옵션:
  --help        도움말 출력
  --version     버전 정보 출력
  --config      설정 파일 경로
  --verbose     상세 출력
  --quiet       최소 출력
  --log-file    로그 파일 경로
  --log-level   로그 레벨 (debug/info/warning/error)

자세한 명령어별 옵션:
  nexussynth <command> --help
```

---

**🎯 다음 단계**: 이 가이드를 마스터했다면 [고급 최적화 가이드](ADVANCED_OPTIMIZATION_KO.md)와 [플러그인 개발](PLUGIN_DEVELOPMENT_KO.md)을 확인해보세요.

**📚 관련 문서**: 
- [보이스 뱅크 생성 가이드](VOICE_BANK_CREATION_KO.md)
- [UTAU 연동 가이드](UTAU_INTEGRATION_KO.md)  
- [문제 해결 FAQ](TROUBLESHOOTING_FAQ.md)

*이 가이드는 NexusSynth 개발팀에서 관리됩니다. 최신 업데이트는 [GitHub 저장소](https://github.com/nexussynth)에서 확인하세요.*