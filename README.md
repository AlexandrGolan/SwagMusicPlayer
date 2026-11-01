# SMP вЂ” Swag Music Player

<div align="center">

**A small, fast terminal music player for WAV, MP3, FLAC and Ogg Vorbis.**

Linux (ALSA) В· Windows (WASAPI) В· no shell-outs В· no external runtime dependencies

[![CI](https://github.com/AlexandrGolan/SwagMusicPlayer/actions/workflows/ci.yml/badge.svg)](https://github.com/AlexandrGolan/SwagMusicPlayer/actions/workflows/ci.yml)
[![License](https://img.shields.io/github/license/AlexandrGolan/SwagMusicPlayer?style=flat-square)](LICENSE)
[![Top language](https://img.shields.io/github/languages/top/AlexandrGolan/SwagMusicPlayer?style=flat-square)](https://github.com/AlexandrGolan/SwagMusicPlayer)

</div>

SMP shows your album art, the current track and the playlist in one terminal
window, plays WAV and MP3 files, and stays out of the way.

![SMP running in Windows Terminal](docs/screenshot.png)

```
 SMP Swag Music Player                         vol  70% в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€в–‘в–‘в–‘в–‘в–‘
 в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
                     в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ
      в–„в–„в–„в–„в–„в–„в–„в–„в–„      в”‚ selected                     в”‚
     в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€     в”‚ Duvet                        в”‚
    в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€    в”‚ bГґa                          в”‚
     в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€     в”‚ Twilight                     в”‚
      в–Ђв–Ђв–Ђв–Ђв–Ђв–Ђв–Ђв–Ђв–Ђ      в”‚ [в–€в–€в–€в–€в–€в–€в–€в–€в–€в–€в–‘в–‘в–‘в–‘в–‘в–‘в–‘в–‘в–‘в–‘] 1:02 / 3:45 в”‚
                     в”њв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”¤
                     в”‚ playlist  2 tracks           в”‚
                     в”‚  1. Duvet            3:45  в–¶ в”‚
                     в”‚  2. Twilight         4:10    в”‚
 в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
 stopped  track 1/2  volume 70%  audio alsa
 в†‘в†“ select  вЏЋ play  space pause  в†ђв†’ seek  +/- vol  n/p  ? help  q quit
```

## Features

- Plays **WAV** (PCM, float, A-Law, Вµ-Law, MS ADPCM, IMA ADPCM) and **MP3**
- Album art rendered with true-colour half blocks вЂ” works in any modern
  terminal, no `tiv`/`chafa`/ImageMagick required
- Correct playback of any sample rate and channel count: everything is
  resampled to 48 kHz stereo on the fly
- Reading of **ID3v2.2/v2.3/v2.4**, **ID3v1** and **RIFF INFO** tags,
  including UTF-16 and CP1251 text
- Seeking, pause/resume, volume control, natural track ordering
- UTF-8 aware layout (Cyrillic, CJK, emoji)
- No flicker: the whole screen is composed in memory and written in one go
- Portable 64-bit C17 core, unit tested; the platform layer is tiny and isolated
- Audio output through miniaudio: WASAPI, ALSA, PulseAudio and CoreAudio behind one API

## Requirements

| Platform | Requirements |
| --- | --- |
| Linux | `cmake` в‰Ґ 3.20, a C17 compiler, `pkg-config`, ALSA headers (`libasound2-dev` on Debian/Ubuntu, `alsa-lib` on Fedora/Arch) |
| Windows | CMake в‰Ґ 3.20, MinGW-w64 (MSYS2) or MSVC 2019+ |

Everything else is vendored: miniaudio, `dr_wav`, `dr_mp3`, `dr_flac`,
`stb_vorbis` and `stb_image` live in
[`vendor/`](vendor/README.md).

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build          # optional, installs /usr/local/bin/smp
```

Presets are provided for the common cases:

```bash
cmake --preset default && cmake --build --preset default
cmake --preset asan    && cmake --build --preset asan && ctest --preset asan
```

On Windows with MSYS2:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --preset windows-mingw
```

### Nix

```bash
nix develop          # dev shell with everything needed
nix build            # builds ./result/bin/smp
```

## Usage

```bash
smp ~/Music/Twilight            # play a directory
smp -d ~/Music/Twilight         # same thing
smp --audio null ~/Music        # silent backend, useful for testing
smp --list-devices              # show output devices
smp --help
```

Put the audio files and a cover image into one directory. The cover is looked
up as `cover.*`, `album.*`, `folder.*`, `front.*` or `art.*`
(`png`, `jpg`, `jpeg`, `bmp`, `gif`).

```
Twilight/
в”њв”Ђв”Ђ 01 - Duvet.mp3
в”њв”Ђв”Ђ 02 - Twilight.mp3
в””в”Ђв”Ђ cover.png
```

### Keyboard

| Key | Action |
| --- | --- |
| `в†‘` / `в†“` | move the selection |
| `Enter` | play the selected track |
| `Space` | play / pause |
| `в†ђ` / `в†’` | seek 5 seconds |
| `n` / `p` | next / previous track |
| `+` / `-`, `PgUp` / `PgDn` | volume |
| `r` | reload the library |
| `h`, `?` | help |
| `q`, `Esc` | quit |

## Project layout

```
include/smp/     public headers (one per module)
src/             portable implementation
src/plat/        terminal and thread backends (POSIX / Win32)
src/audio/       audio backends (ALSA / WASAPI / null)
vendor/          single-header third party libraries
tests/           dependency-free unit tests, run by CTest
docs/            architecture notes and the audit of the 1.x code
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the threading model and
the module dependency graph, and [docs/AUDIT.md](docs/AUDIT.md) for the list of
defects that were fixed during the rewrite.

## Testing

```bash
ctest --test-dir build --output-on-failure
```

The suite covers UTF-8 and layout helpers, path handling, the PCM
converter/resampler, ID3/RIFF/Vorbis-comment parsing, WAV/FLAC/Ogg decoding,
playlist scanning, the render frame and the playback state machine. It runs
without an audio device or a TTY; the FLAC and Ogg fixtures live in
`tests/data`.

## License

MIT вЂ” see [LICENSE](LICENSE). Vendored libraries keep their own licenses, see
[vendor/README.md](vendor/README.md).
