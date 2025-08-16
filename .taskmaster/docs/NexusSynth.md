

# **NexusSynth: 현대적 UTAU 리샘플러 엔진을 위한 하이브리드 파라메트릭 및 통계적 프레임워크**

## **Executive Summary**

본 보고서는 UTAU(우타우) 커뮤니티를 위한 차세대 보컬 합성 리샘플러 엔진인 'NexusSynth'의 포괄적인 아키텍처 설계를 제안한다. NexusSynth의 핵심 철학은 WORLD 및 libllsm2와 같은 현대 보코더의 고충실도 신호 표현 능력과, HTS(HMM-based Speech Synthesis System)와 같은 통계 모델의 문맥 인식 기반 표현력 있는 파라미터 궤적 생성 능력을 융합하는 데 있다. 이는 기존의 단순 접합 방식 리샘플러가 가진 음질적 한계를 극복하고, 보다 자연스럽고 풍부한 표현력을 갖춘 보컬 합성을 목표로 한다.  
본 설계안의 목적은 자연스러움, 표현력, 그리고 성능 측면에서 전통적인 접합 합성(concatenative synthesis) 방식을 뛰어넘는 차세대 UTAU 리샘플러의 완전한 기술적 청사진을 제공하는 것이다. NexusSynth의 핵심 혁신은 다음과 같다: 다층적 신호 모델을 통한 정교한 음성 파라미터 표현, 음표의 음악적·언어적 문맥을 기반으로 음향 파라미터 궤적을 생성하는 통계적 생성 코어, 포르만트 이동 및 무성음 제어와 같은 고급 표현 제어 기능, 그리고 JIT(Just-In-Time) 컴파일 기술을 활용한 고성능 합성 백엔드. 이 아키텍처는 제공된 연구 자료들, 특히 libllsm2, WORLD, HTS Engine의 핵심 아이디어를 체계적으로 통합하여, UTAU 생태계 내에서 기술적 도약을 이룰 수 있는 실현 가능한 경로를 제시한다.  
---

## **제 1장: 기본 원리: 접합 방식에서 하이브리드 합성에 이르기까지**

### **1.1. UTAU 리샘플러 패러다임: 비판적 분석**

전통적인 UTAU 리샘플러는 접합 합성(concatenative synthesis) 원리에 기반하여 작동한다. 이 방식의 핵심은 사전에 녹음된 음소 단위의 오디오 조각들을 피치 변환(pitch transposition) 및 크로스페이딩(crossfading)과 같은 기법을 사용하여 이어 붙이는 것이다.1  
moresampler나 tn\_fnds와 같은 고전적인 리샘플러들이 이 접근법을 대표하며 2, 그 기저에는 대부분 PSOLA(Pitch-Synchronous Overlap-Add) 알고리즘 또는 그 변형이 자리 잡고 있다. PSOLA 알고리즘은 음성 파형의 피치 주기를 하나의 단위(grain)로 보고, 이 단위들을 반복하거나, 삭제하거나, 시간 축 상에서 이동시켜 중첩-가산(overlap-add)함으로써 목표 피치와 길이를 맞추는 시변환(time-domain) 방식이다.3  
이러한 접근법은 계산적으로 매우 효율적이며 원본 녹음의 음색(timbre)을 잘 보존한다는 장점이 있다. 하지만 근본적인 한계 또한 명확하다. 첫째, 음소와 음소의 경계면에서 부자연스러운 연결음(concatenation artifact)이 발생하기 쉽다. 둘째, 큰 폭의 피치 변환 시 비현실적인 결과를 초래한다. 이는 PSOLA가 기본 주파수(fundamental frequency)를 변경할 때 스펙트럼 포락선(spectral envelope), 즉 포르만트(formant)까지 함께 이동시키기 때문이다. 실제 인간의 발성에서는 피치가 변하더라도 포르만트 구조는 상대적으로 독립적으로 제어되지만, PSOLA 방식은 이러한 현상을 모사하지 못한다.9 결과적으로 피치를 높이면 소위 '앨빈과 슈퍼밴드' 효과(chipmunk effect)가 나타나고, 피치를 낮추면 소리가 둔탁하고 어두워지는 문제가 발생하며, 이는 UTAU 특유의 기계적인 음질의 주된 원인이 된다.

### **1.2. 파라메트릭 혁명: 소스-필터 모델과 보코더**

이러한 한계를 극복하기 위해 등장한 것이 보코더(vocoder) 기반의 파라메트릭 합성 방식이다. 특히 Masanori Morise에 의해 개발된 WORLD 보코더는 음성 신호를 지각적으로 의미 있는 파라미터들로 분해하는 패러다임의 전환을 가져왔다.10 WORLD는 음성 신호를 소스-필터 모델(source-filter model)에 입각하여 세 가지 핵심 요소로 분리한다: 성대의 진동 주기를 나타내는 기본 주파수(Fundamental Frequency,  
F0​), 성도의 공명 특성, 즉 음색을 결정하는 스펙트럼 포락선(Spectral Envelope, SP), 그리고 숨소리와 같은 비주기적(aperiodic) 성분을 나타내는 비주기성 포락선(Aperiodicity Envelope, AP)이 그것이다.11  
이러한 분해는 보컬 합성에서 결정적인 장점을 제공한다. 피치(F0​)와 음색(SP)을 독립적으로 제어할 수 있게 된 것이다. 즉, 피치를 변경하면서도 스펙트럼 포락선은 그대로 유지하거나 별도로 제어함으로써 PSOLA 방식의 근본적인 문제를 해결할 수 있다.9  
world4utau는 바로 이 WORLD 기술을 UTAU 생태계에 직접적으로 적용한 사례로, 파라메트릭 합성의 우수성을 입증했다. 이처럼 소스(source, F0​와 AP)와 필터(filter, SP)를 분리하는 개념은 현대의 고품질 음성 변환 및 합성 기술의 초석이며, NexusSynth 엔진이 기본 표현 방식으로 채택할 핵심 원리이다.

### **1.3. 통계적 도약: HTS를 이용한 생성 모델링**

HTS(HMM-based Speech Synthesis System)는 파라메트릭 접근법을 한 단계 더 발전시킨 통계적 생성 모델이다. WORLD와 같은 보코더가 기존 녹음에서 추출한 파라미터를 재사용하고 보간(interpolation)하는 데 그친다면, HTS는 은닉 마르코프 모델(Hidden Markov Model, HMM)을 사용하여 음향 파라미터(스펙트럼, F0​, 발성 길이 등)가 다양한 언어적, 운율적 문맥(context) 하에서 어떻게 변화하는지에 대한 통계적 모델을 구축한다.17  
합성 단계에서 HTS는 주어진 문맥에 통계적으로 가장 적합한 새로운 파라미터 궤적(trajectory)을 '생성'해낸다.29 이는 훈련 데이터에 명시적으로 존재하지 않았던 문맥에 대해서도 자연스러운 보컬 라인을 만들어낼 수 있음을 의미한다. 단순 파라메트릭 시스템에서 발생하는 파라미터 간의 부자연스러운 보간 문제를 해결하고, 전체 발화에 걸쳐 부드럽고 일관된 음향적 특성을 보장한다. 이 생성적 접근법은 NexusSynth가 지향하는 자연스러움과 표현력의 핵심적인 열쇠가 될 것이다.

### **1.4. NexusSynth의 철학: 하이브리드 아키텍처**

최첨단 보컬 합성 시스템은 단일 기술에 의존하지 않고 여러 접근법을 정교하게 통합하는 하이브리드 아키텍처를 채택하는 경향을 보인다. Dreamtonics사의 Synthesizer V의 핵심 기술 중 하나인 libllsm2가 그 대표적인 예이다.31  
libllsm2는 접합 합성과 통계적 파라메트릭 합성을 모두 지원하며, 원본 신호에 가까운 무손실에 가까운 다층적 신호 표현 방식을 사용한다. 특히, 고조파 모델의 정밀함과 WORLD와 같은 IFFT 기반 보코더의 유연성을 결합한 "Pulse-by-Pulse" (PbP) 합성 모드를 제공하는 것은 주목할 만하다.31  
NexusSynth 엔진은 이러한 하이브리드 철학을 적극적으로 수용한다. 신호 표현의 근간으로는 libllsm2에서 영감을 받은 고충실도 파라메트릭 표현 방식을 채택할 것이다. 그러나 음소 단위 파라미터들을 단순히 보간하는 대신, HTS와 유사한 통계적 코어를 사용하여 UTAU 악보가 제공하는 목표 음표들을 연결하는 파라미터 궤적(F0​ 윤곽, 스펙트럼 포락선 동특성 등)을 '생성'한다. 이는 현대 보코더의 고품질 분석/합성 능력과 통계 모델의 문맥 인식 기반 자연스러운 생성 능력을 결합하여 양쪽의 장점을 모두 취하는 전략이다.  
이러한 기술적 발전의 흐름은 보컬 합성 기술이 원시 파형(raw waveform)에서 점차 더 높은 수준의 추상화로 나아가고 있음을 보여준다. PSOLA는 피치 주기라는 낮은 수준에서 작동하고 3, WORLD는 이를 소스-필터 파라미터라는 한 단계 높은 수준으로 추상화했다.16 HTS는 여기서 한 걸음 더 나아가, 파라미터 자체를 넘어 문맥에 따른 파라미터의 '생성 규칙'을 통계적으로 모델링하는 최고 수준의 추상화를 달성했다.19  
libllsm2는 이러한 추상화의 정점에 있으며, 다층적 표현과 다중 합성 경로를 제공한다.31 NexusSynth 엔진은 이러한 진화의 경로상에서, 확립된 보코더 패러다임과 더욱 강력한 통계적 생성 패러다임 사이의 교량 역할을 하도록 설계되었다. 이는 UTAU 리샘플링 과정에 '문맥 지능(contextual intelligence)'이라는 개념을 도입하려는 의도적인 시도이다.  
---

