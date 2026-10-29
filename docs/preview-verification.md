# Verification — Turnforge v0.5.0

Date: 30 October 2026. Baseline: v0.4.1, commit `82950f1`. This report covers the enemy preview and regression checks. The [previous full QA report](qa-report.md) and its raw evidence are retained separately.

## Behavior and correctness

Preview simulates ending the current player turn on a copy of `Game` using the live `stepEnemy()` implementation. It returns the actual one-turn route and HP loss for each enemy that would act. It accounts for spawn order, current occupancy, flanking, solo pursuit, blocked enemies, Guard, and cancellation after a lethal hit. HP loss is capped at remaining HP. No live state, action, message, or journal entry is changed.

The board displays labeled E1/E2 movement arrows and destination outlines; an outline on the player tile marks an incoming attack. The HUD describes each action, total HP loss, and a lethal-result warning. The forecast is recomputed once per idle player frame. Blocking movement/attack/death presentation and non-player phases hide the overlay. Further player actions may change the forecast.

| Check | Result | Evidence |
|---|---|---|
| Core rules and preview | 24/24 scenarios, 352 checks; Release and core-only Debug passed | [Test output](preview-test-results.txt) |
| Preview correctness | Actual turn routes/HP loss match; full state remains unchanged; repeated calls deterministic | Four added core scenarios |
| Action updates and fallbacks | Movement, kills, solo AI, Guard, lethal cancellation, dead units, wall detours, blocked/corner cases, and phase gates passed | [Test output](preview-test-results.txt) |
| Existing stress suite | 500 seeded sessions, 64,433 actions, 1,126,045 checks; Release and Debug passed | [Test output](preview-test-results.txt) |
| Input controller | 25 assertions passed through raylib events | [Input output](preview-input-results.txt) |
| CLI and invalid assets | 17/17 cases passed, including short benchmark and spaced output path | [CLI results](preview-cli-results.json) |
| Render verifier | Eleven 1280×720 screenshots exported; animation, window-close state, win/loss, Guard, ten resets, and preview passed | [Smoke report](preview-smoke-verification.json) |

The four preview scenarios check PRD AC-18 and AC-19. The new preview view, Guard view, and affected gameplay/animation/enemy/result views were visually inspected for AC-20: arrows stay on the grid, E1/E2 labels agree with destinations, the incoming-attack outline is distinguishable, text fits, and overlays hide while actions are busy or the phase changes.

![Enemy preview](screenshots/v0.5/11-enemy-preview.png)

*Preview at the current position: ending the turn removes 4 HP.*

![Guard changes the forecast](screenshots/v0.5/09-guard.png)

*The same position after Guard: the forecast changes to 2 HP. The smoke sequence then verifies two actual hits remove 2 HP in total and protection expires.*

The short smoke run completed in 8.394 seconds / 360 frames, mean frame time 16.975 ms, p95 17.888 ms, and maximum timed enemy decision 0.0396 ms. Peak sampled working set was 140.20 MB, private memory 62.61 MB. These short-run figures are separate from the sustained run below. [Smoke memory](preview-smoke-memory.json).

## Sustained run

Windows, i5-10310U / Intel UHD, driver 31.0.101.2115, OpenGL 3.3, 1280×720, 4× MSAA requested, Release, 60 FPS cap. Tools: MinGW GCC 12.2.0, CMake 3.31.6, Ninja, raylib 5.5.

| Measurement | Observed |
|---|---|
| Duration / rendered frames | 180.013 seconds / 10,326 frames, no crash |
| Mean frame time | 17.120 ms, approximately 58.4 FPS |
| p95 frame time | 18.043 ms |
| Maximum timed actual enemy decision | 0.7797 ms |
| Peak working set / private memory | 142.59 MB / 65.73 MB |
| Memory samples | 361, sampled every 0.5 seconds |
| Required checks | All passed, including preview |
| Presentation active at timed endpoint | No |

Raw reports: [benchmark](preview-benchmark-verification.json), [memory](preview-benchmark-memory.json). The initial capture sequence is excluded from frame-time samples. Forecast calculation runs in normal player rendering and is included in measured frame times; the AI timer measures actual enemy actions separately. Automated matches continue with both AI roles and Guard every fourth player action.

The 60 FPS target was not fully met. AI time and sampled working set remained below the original 50 ms and 256 MB targets. Differences from the prior run are observations under local conditions, not evidence of an optimization. A valid animation may be active at a timed endpoint, while smoke mode must finish idle.

## Package

**Passed.** The v0.5 ZIP was extracted to a new directory outside the source tree with spaces in its path. Its executable passed the verifier with PATH restricted to Windows and System32: eleven screenshots, preview, Guard, character/input/window-close checks, win/loss, and ten resets. The run completed in 9.641 seconds and ended idle. Executable, artwork, and three bundled license hashes matched the installed package; the installed executable matched the verified Release build. Updated reports are included in the final archive with the same runtime hashes.

Raw report: [package verification](preview-package-verification.json).

## Reproduce and limits

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Package
powershell -ExecutionPolicy Bypass -File scripts/verify.ps1
powershell -ExecutionPolicy Bypass -File scripts/test-cli.ps1
powershell -ExecutionPolicy Bypass -File scripts/verify.ps1 -BenchmarkSeconds 180
```

Use the verifier's `-Executable` option for the extracted Windows package. The preview adds no dependency or asset; the character atlas and combat/AI rules are unchanged.

Input checks replay raylib events through the real controller; physical mouse/keyboard devices and Windows desktop callbacks have not been manually playtested. Hidden-window timings do not guarantee visible-window performance. The stress test is bounded, seeded correctness coverage with random restarts and map variations, not a difficulty or win-rate study. No claim is made of exhaustive leak detection, broad user comprehension, or testing other drivers, operating systems, compilers, and desktop DPI settings. Human playtesting, visible-window performance, balance, and a portfolio video remain future work. Portfolio publication and CV changes remain deferred.
