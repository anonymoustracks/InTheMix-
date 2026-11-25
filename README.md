# InTheMix by Anonymous Tracks

**InTheMix** is a Windows app for building DJ mixes on your own computer, with your own music:
drop your songs on the timeline and they are beat-matched and locked to the grid automatically,
then export the finished mix as WAV or MP3. It follows the simple workflow of MixMeister Fusion.

- Automatic BPM, beat-grid, key and phrase analysis; songs snap to the beat
- Smooth tempo glides between songs with different BPMs
- Volume, Treble, Midrange, Bass and Filter markers on each song
- DJM-style effect markers: Delay, Echo, Ping Pong, Reverb, Flanger, Trans, Roll, Vinyl Brake, Sweep, Noise, Crush
- Loops of 1-16 bars, beat-locked cut / copy / paste
- Harmonic mixing (Camelot keys, Match key / Transpose)
- Export to WAV / MP3 with a true-peak limiter

Ready-to-run installer: [anonymoustracks.com](https://anonymoustracks.com)

## Licence

InTheMix is free software, released under the **GNU General Public License v3** (see [LICENSE.txt](LICENSE.txt)).

It is a fork of [GlideMix](https://github.com/cristitm76/glidemix-releases) 1.3.13 (GPL), which is built on
[Audacity](https://www.audacityteam.org/) 4 (GPL v3), the MuseScore framework (GPL v3) and Mixxx effects (GPL v2+).
"GlideMix" and "Audacity" names and logos are not used by InTheMix. Third-party components and their licences are
listed in [BUILDING-INTHEMIX.md](BUILDING-INTHEMIX.md); the installer ships every licence text in its `licenses` folder.

Not included on purpose: GlideMix's `glidemix_phrases.onnx` model (no licence stated for it); InTheMix falls back
on its built-in phrase detector.

## Building

See [BUILDING-INTHEMIX.md](BUILDING-INTHEMIX.md). No support is given for building it yourself.
