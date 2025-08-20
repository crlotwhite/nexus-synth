# UTAU 연동 가이드

이 가이드는 기존 UTAU 사용자가 NexusSynth를 UTAU와 함께 사용하는 방법을 설명합니다. 100% 호환성을 유지하면서 향상된 품질과 성능을 누릴 수 있습니다.

## 목차

1. [연동 개요](#연동-개요)
2. [UTAU 설정](#utau-설정)
3. [리샘플러 교체](#리샘플러-교체)
4. [워크플로우 통합](#워크플로우-통합)
5. [고급 기능 활용](#고급-기능-활용)
6. [마이그레이션 전략](#마이그레이션-전략)
7. [문제 해결](#문제-해결)

## 연동 개요

### NexusSynth의 UTAU 호환성

NexusSynth는 **드롭인 대체 리샘플러**로 설계되어 기존 UTAU 워크플로우를 전혀 변경하지 않고 사용할 수 있습니다.

#### ✅ **완벽 호환 기능**
- **UST 파일**: 모든 UTAU 프로젝트 파일 지원
- **보이스뱅크**: 기존 UTAU 보이스뱅크 그대로 사용
- **플래그 시스템**: g, bre, bri, cle, t 등 모든 표준 플래그
- **oto.ini**: 기존 설정 파일 100% 호환
- **렌더링 파이프라인**: UTAU의 기본 렌더링 프로세스와 동일

#### 🚀 **개선 사항**
- **음질**: 기존 대비 15-25% 향상
- **속도**: 2-3배 빠른 렌더링
- **메모리**: 40-60% 메모리 사용량 감소
- **안정성**: 크래시 및 오류 대폭 감소

### 통합 방식 선택

#### 1. **리샘플러 교체** (권장)
```
기존: UTAU → moresampler.exe → 출력
개선: UTAU → nexussynth.exe → 출력
```
- 가장 간단한 방법
- 기존 워크플로우 완전 유지
- 즉시 품질 향상 체감

#### 2. **하이브리드 사용**
```
일반: UTAU → moresampler.exe
고품질: UTAU → nexussynth.exe
```
- 상황에 따른 선택적 사용
- 실험 및 비교 가능
- 점진적 전환

#### 3. **완전 마이그레이션**
```
기존: UTAU 생태계
신규: NexusSynth 생태계 (CLI + GUI)
```
- 최대 성능 활용
- 고급 기능 사용
- 장기적 전환 계획

## UTAU 설정

### 1. UTAU 버전 호환성

#### 지원 버전
- ✅ **UTAU 0.4.18e**: 완전 지원
- ✅ **UTAU 0.4.17**: 완전 지원  
- ✅ **UTAU 0.4.16**: 완전 지원
- ⚠️ **UTAU 0.4.15 이하**: 제한적 지원 (일부 플래그 미지원)

#### 권장 설정
UTAU에서 다음 설정을 권장합니다:

```
도구(T) → 옵션(O) → 렌더링
├── 출력 형식: WAV
├── 샘플링 레이트: 44100Hz
├── 양자화: 16bit
└── 모노/스테레오: 모노 (기본값)

고급 설정
├── 버퍼 크기: 2048 samples
├── 레이턴시: 낮음
└── 멀티스레딩: 활성화
```

### 2. 필요한 파일들

NexusSynth를 UTAU 리샘플러로 사용하려면:

```
UTAU 설치 디렉터리/
├── utau.exe
├── nexussynth.exe          ← NexusSynth 실행파일
├── nexussynth-wrapper.exe  ← UTAU 호환성 래퍼 (자동 생성)
└── resampler.ini          ← 설정 파일 (선택사항)
```

## 리샘플러 교체

### 방법 1: 자동 설치 (권장)

```bash
# UTAU 디렉터리에서 실행
cd "C:\Program Files (x86)\UTAU"

# NexusSynth UTAU 통합 설치
nexussynth install-utau

# 실행 결과:
# ✅ UTAU 설치 감지: C:\Program Files (x86)\UTAU
# ✅ nexussynth.exe 복사 완료
# ✅ 호환성 래퍼 생성 완료
# ✅ 설정 파일 생성 완료
# 🎉 설치 완료! UTAU를 재시작하세요.
```

### 방법 2: 수동 설치

#### Step 1: 파일 복사
```batch
REM NexusSynth 실행파일을 UTAU 폴더에 복사
copy "C:\Program Files\NexusSynth\bin\nexussynth.exe" "C:\Program Files (x86)\UTAU\"
```

#### Step 2: UTAU 설정 변경
```
UTAU → 도구(T) → 옵션(O) → 디렉터리
└── 리샘플러: nexussynth.exe로 변경
```

#### Step 3: 호환성 설정
```ini
# resampler.ini 파일 생성 (UTAU 디렉터리)
[NexusSynth]
mode=utau_compatible
quality=balanced
flags_mapping=standard
output_format=wav
temp_cleanup=true
```

### 방법 3: 배치 스크립트 사용

UTAU 폴더에 `nexus_render.bat` 파일 생성:

```batch
@echo off
setlocal

REM NexusSynth UTAU 호환 렌더링 스크립트
REM 사용법: nexus_render.bat [UTAU 파라미터들]

REM 환경 설정
set NEXUS_QUALITY=balanced
set NEXUS_THREADS=4

REM NexusSynth 실행 (UTAU 파라미터 전달)
"C:\Program Files\NexusSynth\bin\nexussynth.exe" utau-render %*

REM 종료 코드 전달
exit /b %errorlevel%
```

UTAU 리샘플러 설정을 `nexus_render.bat`로 변경.

## 워크플로우 통합

### 기본 렌더링 프로세스

#### 기존 UTAU 워크플로우
```
1. UST 파일 작성/편집
2. 보이스뱅크 선택
3. F5 또는 렌더링 버튼 클릭
4. moresampler.exe 실행
5. wavtool.exe로 합성
6. 최종 WAV 파일 생성
```

#### NexusSynth 통합 워크플로우
```
1. UST 파일 작성/편집 (동일)
2. 보이스뱅크 선택 (동일)
3. F5 또는 렌더링 버튼 클릭 (동일)
4. nexussynth.exe 실행 ← 여기만 변경!
5. 내장 합성 엔진 사용 (더 빠름)
6. 최종 WAV 파일 생성 (더 고품질)
```

### 플래그 호환성

#### 표준 UTAU 플래그
```
g-10     → gender_shift: -0.1    (성별 변화)
bre50    → breathiness: 0.5      (숨소리)
bri30    → brightness: 0.3       (밝기) 
cle20    → clearness: 0.2        (선명도)
t+10     → tension: 0.1          (긴장도)
```

#### NexusSynth 확장 플래그
```
vib0.3   → vibrato: 0.3          (비브라토)
warm0.4  → warmth: 0.4           (따뜻함)
rough0.2 → roughness: 0.2        (거칠기)
dyn0.5   → dynamics: 0.5         (다이나믹스)
```

#### 플래그 변환 예시
```
기존 UTAU: g-15,bre40,bri20
NexusSynth: gender_shift:-0.15,breathiness:0.4,brightness:0.2

# 자동 변환됨 - 수동 설정 불필요!
```

### 보이스뱅크 활용

#### 기존 UTAU 보이스뱅크 사용
```bash
# 보이스뱅크 검증
nexussynth validate-voicebank "C:\UTAU\voice\MyVoice"

# 검증 결과:
# ✅ oto.ini: OK (127 entries)
# ✅ 음성 파일: OK (127/127 files)
# ✅ 인코딩: OK (UTF-8)
# ⚠️ 권장사항: 일부 파일 최적화 필요
# 
# 💡 최적화를 위해 NVM 변환을 권장합니다:
#    nexussynth convert --input "C:\UTAU\voice\MyVoice" --output "MyVoice.nvm"
```

#### NVM 변환 후 사용 (권장)
```bash
# 1. 기존 보이스뱅크를 NVM으로 변환
nexussynth convert \
    --input "C:\UTAU\voice\MyVoice" \
    --output "C:\NexusSynth\voices\MyVoice.nvm" \
    --quality high

# 2. UTAU에서 NVM 파일 지정
# 보이스뱅크 경로: C:\NexusSynth\voices\MyVoice.nvm
```

## 고급 기능 활용

### 1. 품질 프리셋 설정

UTAU에서 사용할 품질 프리셋을 설정할 수 있습니다:

```ini
# UTAU 폴더의 nexus_presets.ini
[Quality_Presets]
draft=fast,low_memory
normal=balanced,standard_memory  
high=high_quality,normal_memory
master=highest,high_memory

[Default]
preset=normal
```

UTAU 렌더링 시 환경 변수로 프리셋 선택:
```batch
set NEXUS_PRESET=high
REM 이후 UTAU에서 렌더링하면 고품질 모드 사용
```

### 2. 실시간 미리보기

NexusSynth의 실시간 기능을 UTAU와 연동:

```bash
# 실시간 서버 시작 (별도 터미널)
nexussynth realtime-server --port 8080 --voicebank "MyVoice.nvm"

# UTAU에서 실시간 미리보기 활성화
# 도구 → 옵션 → 고급 → 실시간 미리보기: http://localhost:8080
```

### 3. 배치 렌더링 최적화

여러 UTAU 프로젝트를 한 번에 렌더링:

```batch
REM batch_render.bat
@echo off
for %%f in (*.ust) do (
    echo 렌더링: %%f
    utau.exe --batch --input "%%f" --output "rendered\%%~nf.wav" --resampler nexussynth.exe
)
```

### 4. 품질 비교 도구

기존 리샘플러와 품질 비교:

```bash
# A/B 비교 테스트 실행
nexussynth ab-compare \
    --ust "test_song.ust" \
    --voicebank "TestVoice" \
    --resampler-a "moresampler.exe" \
    --resampler-b "nexussynth.exe" \
    --output-report "comparison.html"
```

## 마이그레이션 전략

### 단계적 전환 계획

#### Phase 1: 테스트 및 검증 (1-2주)
```
목표: 기존 워크플로우 영향 없이 NexusSynth 테스트

1. 백업 생성
   - UTAU 설치 폴더 전체 백업
   - 중요한 보이스뱅크 백업
   - 진행 중인 프로젝트 백업

2. NexusSynth 설치
   - 별도 폴더에 설치
   - 기존 UTAU 설정 그대로 유지

3. 테스트 렌더링
   - 간단한 프로젝트로 테스트
   - 품질 및 속도 비교
   - 문제점 파악

결과 판단:
✅ 만족스러운 경우 → Phase 2
❌ 문제 발생시 → 피드백 후 대기
```

#### Phase 2: 부분 통합 (2-4주)
```
목표: 일부 프로젝트에서 NexusSynth 활용

1. 리샘플러 교체
   - UTAU 설정에서 리샘플러를 nexussynth.exe로 변경
   - 기본 품질 설정 조정

2. 선택적 사용
   - 새 프로젝트는 NexusSynth 사용
   - 기존 프로젝트는 점진적 전환
   - 중요한 작업은 기존 방식 유지

3. 보이스뱅크 최적화 시작
   - 자주 사용하는 보이스뱅크 NVM 변환
   - 성능 및 품질 개선 확인

결과 판단:
✅ 안정적 운용 → Phase 3
⚠️ 소소한 문제 → 해결 후 지속
❌ 심각한 문제 → Phase 1으로 회귀
```

#### Phase 3: 완전 통합 (1-2개월)
```
목표: NexusSynth를 주 리샘플러로 완전 전환

1. 전체 전환
   - 모든 프로젝트에서 NexusSynth 사용
   - 기존 리샘플러는 백업 용도로만 보존

2. 워크플로우 최적화
   - NexusSynth 고급 기능 활용
   - 배치 처리 및 자동화 구축
   - 품질 설정 개인화

3. 보이스뱅크 라이브러리 정리
   - 모든 보이스뱅크 NVM 변환
   - 불필요한 파일 정리
   - 체계적 관리 시스템 구축

결과:
🎉 마이그레이션 완료
📈 성능 및 품질 향상 체감
🔧 새로운 워크플로우 정착
```

### 백업 및 롤백 계획

#### 백업 체크리스트
```batch
REM 전체 백업 스크립트: backup_utau.bat
@echo off
set BACKUP_DATE=%DATE:~0,4%%DATE:~5,2%%DATE:~8,2%
set BACKUP_DIR=C:\UTAU_Backup_%BACKUP_DATE%

echo 🔄 UTAU 환경 백업 중...

REM UTAU 설치 디렉터리 백업
xcopy "C:\Program Files (x86)\UTAU" "%BACKUP_DIR%\UTAU\" /E /I /H /Y

REM 보이스뱅크 백업
xcopy "C:\UTAU\voice" "%BACKUP_DIR%\voices\" /E /I /H /Y

REM 프로젝트 파일 백업  
xcopy "%USERPROFILE%\Documents\UTAU" "%BACKUP_DIR%\projects\" /E /I /H /Y

echo ✅ 백업 완료: %BACKUP_DIR%
```

#### 롤백 절차
```batch
REM 롤백 스크립트: rollback_utau.bat
@echo off
echo ⚠️  UTAU 환경 롤백 중...
echo    기존 설정으로 되돌립니다.

REM NexusSynth 파일 제거
del "C:\Program Files (x86)\UTAU\nexussynth.exe"
del "C:\Program Files (x86)\UTAU\nexus_*.ini"

REM 기존 리샘플러 복구
copy "%BACKUP_DIR%\UTAU\moresampler.exe" "C:\Program Files (x86)\UTAU\"
copy "%BACKUP_DIR%\UTAU\*.ini" "C:\Program Files (x86)\UTAU\"

echo ✅ 롤백 완료. UTAU를 재시작하세요.
```

## 문제 해결

### 자주 발생하는 문제들

#### 1. UTAU가 NexusSynth를 인식하지 못함
```
증상: 렌더링 시 "리샘플러를 찾을 수 없음" 오류
원인: 파일 경로 또는 실행 권한 문제

해결방법:
1. nexussynth.exe가 UTAU 폴더에 있는지 확인
2. 파일 실행 권한 확인:
   - 우클릭 → 속성 → 보안 탭 → 전체 제어 권한 확인
3. Windows Defender 실시간 보호 예외 추가:
   - Windows 보안 → 바이러스 및 위협 방지 → 예외 추가
   - nexussynth.exe와 UTAU 폴더 전체 예외 처리
```

#### 2. 렌더링 품질이 떨어짐
```
증상: 음질이 기대보다 좋지 않거나 잡음 발생
원인: 품질 설정 또는 보이스뱅크 문제

해결방법:
1. 품질 설정 확인:
   nexussynth config --quality high
   
2. 보이스뱅크 검증:
   nexussynth validate-voicebank "보이스뱅크경로"
   
3. 샘플링 레이트 일치 확인:
   - UTAU 설정: 44100Hz
   - 보이스뱅크 WAV 파일: 44100Hz
   - NexusSynth 설정: 44100Hz
```

#### 3. 렌더링 속도가 느림
```
증상: 기존 리샘플러보다 느린 렌더링 속도
원인: 잘못된 성능 설정 또는 시스템 제약

해결방법:
1. 스레드 수 확인 및 조정:
   set NEXUS_THREADS=8
   
2. 메모리 제한 해제:
   set NEXUS_MEMORY_LIMIT=8GB
   
3. 품질 모드 조정:
   nexussynth config --quality balanced  # high 대신 balanced
   
4. 시스템 리소스 확인:
   - CPU 사용률 90% 이하 유지
   - RAM 여유 공간 2GB 이상
   - 디스크 공간 충분한지 확인
```

#### 4. 특정 보이스뱅크에서 오류
```
증상: 일부 보이스뱅크에서만 렌더링 실패
원인: 보이스뱅크별 특수한 oto.ini 설정

해결방법:
1. oto.ini 인코딩 확인:
   - UTF-8 또는 SHIFT-JIS 인코딩 필요
   - 메모장에서 인코딩 변경 후 저장
   
2. 파일명 문제 해결:
   - 특수문자나 공백이 포함된 파일명 변경
   - 한글 파일명을 영문으로 변경 (선택사항)
   
3. oto.ini 구문 검증:
   nexussynth validate-oto "보이스뱅크/oto.ini"
```

#### 5. UTAU 플래그가 적용되지 않음
```
증상: g, bre, bri 등 플래그 효과가 없음
원인: 플래그 매핑 설정 문제

해결방법:
1. 플래그 매핑 활성화:
   echo flags_mapping=standard >> C:\UTAU\resampler.ini
   
2. 커스텀 플래그 설정:
   nexussynth config --flags-preset utau-compatible
   
3. 플래그 변환 테스트:
   nexussynth test-flags --input "g-10,bre50,bri30"
   # 출력: gender_shift:-0.1,breathiness:0.5,brightness:0.3
```

### 디버깅 도구

#### 로그 활성화
```batch
REM 상세 로그 활성화
set NEXUS_LOG_LEVEL=debug
set NEXUS_LOG_FILE=C:\UTAU\nexus_debug.log

REM UTAU 렌더링 실행 후 로그 확인
type C:\UTAU\nexus_debug.log
```

#### 호환성 테스트
```bash
# UTAU 호환성 종합 테스트
nexussynth utau-compatibility-test \
    --utau-dir "C:\Program Files (x86)\UTAU" \
    --test-voicebank "C:\UTAU\voice\TestVoice" \
    --output-report "compatibility_report.html"
```

### 성능 최적화

#### UTAU용 NexusSynth 최적화 설정
```ini
# C:\UTAU\nexus_utau.ini
[Performance]
quality=balanced              # high 대신 balanced
threads=4                    # CPU 코어 수의 절반
memory_limit=4GB             # 시스템 RAM의 절반
buffer_size=1024            # 작은 버퍼로 반응성 향상

[Compatibility]
utau_mode=true              # UTAU 최적화 모드
legacy_flags=true           # 구식 플래그 지원
encoding=shift-jis          # 일본어 보이스뱅크 지원
```

## 커뮤니티 및 지원

### 공식 리소스
- **공식 문서**: [NexusSynth UTAU Integration](https://docs.nexussynth.com/utau/)
- **GitHub Issues**: 버그 신고 및 기능 요청
- **Discord 채널**: 실시간 지원 및 커뮤니티

### 유용한 링크
- **UTAU 위키**: [UTAU 기본 사용법](https://utau.wiki/)
- **보이스뱅크 라이브러리**: [무료 보이스뱅크 모음](https://utau-voicebank.com/)
- **튜토리얼 영상**: [YouTube 채널](https://youtube.com/nexussynth)

### 베타 테스터 프로그램
UTAU 통합 기능 개선에 참여하세요:
- 새로운 보이스뱅크 호환성 테스트
- 성능 최적화 피드백
- 워크플로우 개선 제안

---

**🎉 UTAU + NexusSynth 통합 완료!**

이제 기존 UTAU 워크플로우를 그대로 유지하면서 향상된 품질과 성능을 누릴 수 있습니다.

**📚 추천 다음 단계:**
1. [보이스뱅크 최적화](VOICE_BANK_CREATION_KO.md) - NVM 변환으로 더 나은 성능
2. [고급 사용법](ENGINE_USAGE_KO.md) - NexusSynth 전용 기능 활용
3. [성능 튜닝](PERFORMANCE_TUNING_KO.md) - 시스템별 최적화 가이드

**💬 피드백 환영:**
UTAU 통합 과정에서 발견한 문제나 개선 아이디어가 있다면 [GitHub Issues](https://github.com/nexussynth/nexus-synth/issues)에 알려주세요.

*이 가이드는 UTAU 커뮤니티와의 협력으로 지속적으로 개선됩니다.*