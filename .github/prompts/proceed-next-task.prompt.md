---
mode: agent
tools: ['codebase', 'think', 'task-master-ai', 'memory', 'context7', 'microsoft-docs', 'sequentialthinking', 'websearch']
---

다음 작업 흐름을 따르세요. 각 단계는 순차적으로 수행하고, 문제 발생 시 즉시 수정 후 다음 단계로 진행합니다.

1) 작업 불러오기
- Taskmaster에서 현재 task와 subtasks 정보를 가져옵니다 (예: `task-master next`, `task-master show <id>`).
- 의존성, 우선순위, 상태를 확인합니다.

2) 탐색(계획)
- 관련 코드와 테스트 파일을 찾습니다. (프로젝트 트리, 변경 가능한 파일, CTest/pytest 등)
- 성공 기준과 제약(예: 플랫폼, 빌드 옵션, golden files)을 확실히 정의합니다.

3) 구현
- 필요한 코드 변경을 수행합니다. 작은 커밋 단위로 진행하고 각 변경에 대한 설명을 남깁니다.
- 변경 전후로 관련 유닛/통합 테스트를 작성 또는 갱신합니다.

4) 검증
- 빌드 및 테스트 실행: 예) `cmake --build . && ctest --output-on-failure` 또는 언어별 명령(`cargo test`, `pytest`, `dotnet test`).
- 출력 비교(예: WAV 비교)는 `audio_compare` 또는 프로젝트의 지정된 도구로 수행합니다.
- 실패 시: 실패 로그와 원인 분석 → 수정 → 재실행. 모든 수정 내용은 작업의 세부사항에 기록합니다.

5) 기록 및 상태 업데이트
- 하위 작업 단위로 진행 상황을 `task-master update-subtask --id=<id> --prompt="<요약 및 상세>"`로 타임스탬프와 함께 기록합니다.
- 상위 task가 완료되면 `task-master set-status --id=<id> --status=done` 으로 상태를 업데이트합니다.

6) 보고 및 확인 방법
- 사용자에게 검증을 재현할 최소 명령을 제공합니다(예: `ctest -R wrappers_integration_*` 또는 `task-master show 6`).
- 변경된 파일, 테스트, 실패/해결 로그, 최종 ctest 결과 요약을 포함한 간단한 확인 체크리스트를 남깁니다.

7) 필요시 연구 사용
- 최신 라이브러리/호환성 이슈가 의심되는 경우 `task-master research`를 사용해 최신 정보를 수집하고, 결과를 작업 로그에 추가합니다.

우선순위: 실패 복구 및 검증 우선. 모든 변경 후 반드시 자동화된 테스트가 통과해야 다음 단계로 진행합니다.
// ...existing code...
{ changed code }
//