## **제 2장: Nexus 엔진의 아키텍처 청사진**

### **2.1. 이중 단계 프로세스: 오프라인 분석과 실시간 합성**

NexusSynth 엔진은 효율성과 실시간성을 극대화하기 위해 명확한 이중 단계 프로세스로 설계된다. 이는 무거운 연산을 사전에 처리하고, 실시간 합성 단계에서는 가벼운 연산에 집중하기 위함이다.

* **1단계: 보이스뱅크 컨디셔닝 (오프라인):** 각 보이스뱅크에 대해 단 한 번 수행되는 전처리 과정이다. 사용자가 제공한 원본 .wav 파일들과 oto.ini 타이밍 정보를 분석하여, NexusSynth에 최적화된 컴팩트한 보이스뱅크 모델 파일(.nvm)로 변환한다. 이 사전 연산 과정은 음향 파라미터 추출, 문맥 라벨 생성, 그리고 통계 모델(HMM) 훈련을 포함하며, 실시간 합성 시의 부하를 획기적으로 줄이는 데 결정적인 역할을 한다.  
* **2단계: 리샘플링 및 합성 (실시간):** UTAU 엔진이 음표 시퀀스(.ust 데이터)와 함께 NexusSynth를 호출하면, 리샘플러는 사전에 컨디셔닝된 .nvm 파일과 주어진 음표 정보를 활용하여 새로운 음향 파라미터 궤적을 실시간으로 생성하고 최종 파형을 합성한다. 이 단계는 사용자의 입력에 즉각적으로 반응해야 하므로 고도의 최적화가 요구된다.

### **2.2. 데이터 흐름도**

NexusSynth의 실시간 합성 단계에서의 데이터 흐름은 다음과 같이 요약될 수 있다.

1. **입력:** UTAU로부터 음표 정보(가사, 피치, 길이, 플래그 등)를 입력받는다.  
2. **문맥 특징 추출:** 입력된 음표 데이터는 제 4장에서 상세히 기술될 고차원 문맥 벡터(context vector)로 변환된다. 이 벡터는 해당 음표의 음악적, 언어적 환경을 정량적으로 기술한다.  
3. **통계적 파라미터 생성:** HTS 코어는 이 문맥 벡터와 .nvm 파일에 저장된 통계 모델을 사용하여, 목표 F0​, SP, AP 궤적을 MLPG 알고리즘을 통해 생성한다.  
4. **표현 제어 모듈:** 사용자가 지정한 UTAU 플래그(g, bre 등)가 생성된 파라미터 궤적에 변환(transformation) 연산으로 적용된다.  
5. **합성 코어 (PbP):** 최종적으로 수정된 파라미터들은 Pulse-by-Pulse 합성기로 전달되어 출력 파형을 생성한다.  
6. **출력:** 해당 음표에 대한 .wav 오디오 조각이 UTAU 엔진으로 반환된다.

이러한 파이프라인 구조는 각 단계의 역할을 명확히 분리하여 모듈성과 확장성을 높인다.

### **2.3. 핵심 소프트웨어 컴포넌트 및 라이브러리 통합**

NexusSynth 엔진의 구현은 여러 고성능 오픈소스 라이브러리들을 유기적으로 결합하여 이루어진다. 이는 Synthesizer V와 같은 상용 소프트웨어가 검증된 라이브러리 생태계를 적극 활용하는 전략에서 영감을 얻은 것이다.

* **분석 엔진:** WORLD 12와  
  libllsm2 31의 알고리즘을 기반으로 하여, 견고하고 정확한 음향 파라미터 추출 기능을 구현한다.  
* **통계 코어:** HMM 기반의 통계 모델링을 구현한다. HMM 상태의 출력 확률 분포를 구성하는 가우시안 혼합 모델(Gaussian Mixture Models, GMM)의 복잡한 행렬 및 벡터 연산을 처리하기 위해 Eigen 라이브러리를 광범위하게 사용한다.32  
* **합성 백엔드:** libllsm2에서 제안된 Pulse-by-Pulse (PbP) 합성 방식을 구현한다.31 IFFT, 윈도잉 등 핵심적인 DSP 연산 루프는 JIT 최적화의 주요 대상이 된다.  
* **DSP 및 유틸리티 라이브러리:**  
  * Eigen: 모든 행렬/벡터 연산, GMM 계산, MLPG의 선형 방정식 풀이 등 수학적 연산의 근간을 이룬다.36  
  * oourafft: 고속 푸리에 변환(FFT)을 위해 사용된다.  
  * r8brain-free: 필요 시 고품질 샘플링 레이트 변환을 위해 사용된다.  
  * Asmjit: 성능에 민감한 핵심 합성 루프를 런타임에 최적화하기 위한 JIT 컴파일에 사용된다.41  
  * cJSON: 설정 파일 및 모델 직렬화(serialization)에 사용된다.

이러한 라이브러리 조합은 개발 효율성을 높이는 동시에, 최종 애플리케이션의 성능과 안정성을 보장하는 견고한 기반을 제공한다.  
---

## **제 3장: 고급 신호 분석 및 보이스뱅크 컨디셔닝**

NexusSynth의 오프라인 컨디셔닝 단계는 보이스뱅크의 원시 오디오 데이터를 통계 모델링에 적합한 정제된 파라미터로 변환하는 핵심 과정이다. 이 단계의 정밀도가 최종 합성 품질을 좌우한다.

### **3.1. 기본 주파수(F0) 추출**

* **알고리즘:** 기본 주파수(F0​) 추출의 정확도는 합성음의 자연스러움에 직접적인 영향을 미친다. WORLD 생태계에서 제공하는 Harvest 알고리즘은 기존의 DIO와 같은 방법에 비해 월등한 성능과 정확도를 보여주므로 우선적으로 고려된다.13 특히 가수의 목소리와 같이  
  F0​ 변화가 심한 신호에 강점을 보인다. 또 다른 강력한 후보로는 견고성으로 잘 알려진 YIN 알고리즘이 있다.44 목표는 각  
  .wav 파일에 대해 잡음이 적고 부드러운 F0​ 윤곽선을 얻는 것이다.  
* **처리:** 추출된 F0​ 윤곽선은 유성음/무성음(voiced/unvoiced) 구간 정보와 함께 .nvm 파일에 저장된다. 이 구간 정보는 HTS 모델 훈련 시 무성음 구간의 파라미터를 별도로 모델링하고, 합성 시 비주기성(aperiodicity)을 제어하는 데 필수적인 역할을 한다.

### **3.2. 스펙트럼 포락선(SP) 추출**

* **알고리즘:** 스펙트럼 포락선은 음색을 결정하는 가장 중요한 요소이다. WORLD의 CheapTrick 알고리즘은 F0​ 적응형(F0-adaptive) 스펙트럼 포락선 추정기로, F0​ 추출 과정에서 발생할 수 있는 오차에 대해 매우 견고하게 설계되어 있다.13 대안으로는 선형 예측 부호화(Linear Predictive Coding, LPC) 분석이 있다. LPC는 성도를 전극 필터(all-pole filter)로 모델링하여 포르만트를 효과적으로 추출할 수 있는 고전적이면서도 강력한 기법이다.45  
* **처리:** 추출된 스펙트럼 포락선은 통계 모델링에 용이하도록 HTS에서 표준적으로 사용되는 멜-일반화 켑스트럼 계수(Mel-Generalized Cepstral, MGC) 형태로 변환된다.49 이 컴팩트한 표현은 각  
  .wav 파일의 모든 프레임에 대해 계산되어 .nvm 파일에 저장된다.

### **3.3. 비주기성(AP) 추출**

