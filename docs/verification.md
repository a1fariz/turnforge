# Verification — Turnforge v0.4.0

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

## Render and reset smoke check

The actual raylib application ran with a hidden Windows window and exercised title, gameplay, enemy turn, victory, defeat, ten restarts, and Guard. Ten screenshots include movement, attack, active protection, and a reduced-damage hit, exported at the actual 1280x720 framebuffer size. Guard views were visually inspected for clipping, readable status, the shield, and the correct -1 damage indicator.

The v0.4 smoke run passed in **11.219 seconds / 360 frames**. Mean frame time was 17.115 ms, p95 18.127 ms, and maximum timed enemy decision 0.050 ms. Sampled peak working set was 141.53 MB and peak private memory 63.90 MB. These short-run measurements are distinct from the sustained benchmark below.

Character self-checks passed: three usable alpha-bounded atlas cells; a route around a wall reaching its destination; attack/hit timers; actual reduced damage after Guard expires; death fade; restart cleanup; Escape-to-title during movement. The scripted Guard sequence verifies both enemies deal 1 damage each and protection expires on the next player turn.

The smoke check must finish idle. Timed benchmarks may stop during a valid animation; the report records that separately as presentation_active_at_end. This preserves the corrected v0.3 verifier criterion without weakening gameplay, character, result, or reset checks.

Raw reports: [smoke-verification.json](smoke-verification.json), [smoke-memory.json](smoke-memory.json).

## Sustained benchmark

The v0.4 hidden-window run completed **180.012 seconds / 9,662 rendered frames**, without a crash. The initial 360-frame capture sequence is excluded from sustained frame-time samples. Approximate FPS below is calculated from mean sampled frame time, not total frames including capture overhead.

| Measurement | Observed |
|---|---|
| Mean frame time | 17.520 ms (approximately 57.1 FPS) |
| p95 frame time | 19.326 ms |
| Maximum timed enemy decision | 1.0025 ms |
| Peak process working set | 225.69 MB |
| Peak private memory | 145.50 MB |
| Memory samples | 359, sampled every 0.5 seconds |
| Character checks, victory/defeat, ten resets, Guard | Passed |
| Presentation active at timed endpoint | No |

The 60 FPS target was not fully reached in this run. AI time and sampled memory remained below the initial 50 ms and 256 MB targets. Memory fluctuated rather than increasing continuously: sampled private memory was about 145.5 MB near 122 seconds, 78.7 MB near 152 seconds, and 74.8 MB at the final sample. This does not prove absence of leaks. The benchmark cycles matches with both enemy roles, character effects, and Guard every fourth automated player action.

Raw reports: [benchmark-verification.json](benchmark-verification.json), [benchmark-memory.json](benchmark-memory.json).

## Package check

**Passed.** The installed v0.4 executable and bundled atlas ran from dist/Turnforge with PATH restricted to Windows and System32. Ten-view rendering, character checks, Guard, victory/defeat, and ten restarts passed with exit code 0; the scripted run ended idle. Executable and atlas SHA-256 hashes matched the verified build/assets. The PE imports contain only the system libraries listed above.

Raw report: [package-verification.json](package-verification.json). The package smoke run completed in 13.099 seconds; its timing is separate from the sustained benchmark.

## Practical limits

- Automated render checks drive the gameplay model and verify pixel-coordinate boundaries. Physical mouse/keyboard interaction has not been manually playtested.
- Hidden-window performance can differ from a visible, focused game window and from other system power/thermal conditions.
- Ten resets verify clean state and successful rendering; sampled process memory alone is not a proof that all possible leaks are absent.
- The game is one fixed arena with two melee enemies. There is no audio, persistent saving, networking, or recorded portfolio video.
- Character animation uses code-driven transforms/effects on three single-pose illustrations, not a full directional frame-by-frame sprite set.
- A winning and losing strategy is demonstrated; playtime, broader difficulty balance, and user comprehension still need human playtesting.
- The bandit scores current positions and replans each turn; a player move or a blocked side can change its preferred goal.
- Portfolio publication and CV changes were postponed at the user's request.
