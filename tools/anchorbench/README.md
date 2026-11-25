# Anchor bench: pair-aware anchors against today's, by ear

Prototype for docs/research/auto-anchors.md (1.3.8 plan, Chris 2026-11-16). Plain Python 3,
no packages; ffmpeg renders the audio. Nothing in the app changes: this decides whether the
pair-aware rule is worth porting to C++ (MixHouseBeatGrids) for 1.3.9.

## Run

```
cp ~/Library/Application\ Support/Glidemix/Library/library.sqlite /tmp/lib-copy.sqlite
python3 tools/anchorbench/anchorbench.py --library /tmp/lib-copy.sqlite pairs --count 20 --seed 1 --out OUT
```

- Always a COPY of the library (opened read-only anyway); the analysis files are read in place.
- `--ids 1,2,3` scores a chain of library ids instead of random pairs; `--no-audio` skips the renders;
  `--target 16` lowers the longest blend; `--match-keys` lets clashing keys have long blends.
- Random pairs: songs under `--folder` (default `~/Desktop/`), tempos within 6 % and compatible keys
  (what a DJ would pick).
- Song names are private: everything is by library id; the audio files are `pairNN-X.wav` / `-Y.wav`.

## What it compares

GlideMix plays a transition MixMeister's way: A's OUT and B's IN sit at the same moment, B is silent
before its IN, both play together for the blend, A falls 0 -> -6 dB then is cut, B rises -6 -> 0 dB.

- **Today**: the app's default anchors (`MixHouseBeatGrids::sourceAnchors`, ported) and the default
  Volume Fade, 8 bars. Saved (moved) library anchors win, as in the app. Difference: the app uses
  rekordbox's phrases when rekordbox analysed the song; this port always uses GlideMix's own phrases.
- **New**: every 8-bar phrase line in A's last 35 % against every phrase line in B's first 30 %, blends of
  32 / 16 / 8 bars (8 only when the keys clash), scored:
  structure (A's Outro / breakdown under B's Intro best), B arriving on a strong phrase (Chorus / Up,
  louder, bass coming in), minus vocals together (phrase labels: Verse / Chorus = vocals), minus both
  basslines (low band) together, minus holes (both quiet), minus an energy jump, plus length near the
  target, minus skipping more than 20 % of the two songs. Weights: `WEIGHTS` at the top.
  The main audible change: B's IN moves to the start of its intro, so B's intro plays over A's outro and
  B "arrives" when A is cut (today B enters after its intro, its main part over A's outro).

## Renders

ffmpeg: A from 8 bars before its OUT, B tempo-matched to A with `atempo` (not Rubber Band, so the
stretch sounds a little worse than in the app -- the same for both versions), the Volume Fade above,
-3 dB and a limiter. Both renders of a pair have the same length, so the length gives nothing away.
Which file is new is random per pair and written only to `.answers.json` (hidden).

## First run (2026-11-16, seed 1, 20 pairs from the Desktop test music)

Bars with both basslines: today 53, new 17. Bars of vocals together: today 2, new 0. Quiet bars:
today 0, new 2. New blend lengths: 16 bars x8, 8 bars x12 -- none reached 32: these songs' intros and
outros are shorter than DJ tools'. Ship only if Chris's ratings favour the new versions clearly
(auto-anchors.md section 7).

## Rating without ears (`rate`)

`anchorbench.py --library COPY rate --dir OUT` renders each song's part of both versions on its own and
measures the DJ rules on the audio: bars where both basslines play (low band), quiet bars (holes), the
level jump when A is cut, plus phrase lines and blend length; 1-5 per version, `machine-ratings.csv/.json`.
Noted, not scored: the beat offset between the two songs' kicks (it tests the grid this renderer uses --
GlideMix's stored bar 1 and BPM -- not the anchors; the app lines songs up on its own grid and warp) and
"both full in the mids" (no way to tell vocals from a mix without stems).

First rating (2026-11-16, the 20 pairs above): today 3.20, new 3.60; new better in 13, today in 4, equal
in 3. Where new lost: A is cut while B is still in a quiet intro (level drops 9-20 dB at the cut) -- the
blend should end where B arrives. Many renders flag kicks 30-60 ms apart in BOTH versions: a renderer grid
issue, so listen past it.

Run 2 (same pairs, same "today"): the scorer now wants B near full level when A is cut (a filter
first, with a fallback), and penalises a level drop at the cut. 5 pairs changed. Rating: today 3.23,
new 3.83; new better in 13, today in 2, equal in 5 ("today" moved a little: the render window follows
the longer blend). Trade-off to listen for: by the phrase labels the new picks now have 25 bars of vocals
together (run 1: 0) -- B arrives on a verse/chorus more often; the audio can't confirm vocals without stems.
Listening folder: ~/Desktop/GlideMix anchor A-B 2.

## Chris's ears, run 2 (2026-11-16) -- the verdict

Blind, 20 pairs: today 3.65, new 3.60 -- no win. New better in 3, today in 4, equal in 13.
By blend length: new 8-bar blends won 3 (pairs 03, 07, 16), lost 1 (17); new 16/32-bar blends won 0,
lost 3 (01, 02, 19), tied 5. So the new PLACEMENT helps at 8 bars; the long MixMeister volume-only blend
(both songs near full for 16+ bars) sounds worse by ear. The machine rating did not predict his ears
(it picked his winner in 3 of the 7 pairs he split): don't trust `rate` as a stand-in for listening.
Next idea: run 3 = new placement, 8-bar blends only (or long blends only with a bass swap / EQ).

## Chris's ears, run 3 (8-bar blends only)

Today 4.80, new 4.90; new better in 2, today in 0, equal in 18. Never worse, not a clear win -- and the same
"today" anchors scored 3.65 in run 2, so the noise between sessions is larger than this gain.

## Not covered yet

Warped (drifting) songs are rendered on a constant grid; stems (real vocals) are not read; no
double drops; EDM-CUE comparison; Chris's 24 moved anchors as a sanity check.