* **알고리즘:** 비주기성(Aperiodicity)은 숨소리, 파열음의 노이즈 등 음성의 자연스러움과 표현력에 기여하는 중요한 요소이다. WORLD의 D4C 알고리즘은 단일 값이 아닌, 여러 주파수 대역에 대한 비주기성 정보를 제공하는 정교한 밴드-비주기성(band-aperiodicity) 표현을 사용한다.13  
* **처리:** 이러한 다중 밴드 표현은 특히 숨소리(breathiness)와 같은 미묘한 표현을 사실적으로 제어하는 데 매우 중요하다.12  
  libllsm2는 여기서 더 나아가 노이즈의 시간적 형태를 기술하는 고조파 모델을 제안하는데, 이는 향후 고급 기능 확장을 위한 아이디어를 제공한다.31 추출된 비주기성 파라미터 역시  
  .nvm 파일에 저장된다.

이러한 컨디셔닝 과정은 UTAU 보이스뱅크를 단순한 오디오 샘플의 집합에서 풍부한 통계적 모델로 변환시키는 근본적인 패러다임 전환을 의미한다. 전통적인 UTAU는 보이스뱅크를 이어 붙일 오디오 조각의 라이브러리로 취급한다.1 반면 NexusSynth의 컨디셔닝 과정은 이 조각들을 구조화된 음향 파라미터 데이터베이스로 변환하고, HTS 훈련 과정(제 4장)은 이 데이터베이스로부터 해당 가수의 발성 특성을 담은 확률적 모델(HMM)을 구축한다. 이 HMM은 특정 음소의 평균적인 스펙트럼, 그 분산(얼마나 변동하는 경향이 있는지), 그리고 시간에 따른 파라미터의 전이 확률 등을 모두 포함한다. 따라서  
.nvm 파일은 단순한 '데이터베이스'가 아니라, 해당 가수의 정체성과 발성 경향을 학습한 '통계적 사전 확률(statistical prior)'이다. 리샘플러가 새로운 오디오를 생성할 때, 이는 단순히 샘플을 '재생'하는 것이 아니라, UTAU 악보라는 문맥의 안내를 받아 이 학습된 확률 분포로부터 새로운 파라미터를 '샘플링'하는 과정이 된다. 이는 기존의 모든 UTAU 리샘플러와 근본적으로 다른 개념적 전환이며, 새로우면서도 가수의 정체성을 유지하는 보컬 퍼포먼스를 생성하는 핵심 원리이다.  
---

## **제 4장: 통계적 생성 코어: HTS를 이용한 궤적 모델링**

NexusSynth의 심장부는 HTS 기반의 통계적 생성 코어이다. 이 코어는 UTAU 악보의 문맥 정보를 입력받아, 자연스럽고 표현력 있는 음향 파라미터 궤적을 생성하는 역할을 담당한다.

### **4.1. 노래 합성을 위한 문맥 특징 설계**

HTS의 강력함은 문맥을 모델링하는 능력에서 비롯된다. 특히 노래 합성을 위해서는 단순한 음소적 문맥을 넘어 음악적 정보를 포괄하는 정교한 특징 설계가 필수적이다.53 NexusSynth는 UTAU의 각 음표 정보를 HTS 스타일의 완전 문맥 라벨(full-context label) 시퀀스로 변환한다. 이 변환 규칙은 HTS의 질문 파일(  
.hed)에 의해 정의된다. 일본어 VCV UTAU에 적합한 견고한 특징 집합은 다음과 같은 요소들을 포함해야 한다.

* **음소적 문맥:** 이전, 현재, 다음 음소 (예: a-ka-i)  
* **음표 단위 음악적 문맥:** 현재, 이전, 다음 음표의 MIDI 피치; 현재, 이전, 다음 음표의 길이; 마디 내에서의 음표 위치  
* **음소 단위 음악적 문맥:** 음표 내에서 현재 음소의 위치 (예: 3개 중 첫 번째); 현재 음소가 슬러(slur)의 일부인지 여부  
* **가사 문맥:** 현재 음소가 모음인지 자음인지; 음절/단어 내에서 음소의 위치

이러한 특징들은 .ust 파일과 oto.ini 설정으로부터 추출된다. .nvm 파일에 저장된 훈련된 HMM들은 이 질문들에 기반한 결정 트리(decision tree)를 통해 음향 파라미터를 클러스터링하게 된다.28 통계 모델의 품질은 입력 특징의 품질에 전적으로 의존하므로, 잘 설계된 문맥 집합은 미묘한 뉘앙스를 구분하여 풍부한 표현을 가능하게 하는 반면, 부실한 설계는 단조로운 합성 결과를 낳는다. 아래 표는 이러한 문맥 특징 설계를 구체화한 예시이다.

| 특징 범주 | 특징 이름 | 설명 | .hed 파일 패턴 예시 |
| :---- | :---- | :---- | :---- |
| **음소** | p1 (이전 음소) | 이전 음소의 종류 | QS "LL-Phone=a" {/A:-a+\*} |
|  | p2 (현재 음소) | 현재 음소의 종류 | QS "C-Phone=ka" {/B:ka-\*} |
|  | p3 (다음 음소) | 다음 음소의 종류 | QS "RR-Phone=i" {\*+i/C:\*} |
| **음악적 피치** | m1 (이전 음표) | 이전 음표의 MIDI 번호 | QS "P-Note=59" {/B:\*-\*/C:59\_\*} |
|  | m2 (현재 음표) | 현재 음표의 MIDI 번호 | QS "C-Note=60" {/C:60/\*} |
|  | m3 (다음 음표) | 다음 음표의 MIDI 번호 | QS "N-Note=61" {/D:61\_\*/\*+\*} |
| **음악적 길이** | d2 (현재 음표) | 현재 음표의 길이 (ms) | QS "C-Dur\>500" {/D:501\_\*} |
| **음악적 위치** | pos\_in\_bar | 마디 내에서의 위치 | QS "Pos\_in\_Bar=1" {/E:\*/F:1\_\*} |
|  | pos\_in\_note | 음표 내에서의 음소 순서 | QS "Pos\_in\_Note=1" {/G:1\_\*} |

### **4.2. 최대 우도 파라미터 생성(MLPG)의 수학적 프레임워크**

HTS 모델은 합성하고자 하는 출력의 각 프레임에 대해 가우시안 분포(평균 벡터와 공분산 행렬) 시퀀스를 제공한다. 가장 단순한 방법은 각 프레임의 평균 벡터를 그대로 사용하는 것이지만, 이는 정적 특징(MGC 등)과 동적 특징(델타, 델타-델타 계수) 간의 시간적 상관관계를 무시하게 되어, 결과적으로 뚝뚝 끊기는 부자연스러운 궤적을 생성한다.56  
MLPG(Maximum Likelihood Parameter Generation)는 이러한 문제를 해결하기 위한 알고리즘이다.21 MLPG는 주어진 HMM 모델 하에서 전체 관측 벡터  
O(정적 \+ 동적 특징)의 우도(likelihood)를 최대화하는 부드러운 정적 특징 궤적 C를 생성한다. 정적 특징과 동적 특징 간의 관계는 O \= WC라는 선형 변환으로 표현될 수 있으며, 여기서 W는 정적 특징으로부터 동적 특징을 계산하는 '윈도우 행렬(window matrix)'이다.21 알고리즘의 목표는 다음 선형 방정식을 풀어 최적의  
C를 찾는 것이다:  
(WTΣ−1W)C=WTΣ−1μ  
여기서 WT는 W의 전치행렬, μ는 HMM이 예측한 평균 벡터 시퀀스, $\\Sigma^{-1}$는 공분산 행렬의 역행렬(정밀도 행렬)이다.56 이 방정식은 거대하지만 대역(banded) 및 희소(sparse) 행렬인 $(W^T \\Sigma^{-1} W)$을 포함하므로,  
Eigen 라이브러리의 효율적인 솔버를 사용하여 풀 수 있다. 파이썬 라이브러리 nnmnkwii는 윈도우 행렬을 구성하고 이 시스템을 효율적으로 푸는 방법에 대한 참조 구현을 제공한다.60

### **4.3. 전역 분산(GV)을 이용한 자연스러움 향상**

표준 MLPG 알고리즘은 통계적 평균화로 인해 생성된 파라미터를 과도하게 평활화(over-smoothing)하는 경향이 있다. 이는 마치 여러 사람의 얼굴 사진을 평균내면 개성 없는 흐릿한 얼굴이 나오는 것과 같은 원리로, 결과적으로 답답하고 단조로운 소리를 만들어낸다.62  
이 문제를 완화하기 위해 전역 분산(Global Variance, GV) 개념이 도입되었다.21 GV는 MLPG의 목적 함수에 두 번째 항을 추가하는 방식으로 작동한다. 이 항은 생성된 궤적의 분산이 훈련 데이터로부터 학습된 목표 '전역 분산'에서 벗어날 경우 페널티를 부과한다. 이는 생성된 궤적이 자연스러운 다이나믹 레인지를 갖도록 유도하는 역할을 한다. 수정된 목적 함수는 다음과 같다:  
F(c)=wlogP(Wc∣λ)+logP(v(c)∣λgv​)  
여기서 두 번째 항이 GV 우도이며, $v(c)$는 궤적 c의 분산을 나타낸다.21 이 새로운 목적 함수는 닫힌 해(closed-form solution)가 존재하지 않으므로, 반복적인 최적화 과정을 통해 해를 구해야 한다. 이를 위해  
.nvm 파일은 각 파라미터 차원별로 학습된 GV 통계량을 저장하고 있어야 한다.  
---

