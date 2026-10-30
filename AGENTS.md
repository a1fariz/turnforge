# Turnforge project workflow

Use the installed Ponytail skill at level full for planning and coding when available. Prefer the smallest complete solution and follow the existing C++17/raylib conventions.

## Complete each task

1. Finish one requested task and update its relevant tests and documentation.
2. Run checks appropriate to the change. For gameplay changes, build and run CTest; for presentation changes, also run the render verifier and inspect the affected screenshots. Documentation-only changes need a relevant content/link check.
3. Inspect the diff and staged files. Exclude unrelated work, secrets, generated builds, downloaded dependencies, and release archives.
4. Commit the task with a concise message describing its result, then push that commit to the current tracked branch. Do this after each completed task rather than accumulating unrelated tasks in one commit. Respect an explicit request to defer commits or pushes.
5. Report the result and commit. If authentication, connectivity, or a remote conflict blocks the push, preserve the local commit and explain the blocker. Do not force-push or rewrite published history without explicit authorization.

The initial import is a snapshot of the completed v0.4 game; earlier development stages were not separate Git commits. Subsequent work follows the per-task workflow above.

## Scope and documentation

Keep the game offline, 2D, turn-based, and suitable for the target integrated-GPU laptop. Do not add features or dependencies without a task requiring them.

Maintain the existing 17-section Indonesian PRD format. Documentation diagrams should be readable, formal PNG images embedded in the document. Distinguish measured results from targets and disclose AI-assisted code/artwork accurately.

The owner authorized publishing Turnforge on alfarizi.my.id and making this repository public. Keep credentials and private provider configuration out of source, history, browser assets, and release packages. CV updates remain deferred until explicitly resumed.
