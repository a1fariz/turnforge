# Verification — Turnforge v0.4.1

Date: 30 October 2026. These are local results, not a claim of testing every Windows configuration.

## Environment

- Windows desktop, Intel Core i5-10310U, Intel UHD Graphics.
- OpenGL 3.3, Intel driver 31.0.101.2115 (reported by the running game).
- 1280x720 render framebuffer, 4x MSAA requested, 60 FPS cap.
- MinGW-w64 GCC 12.2.0, CMake 3.31.6, Ninja, raylib 5.5.
- Release build; statically linked compiler runtimes and raylib. PE import inspection lists only Windows system libraries: GDI32, KERNEL32, ucrtbase, SHELL32, USER32, WINMM.

## Core rules

**20/20 scenarios, 268 checks passed** through CTest. The checks remain enabled in Release builds. The test output is retained in [gameplay-test-results.txt](gameplay-test-results.txt).

Coverage includes BFS detours and unreachable cells, bounds/occupancy, dead units, action ordering, invalid attacks, phase guards, enemy movement/tie breaks, blocked AI, death mid-turn, complete victory/defeat, and ten resets. AI cases cover opposite-side flanking, deterministic decisions, the two-extra-step detour budget, and dead-ally/adjacent/blocked/corner fallbacks. The original map has a verified focus-fire victory in round 6 with 2 HP remaining.

Guard cases cover action consumption, moving before Guard, repeated activation, attack conflicts, invalid phases, two reduced hits, expiry with and without hits, subsequent full damage, title/restart cleanup, and lethal damage stopping later enemies. Guard is not healing or invulnerability.

The separate stress suite passed 500 seeded sessions / 64,433 actions / 1,126,045 checks in Release and Debug with checked standard-library assertions. It checks an independent path-distance oracle, state invariants, rejected-action preservation, and deterministic replay. [Stress results](qa-stress-results.txt). GCC analyzer compilation of the core completed without diagnostics.

## Render and reset smoke check

The actual raylib application ran with a hidden Windows window and exercised title, gameplay, enemy turn, victory, defeat, ten restarts, and Guard. Ten screenshots include movement, attack, active protection, and a reduced-damage hit, exported at the actual 1280x720 framebuffer size. Guard views were visually inspected for clipping, readable status, the shield, and the correct -1 damage indicator.

The v0.4.1 smoke run passed in **8.043 seconds / 360 frames**. Mean frame time was 17.019 ms, p95 18.059 ms, and maximum timed enemy decision 0.0277 ms. Sampled peak working set was 140.24 MB and peak private memory 63.47 MB. These short-run measurements are distinct from the sustained benchmark below.

Character self-checks passed: three usable alpha-bounded atlas cells; a route around a wall reaching its destination; attack/hit timers; actual reduced damage after Guard expires; death fade; restart cleanup; Escape-to-title during movement. The scripted Guard sequence verifies both enemies deal 1 damage each and protection expires on the next player turn.

The input self-check passed 25 assertions by replaying raylib keyboard/mouse events through the real input controller. It covers buttons, shortcuts, arena boundaries, invalid targets, phase restrictions, animation locks, and restart/Escape. A raylib window-close event checks the close state. This does not operate physical desktop devices. [Input results](qa-input-results.txt). The command-line/asset regression suite passed 17 cases, including a short positive benchmark, malformed durations, missing/corrupt/empty artwork, invalid output paths, and paths with spaces. [CLI results](qa-cli-results.json).

The smoke check must finish idle. Timed benchmarks may stop during a valid animation; the report records that separately as presentation_active_at_end. This preserves the corrected v0.3 verifier criterion without weakening gameplay, character, result, or reset checks.

Raw reports: [smoke-verification.json](smoke-verification.json), [smoke-memory.json](smoke-memory.json).

## Sustained benchmark

The v0.4.1 hidden-window run completed **180.008 seconds / 10,418 rendered frames**, without a crash. The initial 360-frame capture sequence is excluded from sustained frame-time samples. Approximate FPS below is calculated from mean sampled frame time, not total frames including capture overhead.

| Measurement | Observed |
|---|---|
| Mean frame time | 17.138 ms (approximately 58.4 FPS) |
| p95 frame time | 18.056 ms |
| Maximum timed enemy decision | 0.3386 ms |
| Peak process working set | 139.99 MB |
| Peak private memory | 63.19 MB |
| Memory samples | 360, sampled every 0.5 seconds |
| Character checks, victory/defeat, ten resets, Guard | Passed |
| Presentation active at timed endpoint | Yes; valid timed endpoint |

The 60 FPS target was not fully reached in this run. AI time and sampled memory remained below the initial 50 ms and 256 MB targets. Sampled private memory was approximately 46.18 MB at 60 seconds, 48.16 MB at 121 seconds, and 46.95 MB at the final sample. This does not prove absence of leaks. The benchmark cycles matches with both enemy roles, character effects, and Guard every fourth automated player action.

Raw reports: [benchmark-verification.json](benchmark-verification.json), [benchmark-memory.json](benchmark-memory.json).

## Package check

**Passed.** The v0.4.1 ZIP was extracted to a new directory outside the source tree with spaces in its path. Its executable passed the smoke verifier with PATH restricted to Windows and System32: 25 input assertions, window-close state, character checks, Guard, victory/defeat, ten screenshots, and ten resets. The run completed in 7.872 seconds and ended idle. Executable, artwork, and bundled license SHA-256 hashes matched the installed package; the installed executable matched the verified Release build. PE imports contain only the Windows system libraries listed above.

Raw report: [package-verification.json](package-verification.json). Updated documentation is included in the final ZIP; runtime files retain the verified hashes.

## Practical limits

- Automated checks exercise the model and raylib input controller, including pixel-coordinate boundaries. Physical mouse/keyboard interaction has not been manually playtested.
- Hidden-window performance can differ from a visible, focused game window and from other system power/thermal conditions.
- Ten resets verify clean state and successful rendering; sampled process memory alone is not a proof that all possible leaks are absent.
- The game is one fixed arena with two melee enemies. There is no audio, persistent saving, networking, or recorded portfolio video.
- Character animation uses code-driven transforms/effects on three single-pose illustrations, not a full directional frame-by-frame sprite set.
- A winning and losing strategy is demonstrated; playtime, broader difficulty balance, and user comprehension still need human playtesting.
- The bandit scores current positions and replans each turn; a player move or a blocked side can change its preferred goal.
- Portfolio publication and CV changes were postponed at the user's request.

Full acceptance mapping, fixed defects, and reproduction commands: [QA report](qa-report.md).