## **제 5장: 실시간 합성과 표현 제어**

이 장에서는 통계 코어에서 생성된 파라미터 궤적을 실제 오디오 파형으로 변환하는 실시간 합성 과정과, UTAU 플래그를 통해 사용자가 음성의 표현력을 제어하는 메커니즘을 다룬다.

### **5.1. 합성 경로: Pulse-by-Pulse (PbP) 생성**

* **이론적 근거:** libllsm2는 PbP(Pulse-by-Pulse) 합성 모드를 핵심 기술로 제시한다.31 이 접근법은 각 피치 주기에 해당하는 성대 파형(glottal flow)을 주파수 영역에서 직접 생성하고, 이를 스펙트럼 포락선으로 필터링한 후, 중첩-가산 IFFT(Overlap-Add Inverse FFT)를 통해 시변환 파형으로 변환한다.  
* **알고리즘:**  
  1. 각 프레임에 대해, 통계적으로 생성된 F0​ 값을 기반으로 고조파 여기 신호(harmonic excitation signal) 스펙트럼을 생성한다.  
  2. 통계적으로 생성된 다중 밴드 비주기성 파라미터를 기반으로 노이즈 신호 스펙트럼을 생성한다.  
  3. 고조파와 노이즈 스펙트럼을 결합하여 최종 여기(excitation) 스펙트럼을 형성한다.  
  4. 이 여기 스펙트럼에 MGC로부터 변환된 스펙트럼 포락선을 곱한다.  
  5. 역 푸리에 변환(IFFT)을 수행하여 해당 프레임의 시변환 파형을 얻는다.  
  6. 중첩-가산(Overlap-Add, OLA) 기법을 사용하여 각 프레임의 파형을 부드럽게 이어 붙여 연속적인 최종 오디오를 생성한다.  
* **장점:** 이 방식은 WORLD 보코더의 합성 과정과 매우 유사하여 높은 음질을 보장한다.31 또한, 각 피치 주기 단위의 미세한 파라미터 수정이 가능하여, 다양한 표현 효과를 구현하는 데 이상적이다.

### **5.2. 표현 플래그의 알고리즘적 구현**

이 모듈은 HTS 코어가 생성한 파라미터 궤적에 대한 후처리기로 작동하며, 사용자가 UTAU 인터페이스를 통해 입력한 표준 플래그를 해석하여 음향적 변화를 만들어낸다.

* **성별/포르만트 시프트 (g 플래그):**  
  * **알고리즘:** 단순한 리샘플링이 아닌, 합성 전 스펙트럼 포락선을 직접 조작하여 구현한다. 효과적인 방법 중 하나는 \*\*LPC 극점 스케일링(LPC Pole Scaling)\*\*이다. MGC 스펙트럼 포락선을 LPC 계수로 변환한 뒤, LPC 다항식의 근(극점)을 찾아 그 각도를 조절하고, 다시 다항식을 재구성하는 방식이다.45 더 간단하면서도 효과적인 대안은 \*\*스펙트럼 워핑(Spectral Warping)\*\*으로, 합성 전에 스펙트럼 포락선의 주파수 축 자체를 비선형적으로 왜곡하는 방법이다.64  
  * **참조:** ESPER-Utau 리샘플러가 이 목적을 위한 g 플래그를 제공한다.74  
* **숨소리 (bre 플래그):**  
  * **알고리즘:** 음성의 숨소리 정도는 음원(voice source)에 포함된 비주기적(노이즈) 에너지의 양과 직접적인 상관관계가 있다.50  
    bre 플래그는 PbP 합성기에서 노이즈 성분을 생성하기 전에 비주기성 파라미터(AP)에 적용될 스케일링 팩터를 제어한다. 양수 값은 모든 주파수 대역의 비주기성 성분 진폭을 증가시켜 더 거친 숨소리를 만들고, 음수 값은 이를 억제하여 더 맑은 소리를 만든다.  
  * **참조:** WORLD의 다중 밴드 비주기성 표현은 이러한 미묘한 제어에 적합하며 12,  
    ESPER-Utau 역시 bre 플래그를 구현하고 있다.74  
* **밝기 (bri 플래그):**  
  * **알고리즘:** 음성의 밝기는 고주파수 에너지와 저주파수 에너지 간의 균형과 관련이 있다. 이는 생성된 스펙트럼 포락선에 \*\*스펙트럼 기울기(spectral tilt)\*\*를 적용하여 구현된다. 양수 bri 값은 스펙트럼 포락선의 고주파수 성분을 증폭시켜 더 밝고 선명한 소리를 만들고, 음수 값은 이를 감쇠시켜 더 부드럽고 어두운 소리를 만든다. 이는 로그 스펙트럼 영역에서 스펙트럼 포락선에 기울어진 직선을 곱하는 간단하면서도 효과적인 연산이다.  
  * **참조:** ESPER-Utau가 이 기능을 위한 bri 플래그를 제공한다.74

아래 표는 UTAU 플래그와 NexusSynth 내부의 파라미터 조작 간의 변환 관계를 명시한다. 이는 사용자의 직관적인 제어와 엔진 내부의 복잡한 DSP 연산을 연결하는 중요한 '번역 계층' 역할을 한다.

| UTAU 플래그 | 제어 파라미터 | NexusSynth 조작 | 핵심 알고리즘 | 관련 자료 |
| :---- | :---- | :---- | :---- | :---- |
| g (Gender) | 포르만트 시프트 | 스펙트럼 포락선 변형 | 스펙트럼 워핑 / LPC 극점 스케일링 | 45 |
| bre (Breathiness) | 비주기성 | 합성 전 AP 파라미터 스케일링 | 비주기성 에너지 제어 | 12 |
| bri (Brightness) | 스펙트럼 기울기 | SP에 기울기 필터 곱셈 | 스펙트럼 틸팅 | 74 |
| t (Pitch Offset) | F0​ 오프셋 | 생성된 F0​ 궤적에 가산적 이동 | 피치 컨투어 시프팅 | 74 |

---

## **제 6장: Just-In-Time (JIT) 컴파일을 통한 성능 최적화**

### **6.1. 연산 병목 지점 식별**

실시간 오디오 합성은 매우 높은 연산 부하를 요구하는 작업이다. 특히 제 5.1장에서 설명한 PbP 합성 루프는 오디오 버퍼가 생성될 때마다 실행되며, 그 내부에는 집약적인 DSP 연산이 포함되어 있다. 성능 최적화의 첫 단계는 이러한 '핫스팟(hotspot)'을 정확히 식별하는 것이다.

* **이론적 근거:** 합성 루프는 수많은 부동소수점 배열에 대해 반복적으로 수행되는 타이트한 루프(tight loop) 구조를 가지므로, 저수준 최적화의 효과가 극대화되는 지점이다.79  
* **주요 병목 지점:** PbP 합성 루프 내에서 가장 많은 연산량을 차지할 것으로 예상되는 부분은 다음과 같다:  
  1. 각 프레임에 대한 역 고속 푸리에 변환(IFFT).  
  2. 윈도잉 및 중첩-가산(Overlap-Add) 연산.  
  3. 스펙트럼 곱셈 및 표현 제어를 위한 필터링 연산.

### **6.2. Asmjit을 이용한 동적 코드 생성 전략**

* **개념:** 전통적인 AOT(Ahead-Of-Time) 컴파일 방식, 즉 C++ 컴파일러가 사전에 생성한 기계어 코드를 사용하는 대신, Asmjit 라이브러리를 사용하여 이러한 핵심 DSP 커널을 위한 기계어 코드를 *런타임에* 동적으로 생성할 수 있다.41  
* **장점:** JIT 컴파일의 가장 큰 장점은 사용자의 CPU 아키텍처에 완벽하게 최적화된 코드를 생성할 수 있다는 점이다. Asmjit은 런타임에 CPU의 기능을 질의하여 SSE, AVX, AVX-512와 같은 고급 벡터 명령어(SIMD, Single Instruction Multiple Data)의 사용 가능 여부를 확인하고, 이를 활용하는 코드를 즉석에서 생성할 수 있다.41 정적으로 컴파일된 바이너리는 가장 낮은 공통분모의 명령어 집합을 타겟으로 하거나, 여러 아키텍처를 위한 코드 경로를 모두 포함해야 하므로 효율성이 떨어진다.  
* **구현 계획:**  
  1. 엔진 초기화 시, Asmjit을 통해 현재 실행 환경의 CPU 기능(feature)을 감지한다.  
  2. 감지된 기능에 따라 사전에 준비된 DSP 커널 템플릿(예: AVX2에 최적화된 IFFT 루프) 중 최적의 것을 선택한다.  
  3. Asmjit의 Compiler 인터페이스를 사용하여 선택된 템플릿을 실행 가능한 메모리 영역에 어셈블한다.41  
  4. 이렇게 JIT 컴파일된 코드의 메모리 주소를 함수 포인터(function pointer)에 저장한다.  
  5. 실시간 합성 스레드는 일반 C++ 함수 대신, 이 고도로 최적화된 함수 포인터를 호출하여 DSP 연산을 수행한다.

