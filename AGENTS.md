# Agent instructions for Qwenvoid_RECODE

This directory is the isolated working copy for the prop-pickup regression bisect.

## Work location and baseline

- Make all new source, build, test, and documentation changes for the recode in this directory.
- The starting point is public upstream tag `v0.9.0n-b292-dev`, commit `ca8537cb902bc4ed4fc5f276cb4a4b693b69909b`, on branch `recode/b301-prop-pickup-bisect`.
- Do not base recode changes on b329 or copy the modified project source wholesale into this checkout.
- Keep the parent `qwenvoid_patch` repository and its b301–b329 reports/builds intact. Consult them as historical evidence; do not edit them as part of the recode.

## Bisect discipline

- Compare upstream to the earliest failing build, b301, before choosing code to reintroduce. Identify the source or artifact evidence for each change group and record unknowns.
- Reintroduce one logical group at a time, cumulatively from this clean upstream branch: prop physics; mirror creation; input interception; synchronization. Refine a group if it contains multiple independent changes.
- Build and test after each group. Do not proceed to the next group until the user has reported the two-instance client pickup result for that candidate. Mark source/build checks separately from in-game observations.
- If a group breaks pickup, revert that group and confirm the upstream-based candidate works again before narrowing the group further.
- Keep E tap pickup, intentional hold-E context actions, host-side pickup, and R-held luggage as separate checks.
- Do not claim a fix from compilation, static tests, logs, or package verification. The user’s in-game result is the acceptance evidence.
- Do not add diagnostics or alter interpolation parameters as a substitute for the bisect.

## Documentation

- Preserve this checkout's upstream README, START_HERE, feature docs, and build docs as the baseline documentation.
- Record recode decisions and build-specific findings in `RECODE_DECISION.md` and a dedicated test ledger; do not overwrite upstream documentation with old b329 status.
- The original project documentation outside this folder remains useful historical evidence and must stay intact. Label any reused claim by its build and evidence type.
