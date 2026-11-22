# Recode test ledger

Append one entry per cumulative group candidate. Record source commit and exact group delta, build/package checks, then wait for the user’s two-instance result before adding the next group.

| Candidate | Base + group | Client tap-E | Intentional hold-E action | Host control | R luggage | User result / date |
|---|---|---|---|---|---|---|
| Public baseline | `v0.9.0n-b292-dev` (`ca8537c`) | User reports the locally built baseline does not contain the pickup bug | Not separately recorded here | Not separately recorded here | Not separately recorded here | User confirmed the clean upstream baseline is a valid control on 2026-11-22 |
| b301 | Source snapshot not recovered | User reports fails | Build-specific result not recorded here | User says symptom follows client role | Not stated in this report | Current parent-project report; see `../multivoid/Multivoid-main/Multivoid-main/handoff.md` |
| b329 | Failed previous candidate; not a recode base | User reports no fix | Not separately reported for b329 | Not separately tested here | Earlier client R-luggage failure reported on b328; b329 R result not supplied | Do not build on this candidate |
| Recode upstream baseline | `v0.9.0n-b292-dev` plus documentation-only commit `5686bc8` | User reports no pickup bug in the locally built baseline | Not separately recorded | Not separately recorded | Not separately recorded | Windows Release build, ABI gate, and package verification passed in [CI run 45029232110](https://github.com/boiling-tea-cup-26/Multivoid/actions/runs/45029232110); package is in `~/Desktop/readytotest/recode/`; no gameplay changes in this candidate |
| Recode event candidate | `v0.9.0n-b292-dev` plus anti-gravity camera-light cue only; protocol b293 | Repeat as a control | Not separately requested | Repeat as a control | Out of this event check | Build/runtime results pending; test the light edge independently from prop gravity and the ship timeline |