### **6.3. 오버헤드와 실행 속도의 균형**

* **트레이드오프:** JIT 컴파일은 초기화 단계에서 코드를 생성하고 컴파일하는 데 시간이 걸리는 '웜업(warm-up)' 비용을 발생시킨다.80 하지만 UTAU 리샘플러의 사용 환경을 고려할 때 이는 충분히 수용 가능한 트레이드오프이다. 이 비용은 UTAU 프로젝트가 로드되거나 첫 음표가 렌더링될 때 단 한 번만 발생하며, 그 이후 수천, 수만 번 호출될 합성 프레임에서는 최적화된 코드가 실행되어 훨씬 큰 성능 이득을 얻을 수 있다.  
* **전략:** JIT 컴파일 과정은 UI 스레드를 차단하지 않도록 초기화 시 백그라운드 스레드에서 수행하는 것이 바람직하다. 한 번의 초기 비용으로 얻는 네이티브 벡터 명령어의 성능 향상은, 전체 합성 과정에서 발생하는 총 연산 시간을 크게 단축시킬 것이다.

---

## **제 7장: 시스템 통합 및 미래 전망**

### **7.1. 보이스뱅크 변환 파이프라인**

* **프로세스:** 제 3장에서 설명한 오프라인 컨디셔닝 프로세스를 수행하기 위한 별도의 커맨드 라인 도구가 개발될 것이다. 이 도구는 사용자가 기존 UTAU 보이스뱅크를 NexusSynth용 .nvm 모델로 변환할 수 있게 해준다.  
* **입력:** 표준 UTAU 보이스뱅크 폴더 경로.  
* **단계:**  
  1. oto.ini 파일을 파싱하여 각 음소의 시간 정보를 획득한다.  
  2. 각 .wav 파일에 대해 F0​, MGC, AP 파라미터를 추출한다.  
  3. 추출된 파라미터와 생성된 완전 문맥 라벨을 사용하여 문맥 의존적 HMM을 훈련한다.  
  4. 전역 분산(GV) 통계량을 계산하고 저장한다.  
  5. 훈련된 모든 모델(HMM, 결정 트리, GV 통계 등)을 단일의 최적화된 .nvm 파일로 직렬화(serialize)한다.

### **7.2. UTAU 엔진 사양과의 인터페이스**

NexusSynth 엔진은 UTAU 리샘플러 표준 인터페이스를 준수하는 커맨드 라인 실행 파일(.exe) 형태로 제공될 것이다. UTAU 엔진은 표준 인자(입력 wav, 출력 wav, 음표 피치, 벨로시티, 플래그, oto 파라미터 등)를 NexusSynth에 전달하고, NexusSynth는 렌더링된 .wav 파일을 표준 출력(stdout) 또는 지정된 파일 경로로 반환한다. 이를 통해 기존 UTAU 및 OpenUTAU 환경과의 완벽한 호환성을 보장한다.

### **7.3. 격차 해소: HTS에서 신경망 생성으로**

* **차세대 기술:** HTS 프레임워크는 강력하지만, 현재 보컬 합성 연구의 최전선은 ENUNU 플러그인이 사용하는 NNSVS나 DiffSinger와 같은 신경망 기반 접근법으로 이동하고 있다.89 이러한 시스템들은 HMM을 심층 신경망(Deep Neural Network, DNN)으로 대체하여 문맥 특징에서 음향 파라미터로의 매핑을 더욱 정교하게 모델링한다.  
* **NexusSynth의 미래 확장성:** NexusSynth의 아키텍처는 이러한 기술적 발전을 수용하기에 매우 유리한 구조를 가지고 있다. '문맥 특징 → 파라미터 생성 → 합성'이라는 핵심 데이터 파이프라인은 그대로 유지된다. 미래에는 HTS 기반의 통계 코어를 신경망 모델(예: ONNX 런타임 추론 엔진)로 교체함으로써, 주변 아키텍처의 큰 변경 없이도 시스템을 업그레이드할 수 있다. 이 경우, 보이스뱅크 컨디셔닝 파이프라인은 HMM 대신 신경망 모델을 훈련하도록 수정될 것이며, 이는 NNSVS의 훈련 워크플로우와 유사한 형태를 띨 것이다.92  
* **결론:** NexusSynth는 단순히 하나의 완성된 제품이 아니라, 미래 기술을 담을 수 있는 플랫폼으로 설계되었다. 견고하고 고성능인 파라메트릭 합성 프레임워크를 구축함으로써, 향후 신경망 기반 노래 합성 기술의 발전을 UTAU 생태계로 통합할 수 있는 직접적이고 논리적인 경로를 마련한다. 이는 UTAU 커뮤니티가 지속적으로 최신 기술의 혜택을 누릴 수 있도록 하는 중요한 교두보가 될 것이다.

#### **참고 자료**

