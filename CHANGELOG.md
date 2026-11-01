# Changelog

All notable changes to this project are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](https://semver.org/).

## [2.0.0] — 2026-11-01

Complete rewrite of the 1.x codebase. The user interface, the keyboard layout
and the "one directory = one album" model are preserved.

### Added

- Windows support through a WASAPI backend; the player now builds and runs on
  Linux and Windows from the same sources.
- Seeking (`←`/`→`, `smp_player_seek`), volume control in software, library
  reload (`r`) and a help overlay (`?`).
- Built-in cover art rendering with true-colour half blocks. No `tiv`, no
  ImageMagick, no subprocesses.
- ID3v2.2/v2.3/v2.4, ID3v1 and RIFF INFO tag reading, including UTF-16 and
  CP1251 text.
- Correct playback of any sample rate and channel count (resampler and channel
  mapper in `src/pcm.c`).
- Unit test suite (11 groups, 5 380 assertions) and a GitHub Actions workflow
  for Linux and Windows.
- CMake presets, `--help`, `--version`,
  `--list-devices`, `--audio <backend>`.
- Documentation: `docs/ARCHITECTURE.md`.

### Changed

- C99 → C17; fixed-size buffers replaced with owned strings.
- One persistent worker thread with a mutex/condition variable instead of
  ad-hoc thread creation on every track change.
- The screen is composed as a cell grid and written in one pass instead of
  `printf` per character with a full clear on every frame.
- Build system: hand-written `Makefile` → CMake with `pkg-config`, install
  rules and sanitizer presets.
- WAV and MP3 decoding moved to the vendored `dr_wav`/`dr_mp3`, dropping the
  `libsndfile`, `libmpg123` and `taglib` dependencies.

### Fixed

- Out-of-bounds read when advancing past the last track.
- Stack corruption from negative indices in the title/playlist truncation code.
- Shell command injection through cover file names (`popen("tiv \"%s\"")`).
- Interface freeze after pressing `Esc`.
- Thread leak and concurrent writes to one PCM handle on track changes.
- Data races on the playlist and playback flags.
- Position derived from the wall clock, which made the timer jump after pause.
- Mono WAV files not playing and non-44.1 kHz files playing at the wrong speed.
- Terminal left in raw mode (no echo, hidden cursor) after `Ctrl+C` or errors.
- Resource leaks on every early-exit path.
- `system("which tiv")` being executed on every redraw.

### Removed

- The `tiv` dependency and all `system()`/`popen()` calls.
- `Makefile` and `shell.nix` (replaced by CMake).
- Dead declarations (`count_visible_chars`) and duplicated globals.

## [1.0.0] — 2025-11-30

- Initial release: WAV/MP3 playback, playlist, cover art through `tiv`.
