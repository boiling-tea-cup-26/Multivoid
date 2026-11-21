# Recode test ledger

No recode candidates have been built or tested yet. Append one entry per cumulative group candidate. Record source commit and exact group delta, build/package checks, then wait for the user’s two-instance result before adding the next group.

| Candidate | Base + group | Client tap-E | Intentional hold-E action | Host control | R luggage | User result / date |
|---|---|---|---|---|---|---|
| Public baseline | `v0.9.0n-b292-dev` (`ca8537c`) | User reports works | Not separately recorded here | Not separately recorded here | Not separately recorded here | Existing public build was user-verified working; fresh baseline run not requested yet |
| b301 | Source snapshot not recovered | User reports fails | Build-specific result not recorded here | User says symptom follows client role | Not stated in this report | Current parent-project report; see `../multivoid/Multivoid-main/Multivoid-main/handoff.md` |
| b329 | Failed previous candidate; not a recode base | User reports no fix | Not separately reported for b329 | Not separately tested here | Earlier client R-luggage failure reported on b328; b329 R result not supplied | Do not build on this candidate |