1. To One In Paradise (Synthesizer V) \- PG Music Forums, 8월 12, 2025에 액세스, [https://www.pgmusic.com/forums/ubbthreads.php?ubb=showthreaded\&Number=644065](https://www.pgmusic.com/forums/ubbthreads.php?ubb=showthreaded&Number=644065)  
2. Moresampler does not render some notes (SOLVED\!\!) \- VocaVerse Network, 8월 12, 2025에 액세스, [https://vocaverse.network/threads/moresampler-does-not-render-some-notes-solved.8833/](https://vocaverse.network/threads/moresampler-does-not-render-some-notes-solved.8833/)  
3. PSOLA \- Wikipedia, 8월 12, 2025에 액세스, [https://en.wikipedia.org/wiki/PSOLA](https://en.wikipedia.org/wiki/PSOLA)  
4. pitch-synchronous overlap-add (TD-PSOLA) \- learnius, 8월 12, 2025에 액세스, [https://learnius.com/slp/9+Speech+Synthesis/1+Fundamental+Concepts/2+Technologies/pitch-synchronous+overlap-add+(TD-PSOLA)](https://learnius.com/slp/9+Speech+Synthesis/1+Fundamental+Concepts/2+Technologies/pitch-synchronous+overlap-add+\(TD-PSOLA\))  
5. Pitch-Synchoronous Overlap-Add (PSOLA) \- Colab, 8월 12, 2025에 액세스, [https://colab.research.google.com/github/Speech-Interaction-Technology-Aalto-U/itsp/blob/main/Representations/Pitch-Synchoronous\_Overlap-Add\_PSOLA.ipynb](https://colab.research.google.com/github/Speech-Interaction-Technology-Aalto-U/itsp/blob/main/Representations/Pitch-Synchoronous_Overlap-Add_PSOLA.ipynb)  
6. PITCH-SYNCHRONOUS WAVEFORM PROCESSING TECHNIQUES FOR TEXT-TO-SPEECH SYNTHESIS USING DIPHONES, 8월 12, 2025에 액세스, [https://courses.physics.illinois.edu/ece420/sp2019/5\_PSOLA.pdf](https://courses.physics.illinois.edu/ece420/sp2019/5_PSOLA.pdf)  
7. 3.13. Pitch-Synchoronous Overlap-Add (PSOLA) \- Introduction to Speech Processing, 8월 12, 2025에 액세스, [https://speechprocessingbook.aalto.fi/Representations/Pitch-Synchoronous\_Overlap-Add\_PSOLA.html](https://speechprocessingbook.aalto.fi/Representations/Pitch-Synchoronous_Overlap-Add_PSOLA.html)  
8. TD-PSOLA \- speech.zone, 8월 12, 2025에 액세스, [https://speech.zone/courses/speech-processing/module-6-speech-synthesis-waveform-generation-and-connected-speech/videos/td-psola/](https://speech.zone/courses/speech-processing/module-6-speech-synthesis-waveform-generation-and-connected-speech/videos/td-psola/)  
9. Pitch shifting and voice transformation techniques \- DSP-Book, 8월 12, 2025에 액세스, [https://dsp-book.narod.ru/Pitch\_shifting.pdf](https://dsp-book.narod.ru/Pitch_shifting.pdf)  
10. WORLD: A Vocoder-Based High-Quality Speech Synthesis System for Real-Time Applications \- IEICE Transactions, 8월 12, 2025에 액세스, [https://search.ieice.org/bin/summary.php?id=e99-d\_7\_1877](https://search.ieice.org/bin/summary.php?id=e99-d_7_1877)  
11. WORLD, 8월 12, 2025에 액세스, [http://www.isc.meiji.ac.jp/\~mmorise/world/english/](http://www.isc.meiji.ac.jp/~mmorise/world/english/)  
12. (PDF) WORLD: A Vocoder-Based High-Quality Speech Synthesis System for Real-Time Applications \- ResearchGate, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/304671740\_WORLD\_A\_Vocoder-Based\_High-Quality\_Speech\_Synthesis\_System\_for\_Real-Time\_Applications](https://www.researchgate.net/publication/304671740_WORLD_A_Vocoder-Based_High-Quality_Speech_Synthesis_System_for_Real-Time_Applications)  
13. mmorise/World: A high-quality speech analysis, manipulation and synthesis system \- GitHub, 8월 12, 2025에 액세스, [https://github.com/mmorise/World](https://github.com/mmorise/World)  
14. A practical method for generating whispers from singing voices: Application of improved phantom silhouette method \- J-Stage, 8월 12, 2025에 액세스, [https://www.jstage.jst.go.jp/article/ast/44/3/44\_E2249/\_article/-char/ja/](https://www.jstage.jst.go.jp/article/ast/44/3/44_E2249/_article/-char/ja/)  
15. WORLD: A Vocoder-Based High-Quality Speech Synthesis System for Real-Time Applications \- Semantic Scholar, 8월 12, 2025에 액세스, [https://www.semanticscholar.org/paper/WORLD%3A-A-Vocoder-Based-High-Quality-Speech-System-Morise-Yokomori/ba91dabec842d507a647aab97ad224b4abdc1635](https://www.semanticscholar.org/paper/WORLD%3A-A-Vocoder-Based-High-Quality-Speech-System-Morise-Yokomori/ba91dabec842d507a647aab97ad224b4abdc1635)  
16. JeremyCCHsu/Python-Wrapper-for-World-Vocoder \- GitHub, 8월 12, 2025에 액세스, [https://github.com/JeremyCCHsu/Python-Wrapper-for-World-Vocoder](https://github.com/JeremyCCHsu/Python-Wrapper-for-World-Vocoder)  
17. Welcome \- HMM/DNN-based speech synthesis system (HTS), 8월 12, 2025에 액세스, [https://hts.sp.nitech.ac.jp/](https://hts.sp.nitech.ac.jp/)  
18. Speech Synthesis Based on Hidden Markov Models \- University of Edinburgh Research Explorer, 8월 12, 2025에 액세스, [https://www.research.ed.ac.uk/files/15269212/Speech\_Synthesis\_Based\_on\_Hidden\_Markov\_Models.pdf](https://www.research.ed.ac.uk/files/15269212/Speech_Synthesis_Based_on_Hidden_Markov_Models.pdf)  
19. The HMM-based Speech Synthesis System (HTS) Version 2.0 \- CMU School of Computer Science, 8월 12, 2025에 액세스, [https://www.cs.cmu.edu/\~./awb/papers/ssw6/ssw6\_294.pdf](https://www.cs.cmu.edu/~./awb/papers/ssw6/ssw6_294.pdf)  
20. An introduction to statistical parametric speech synthesis \- University of Edinburgh Research Explorer, 8월 12, 2025에 액세스, [https://www.research.ed.ac.uk/files/8400597/An\_introduction\_to\_statistical\_parametric\_speech\_synthesis.pdf](https://www.research.ed.ac.uk/files/8400597/An_introduction_to_statistical_parametric_speech_synthesis.pdf)  
21. (PDF) Recent development of the HMM-based speech synthesis system (HTS), 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/39999862\_Recent\_development\_of\_the\_HMM-based\_speech\_synthesis\_system\_HTS](https://www.researchgate.net/publication/39999862_Recent_development_of_the_HMM-based_speech_synthesis_system_HTS)  
22. A tutorial on HMM speech synthesis (invited paper) \- University of Edinburgh Research Explorer, 8월 12, 2025에 액세스, [https://www.research.ed.ac.uk/files/8425701/A\_tutorial\_on\_HMM\_speech\_synthesis\_invited\_paper\_.pdf](https://www.research.ed.ac.uk/files/8425701/A_tutorial_on_HMM_speech_synthesis_invited_paper_.pdf)  
23. Deep Learning in Speech Synthesis \- Google Research, 8월 12, 2025에 액세스, [https://research.google.com/pubs/archive/41539.pdf](https://research.google.com/pubs/archive/41539.pdf)  
24. The HMM-based Speech Synthesis System Version 2.0 \- ResearchGate, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/309253810\_The\_HMM-based\_Speech\_Synthesis\_System\_Version\_20](https://www.researchgate.net/publication/309253810_The_HMM-based_Speech_Synthesis_System_Version_20)  
25. Statistical Parametric Speech Synthesis: From HMM to LSTM-RNN \- Google Research, 8월 12, 2025에 액세스, [https://research.google.com/pubs/archive/44312.pdf](https://research.google.com/pubs/archive/44312.pdf)  
26. Recent development of the HMM-based speech synthesis system (HTS) \- CMU School of Computer Science, 8월 12, 2025에 액세스, [https://www.cs.cmu.edu/\~awb/papers/apsipa2009/zen\_APSIPA2009.pdf](https://www.cs.cmu.edu/~awb/papers/apsipa2009/zen_APSIPA2009.pdf)  
27. The HMM-based Speech Synthesis System (HTS) Version 2.0 \- Tohoku University, 8월 12, 2025에 액세스, [https://tohoku.elsevierpure.com/en/publications/the-hmm-based-speech-synthesis-system-hts-version-20](https://tohoku.elsevierpure.com/en/publications/the-hmm-based-speech-synthesis-system-hts-version-20)  
28. an hmm-based speech synthesis system applied to english \- CMU School of Computer Science, 8월 12, 2025에 액세스, [https://www.cs.cmu.edu/\~awb/papers/IEEE2002/hmmenglish.pdf](https://www.cs.cmu.edu/~awb/papers/IEEE2002/hmmenglish.pdf)  
29. hts\_engine — HMM-based speech synthesis engine \- Ubuntu Manpage, 8월 12, 2025에 액세스, [https://manpages.ubuntu.com/manpages/focal/man1/hts\_engine.1.html](https://manpages.ubuntu.com/manpages/focal/man1/hts_engine.1.html)  
30. hts\_engine API, 8월 12, 2025에 액세스, [https://hts-engine.sourceforge.net/](https://hts-engine.sourceforge.net/)  
31. Sleepwalking/libllsm2: Low Level Speech Model (version 2.1) for high quality speech analysis-synthesis \- GitHub, 8월 12, 2025에 액세스, [https://github.com/Sleepwalking/libllsm2](https://github.com/Sleepwalking/libllsm2)  
32. 2.1. Gaussian mixture models \- Scikit-learn, 8월 12, 2025에 액세스, [https://scikit-learn.org/stable/modules/mixture.html](https://scikit-learn.org/stable/modules/mixture.html)  
33. andreacasalino/Gaussian-Mixture-Model: C++ library handling Gaussian Mixure Models \- GitHub, 8월 12, 2025에 액세스, [https://github.com/andreacasalino/Gaussian-Mixture-Model](https://github.com/andreacasalino/Gaussian-Mixture-Model)  
34. ozansener/GMM-with-Eigen: Gaussian Mixture Model Implementation using Eigen Library, 8월 12, 2025에 액세스, [https://github.com/ozansener/GMM-with-Eigen](https://github.com/ozansener/GMM-with-Eigen)  
35. Eigen (C++ library) \- Wikipedia, 8월 12, 2025에 액세스, [https://en.wikipedia.org/wiki/Eigen\_(C%2B%2B\_library)](https://en.wikipedia.org/wiki/Eigen_\(C%2B%2B_library\))  
36. How to use the Eigen library in C++ \- Creatronix, 8월 12, 2025에 액세스, [https://creatronix.de/how-to-use-the-eigen-library-in-cplusplus/](https://creatronix.de/how-to-use-the-eigen-library-in-cplusplus/)  
37. Eigen Library for Matrix Algebra in C++ \- QuantStart, 8월 12, 2025에 액세스, [https://www.quantstart.com/articles/Eigen-Library-for-Matrix-Algebra-in-C/](https://www.quantstart.com/articles/Eigen-Library-for-Matrix-Algebra-in-C/)  
38. How to use Eigen, the C++ template library for linear algebra? \- Stack Overflow, 8월 12, 2025에 액세스, [https://stackoverflow.com/questions/3257062/how-to-use-eigen-the-c-template-library-for-linear-algebra](https://stackoverflow.com/questions/3257062/how-to-use-eigen-the-c-template-library-for-linear-algebra)  
39. Eigen \- TuxFamily, 8월 12, 2025에 액세스, [https://eigen.tuxfamily.org/index.php?title=Main\_Page](https://eigen.tuxfamily.org/index.php?title=Main_Page)  
40. Guide to Linear Algebra With the Eigen C++ Library \- Daniel Hanson \- CppCon 2024, 8월 12, 2025에 액세스, [https://www.youtube.com/watch?v=99G-APJkMc0](https://www.youtube.com/watch?v=99G-APJkMc0)  
41. AsmJit, 8월 12, 2025에 액세스, [https://asmjit.com/](https://asmjit.com/)  
42. asmjit/asmjit: Low-latency machine code generation \- GitHub, 8월 12, 2025에 액세스, [https://github.com/asmjit/asmjit](https://github.com/asmjit/asmjit)  
43. Core \- API \- Docs \- AsmJit, 8월 12, 2025에 액세스, [https://asmjit.com/doc/group\_\_asmjit\_\_core.html](https://asmjit.com/doc/group__asmjit__core.html)  
44. Pitch detection algorithm \- Wikipedia, 8월 12, 2025에 액세스, [https://en.wikipedia.org/wiki/Pitch\_detection\_algorithm](https://en.wikipedia.org/wiki/Pitch_detection_algorithm)  
45. Formant Analysis using LPC, 8월 12, 2025에 액세스, [https://sail.usc.edu/\~lgoldste/Ling582/Week%209/LPC%20Analysis.pdf](https://sail.usc.edu/~lgoldste/Ling582/Week%209/LPC%20Analysis.pdf)  
46. LPC: To Formant, 8월 12, 2025에 액세스, [https://www.fon.hum.uva.nl/praat/manual/LPC\_\_To\_Formant.html](https://www.fon.hum.uva.nl/praat/manual/LPC__To_Formant.html)  
47. Formant Estimation with LPC Coefficients \- MATLAB & Simulink \- MathWorks, 8월 12, 2025에 액세스, [https://www.mathworks.com/help/signal/ug/formant-estimation-with-lpc-coefficients.html](https://www.mathworks.com/help/signal/ug/formant-estimation-with-lpc-coefficients.html)  
48. (PDF) A Comparative Study of Formant Frequencies Estimation Techniques \- ResearchGate, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/254455971\_A\_Comparative\_Study\_of\_Formant\_Frequencies\_Estimation\_Techniques](https://www.researchgate.net/publication/254455971_A_Comparative_Study_of_Formant_Frequencies_Estimation_Techniques)  
49. Preparing Data for Training an HTS Voice, 8월 12, 2025에 액세스, [http://www.cs.columbia.edu/\~ecooper/tts/data.html](http://www.cs.columbia.edu/~ecooper/tts/data.html)  
50. Analysis/synthesis and modification of the speech aperiodic component \- ResearchGate, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/223501151\_Analysissynthesis\_and\_modification\_of\_the\_speech\_aperiodic\_component](https://www.researchgate.net/publication/223501151_Analysissynthesis_and_modification_of_the_speech_aperiodic_component)  
51. Analysis/synthesis and modification of the speech aperiodic component ', 8월 12, 2025에 액세스, [https://perso.telecom-paristech.fr/grichard/Publications/SP96\_Richard.pdf](https://perso.telecom-paristech.fr/grichard/Publications/SP96_Richard.pdf)  
52. Installing Voicebanks \- utau.us, 8월 12, 2025에 액세스, [http://utau.us/vb.html](http://utau.us/vb.html)  
53. Linguistic Extension of UTAU Singing Voice Synthesis and Its Application from Japanese to Mandarin \- ResearchGate, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/362964382\_Linguistic\_Extension\_of\_UTAU\_Singing\_Voice\_Synthesis\_and\_Its\_Application\_from\_Japanese\_to\_Mandarin](https://www.researchgate.net/publication/362964382_Linguistic_Extension_of_UTAU_Singing_Voice_Synthesis_and_Its_Application_from_Japanese_to_Mandarin)  
54. (PDF) An HMM-based singing voice synthesis system \- ResearchGate, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/221484784\_An\_HMM-based\_singing\_voice\_synthesis\_system](https://www.researchgate.net/publication/221484784_An_HMM-based_singing_voice_synthesis_system)  
55. HMM-based Mandarin Singing Voice Synthesis Using Tailored Synthesis Units and Question Sets \- ACL Anthology, 8월 12, 2025에 액세스, [https://aclanthology.org/O13-5005.pdf](https://aclanthology.org/O13-5005.pdf)  
56. PARAMETER GENERATION ALGORITHMS FOR TEXT-TO-SPEECH SYNTHESIS WITH RECURRENT NEURAL NETWORKS \- Amazon Science, 8월 12, 2025에 액세스, [https://assets.amazon.science/06/d0/be59fdfa485196b346d0b59a8417/parameter-generation-algorithms-for-text-to-speech-synthesis-with-recurrent-neural-networks.pdf](https://assets.amazon.science/06/d0/be59fdfa485196b346d0b59a8417/parameter-generation-algorithms-for-text-to-speech-synthesis-with-recurrent-neural-networks.pdf)  
57. \[PDF\] Speech parameter generation algorithms for HMM-based speech synthesis, 8월 12, 2025에 액세스, [https://www.semanticscholar.org/paper/Speech-parameter-generation-algorithms-for-speech-Tokuda-Yoshimura/3d7465338dd3a93f171786e2e858faf18b47cc51](https://www.semanticscholar.org/paper/Speech-parameter-generation-algorithms-for-speech-Tokuda-Yoshimura/3d7465338dd3a93f171786e2e858faf18b47cc51)  
58. (PDF) Speech parameter generation algorithms for HMM-based speech synthesis, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/232647092\_Speech\_parameter\_generation\_algorithms\_for\_HMM-based\_speech\_synthesis](https://www.researchgate.net/publication/232647092_Speech_parameter_generation_algorithms_for_HMM-based_speech_synthesis)  
59. SPEECH PARAMETER GENERATION ALGORITHMS FOR HMM-BASED SPEECH SYNTHESIS, 8월 12, 2025에 액세스, [https://www.sp.nitech.ac.jp/\~tokuda/selected\_pub/pdf/conference/tokuda\_icassp2000.pdf](https://www.sp.nitech.ac.jp/~tokuda/selected_pub/pdf/conference/tokuda_icassp2000.pdf)  
60. Source code for nnmnkwii.functions.\_impl.mlpg \- Ryuichi Yamamoto, 8월 12, 2025에 액세스, [https://r9y9.github.io/nnmnkwii/v0.0.1/\_modules/nnmnkwii/functions/\_impl/mlpg.html](https://r9y9.github.io/nnmnkwii/v0.0.1/_modules/nnmnkwii/functions/_impl/mlpg.html)  
61. nnmnkwii/nnmnkwii/paramgen/\_mlpg.py at master · r9y9/nnmnkwii \- GitHub, 8월 12, 2025에 액세스, [https://github.com/r9y9/nnmnkwii/blob/master/nnmnkwii/paramgen/\_mlpg.py](https://github.com/r9y9/nnmnkwii/blob/master/nnmnkwii/paramgen/_mlpg.py)  
62. A Speech Parameter Generation Algorithm Considering Global Variance for HMM-Based Speech Synthesis \- ResearchGate, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/31483684\_A\_Speech\_Parameter\_Generation\_Algorithm\_Considering\_Global\_Variance\_for\_HMM-Based\_Speech\_Synthesis](https://www.researchgate.net/publication/31483684_A_Speech_Parameter_Generation_Algorithm_Considering_Global_Variance_for_HMM-Based_Speech_Synthesis)  
63. Ways to Implement Global Variance in Statistical Speech Synthesis \- ResearchGate, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/257303464\_Ways\_to\_Implement\_Global\_Variance\_in\_Statistical\_Speech\_Synthesis](https://www.researchgate.net/publication/257303464_Ways_to_Implement_Global_Variance_in_Statistical_Speech_Synthesis)  
64. Spectral Envelope Transformation in Singing Voice for Advanced Pitch Shifting \- MDPI, 8월 12, 2025에 액세스, [https://www.mdpi.com/2076-3417/6/11/368](https://www.mdpi.com/2076-3417/6/11/368)  
65. (PDF) A Detailed Analysis of a Time-Domain Formant-Corrected Pitch-Shifting Algorithm, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/255966071\_A\_Detailed\_Analysis\_of\_a\_Time-Domain\_Formant-Corrected\_Pitch-Shifting\_Algorithm](https://www.researchgate.net/publication/255966071_A_Detailed_Analysis_of_a_Time-Domain_Formant-Corrected_Pitch-Shifting_Algorithm)  
66. (PDF) Morphing spectral envelopes using audio flow \- ResearchGate, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/221480651\_Morphing\_spectral\_envelopes\_using\_audio\_flow](https://www.researchgate.net/publication/221480651_Morphing_spectral_envelopes_using_audio_flow)  
67. Voice Transformation Using Two-Level Dynamic Warping and Neural Networks \- MDPI, 8월 12, 2025에 액세스, [https://www.mdpi.com/2624-6120/2/3/28](https://www.mdpi.com/2624-6120/2/3/28)  
68. Morphing Spectral Envelopes Using Audio Flow \- CiteSeerX, 8월 12, 2025에 액세스, [https://citeseerx.ist.psu.edu/document?repid=rep1\&type=pdf\&doi=310e7c7ce51b9f9845b00717e1e1b69c842818d7](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=310e7c7ce51b9f9845b00717e1e1b69c842818d7)  
69. efficient spectral envelope estimation and its application to pitch \- International Conference on Digital Audio Effects (DAFx), 8월 12, 2025에 액세스, [https://dafx.de/paper-archive/2005/P\_030.pdf](https://dafx.de/paper-archive/2005/P_030.pdf)  
70. (PDF) Spectral Envelope Transformation in Singing Voice for Advanced Pitch Shifting, 8월 12, 2025에 액세스, [https://www.researchgate.net/publication/310664414\_Spectral\_Envelope\_Transformation\_in\_Singing\_Voice\_for\_Advanced\_Pitch\_Shifting](https://www.researchgate.net/publication/310664414_Spectral_Envelope_Transformation_in_Singing_Voice_for_Advanced_Pitch_Shifting)  
71. Spectral envelope conversion. | Download Scientific Diagram \- ResearchGate, 8월 12, 2025에 액세스, [https://www.researchgate.net/figure/Spectral-envelope-conversion\_fig1\_4087475](https://www.researchgate.net/figure/Spectral-envelope-conversion_fig1_4087475)  
72. Weighted Frequency Warping for Voice Conversion \- UPC, 8월 12, 2025에 액세스, [https://nlp.lsi.upc.edu/papers/erro07.pdf](https://nlp.lsi.upc.edu/papers/erro07.pdf)  
73. Structured Sparse Spectral Transforms and Structural Measures for Voice Conversion \- PMC, 8월 12, 2025에 액세스, [https://pmc.ncbi.nlm.nih.gov/articles/PMC6980218/](https://pmc.ncbi.nlm.nih.gov/articles/PMC6980218/)  
74. CdrSonan/ESPER-Utau: UTAU resampler using the ... \- GitHub, 8월 12, 2025에 액세스, [https://github.com/CdrSonan/ESPER-Utau](https://github.com/CdrSonan/ESPER-Utau)  
75. Aperiodicity Control in ARX-Based Speech Analysis-Synthesis Method \- ISCA Archive, 8월 12, 2025에 액세스, [https://www.isca-archive.org/eurospeech\_2001/ohtsuka01\_eurospeech.pdf](https://www.isca-archive.org/eurospeech_2001/ohtsuka01_eurospeech.pdf)  
76. Categorization in the Perception of Breathy Voice Quality and Its Relation to Voice Production in Healthy Speakers, 8월 12, 2025에 액세스, [https://pmc.ncbi.nlm.nih.gov/articles/PMC7201331/](https://pmc.ncbi.nlm.nih.gov/articles/PMC7201331/)  
77. Modeling of Breathy Voice Quality Using Pitch Strength Estimates \- PMC, 8월 12, 2025에 액세스, [https://pmc.ncbi.nlm.nih.gov/articles/PMC4937833/](https://pmc.ncbi.nlm.nih.gov/articles/PMC4937833/)  
78. Synthesis, analysis, and pitch modification of the breathy vowel \- SciSpace, 8월 12, 2025에 액세스, [https://scispace.com/pdf/synthesis-analysis-and-pitch-modification-of-the-breathy-22or2lpynv.pdf](https://scispace.com/pdf/synthesis-analysis-and-pitch-modification-of-the-breathy-22or2lpynv.pdf)  
79. c++ \- Realtime audio application, improving performance \- Stack Overflow, 8월 12, 2025에 액세스, [https://stackoverflow.com/questions/26832382/realtime-audio-application-improving-performance](https://stackoverflow.com/questions/26832382/realtime-audio-application-improving-performance)  
80. Just-in-time compilation \- Wikipedia, 8월 12, 2025에 액세스, [https://en.wikipedia.org/wiki/Just-in-time\_compilation](https://en.wikipedia.org/wiki/Just-in-time_compilation)  
81. Synthesizing JIT Compilers for In-Kernel DSLs \- UNSAT, 8월 12, 2025에 액세스, [https://unsat.cs.washington.edu/papers/geffen-jitsynth.pdf](https://unsat.cs.washington.edu/papers/geffen-jitsynth.pdf)  
82. Augmenting computer music with just-in-time compilation \- SciSpace, 8월 12, 2025에 액세스, [https://scispace.com/pdf/augmenting-computer-music-with-just-in-time-compilation-39ge4qfqxd.pdf](https://scispace.com/pdf/augmenting-computer-music-with-just-in-time-compilation-39ge4qfqxd.pdf)  
83. What is JIT compilation, exactly? : r/ProgrammingLanguages \- Reddit, 8월 12, 2025에 액세스, [https://www.reddit.com/r/ProgrammingLanguages/comments/1cvnluz/what\_is\_jit\_compilation\_exactly/](https://www.reddit.com/r/ProgrammingLanguages/comments/1cvnluz/what_is_jit_compilation_exactly/)  
84. Creating a simple JIT compiler in C++ | Tibor Djurica Potpara, 8월 12, 2025에 액세스, [https://ojdip.net/2014/06/simple-jit-compiler-cpp/](https://ojdip.net/2014/06/simple-jit-compiler-cpp/)  
85. p0prxx/asmjit-1: Complete x86/x64 JIT and Remote Assembler for C++ \- GitHub, 8월 12, 2025에 액세스, [https://github.com/p0prxx/asmjit-1](https://github.com/p0prxx/asmjit-1)  
86. Adventures in JIT compilation: Part 2 \- an x64 JIT \- Eli Bendersky's website, 8월 12, 2025에 액세스, [https://eli.thegreenplace.net/2017/adventures-in-jit-compilation-part-2-an-x64-jit/](https://eli.thegreenplace.net/2017/adventures-in-jit-compilation-part-2-an-x64-jit/)  
87. Unlocking PostgreSQL Performance with Just-In-Time (JIT) Compilation \- Medium, 8월 12, 2025에 액세스, [https://medium.com/@CoyoteLeo/unlocking-postgresql-performance-with-just-in-time-jit-compilation-dc271161fdf7](https://medium.com/@CoyoteLeo/unlocking-postgresql-performance-with-just-in-time-jit-compilation-dc271161fdf7)  
88. What are the advantages of just-in-time compilation versus ahead-of-time compilation?, 8월 12, 2025에 액세스, [https://stackoverflow.com/questions/2106380/what-are-the-advantages-of-just-in-time-compilation-versus-ahead-of-time-compila](https://stackoverflow.com/questions/2106380/what-are-the-advantages-of-just-in-time-compilation-versus-ahead-of-time-compila)  
89. Reference Thread: An Exhausting List of Vocal Synth Engines | VI-CONTROL, 8월 12, 2025에 액세스, [https://vi-control.net/community/threads/reference-thread-an-exhausting-list-of-vocal-synth-engines.147683/](https://vi-control.net/community/threads/reference-thread-an-exhausting-list-of-vocal-synth-engines.147683/)  
90. A Guide to NNSVS / ENUNU, 8월 12, 2025에 액세스, [https://nnsvs.carrd.co/](https://nnsvs.carrd.co/)  
91. NNSVS: A Neural Network-Based Singing Voice Synthesis Toolkit | LESS IS MORE, 8월 12, 2025에 액세스, [https://r9y9.github.io/projects/nnsvs/](https://r9y9.github.io/projects/nnsvs/)  
92. Getting started with recipes — nnsvs 0.1.0 documentation, 8월 12, 2025에 액세스, [https://nnsvs.github.io/recipes.html](https://nnsvs.github.io/recipes.html)  
93. How to build a new voicedb? · Issue \#22 · nnsvs/nnsvs \- GitHub, 8월 12, 2025에 액세스, [https://github.com/r9y9/nnsvs/issues/22](https://github.com/r9y9/nnsvs/issues/22)  
94. Creating your own Singing voice synthesizer \- The Audio Developer Conference, 8월 12, 2025에 액세스, [https://data.audio.dev/talks/ADCxSF/2023/creating-your-own-singing-voice-synthesizer/slides.pdf](https://data.audio.dev/talks/ADCxSF/2023/creating-your-own-singing-voice-synthesizer/slides.pdf)