# Architecture

## Goals

1. **Correctness first.** No undefined behaviour, no data races, no unbounded
   `strcpy`/`sprintf`, no shell-outs.
2. **Small platform layer.** Everything that is not POSIX/Win32 lives in
   portable code that can be unit tested without a sound card or a terminal.
3. **One owner per resource.** A file is opened by one object (the decoder), a
   frame is drawn by one thread (the UI), and the player is driven by one
   worker thread.

## Module graph

```
                    в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ
                    в”‚  main.c   в”‚  argument parsing, event loop
                    в””в”Ђв”Ђв”Ђв”Ђв”Ђв”¬в”Ђв”Ђв”Ђв”Ђв”Ђв”
          в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”јв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ
          в”‚               в”‚                в”‚
    в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв–јв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ   в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв–јв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ    в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв–јв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ
    в”‚    ui.c   в”‚   в”‚ player.c  в”‚    в”‚  term.c   в”‚
    в”‚ layout,   в”‚   в”‚ state +   в”‚    в”‚ raw mode, в”‚
    в”‚ painting  в”‚   в”‚ worker    в”‚    в”‚ input     в”‚
    в””в”Ђв”Ђв”Ђв”Ђв”Ђв”¬в”Ђв”Ђв”Ђв”Ђв”Ђв”   в””в”Ђв”Ђв”Ђв”Ђв”Ђв”¬в”Ђв”Ђв”Ђв”Ђв”Ђв”    в””в”Ђв”Ђв”Ђв”Ђв”Ђв”¬в”Ђв”Ђв”Ђв”Ђв”Ђв”
          в”‚               в”‚                в”‚
    в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв–јв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ   в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв–јв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ    в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв–јв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ
    в”‚ frame.c   в”‚   в”‚  pcm.c    в”‚    в”‚ thread.c  в”‚
    в”‚ cells в†’   в”‚   в”‚ channels, в”‚    в”‚ mutex,    в”‚
    в”‚ ANSI      в”‚   в”‚ resample  в”‚    в”‚ cond,     в”‚
    в””в”Ђв”Ђв”Ђв”Ђв”Ђв”¬в”Ђв”Ђв”Ђв”Ђв”Ђв”   в””в”Ђв”Ђв”Ђв”Ђв”Ђв”¬в”Ђв”Ђв”Ђв”Ђв”Ђв”    в”‚ join      в”‚
          в”‚               в”‚          в””в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”
    в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв–јв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ   в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв–јв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ
    в”‚ cover.c   в”‚   в”‚ media.c   в”‚
    в”‚ stb_image в”‚   в”‚ dr_wav /  в”‚
    в”‚ + box     в”‚   в”‚ dr_mp3    в”‚
    в””в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”   в””в”Ђв”Ђв”Ђв”Ђв”Ђв”¬в”Ђв”Ђв”Ђв”Ђв”Ђв”
                          в”‚
                 в”Њв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв–јв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”ђ
                 в”‚ playlist.c      в”‚  scan, tags, natural sort
                 в”‚ tags.c, fs.c    в”‚
                 в””в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”
```

`audio/audio_miniaudio.c` is the only audio backend: miniaudio picks WASAPI,
ALSA, PulseAudio or CoreAudio at run time, so there is no per-platform audio
code to maintain. `plat/term_posix.c`/`plat/term_win.c` and
`plat/thread_posix.c`/`plat/thread_win.c` are the only files with `#ifdef` in
them.

## Threading model

There are exactly two threads.

**UI thread** вЂ” owns the `SmpFrame`, polls the terminal and handles keys.
It never touches the decoder.

**Worker thread** вЂ” owns the `SmpMedia` decoder, the `PcmConverter` and the
audio device. It is created once and joined once.

All shared state lives in `SmpPlayer` behind one mutex:

```
UI thread                            worker thread
в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ                            в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
play(index)                          wait for state == PLAYING
  lock: index, generation++,  в”Ђв”Ђв”Ђв”Ђв”Ђв–є
        state = PLAYING, signal
                                     open decoder, publish duration
snapshot()                           loop:
  lock: copy state, position    в—„в”Ђв”Ђв”Ђв”Ђ   lock: check quit/generation/seek
        unlock                          read + convert + write (no lock)
                                        lock: position = media position
```

Rules that keep it simple:

- The lock is never held across a blocking call (I/O, `smp_audio_write`,
  condition wait is the only exception and it releases the lock itself).
- A track change is a **generation counter bump**. The worker notices that its
  generation is stale, closes the decoder and loops. No thread is ever killed
  or detached.
- A seek is expressed in **seconds**, not frames, so it also works before the
  decoder exists (pause, then seek, then resume).
- The UI never reads `SmpPlayer` fields directly; it calls
  `smp_player_snapshot()`, which copies a consistent `SmpPlayerSnapshot`.

## Audio pipeline

```
file в”Ђв”Ђв–є dr_wav / dr_mp3 в”Ђв”Ђв–є int16 interleaved, native rate/channels
                                    в”‚
                              pcm_converter
                        channel map + linear resample
                                    в”‚
                             int16 stereo 48 kHz
                                    в”‚
                             pcm_apply_gain
                                    в”‚
                        ALSA / WASAPI / null backend
```

The device always runs at 48 kHz stereo S16, which keeps the device
configuration code trivial and makes the converter testable in isolation. Mono
is duplicated, more than two channels are folded left/right, and the resampler
keeps its fractional position across calls so there is no discontinuity at
block boundaries (a unit test feeds 43 blocks and checks continuity).

## Rendering

`SmpFrame` is a grid of cells (`code point + fg + bg + attributes`). Drawing
is a pure function of the model; there is no cursor state to get wrong and
clipping is enforced per cell.

`smp_frame_render()` walks the grid and emits ANSI sequences into one `StrBuf`,
which is written with a single `write()`/`WriteConsoleW()` call. That removes
flicker, keeps a redraw at ~12 fps cheap, and makes it impossible to leave a
half-updated screen.

Wide characters occupy two cells; the second is marked as a continuation and
is skipped by the renderer. Cover art uses the upper-half block `в–Ђ` with the
top pixel as foreground and the bottom pixel as background, so one cell shows
two vertical pixels.

## Error handling

Every fallible function returns `SmpStatus`; allocations return `NULL`.
Owners clean up with a single `goto cleanup` path, and nothing in the library
calls `exit()`. Terminal state is restored in `smp_term_shutdown()`, which is
called unconditionally at process exit.

## Testing

Tests are plain C with a 60-line harness (`tests/test.h`), built as one
executable and run by CTest. They need neither a sound card nor a TTY: the
`null` audio backend and generated WAV/BMP fixtures keep the suite hermetic.
Coverage per module:

| Suite | What it checks |
| --- | --- |
| `common` | status strings, monotonic clock |
| `str` | string builder, UTF-8 decode/width/truncate, natural sort |
| `fs` | path joining, extension handling, file reads, directory iteration |
| `pcm` | channel mapping, resampling length and continuity, gain |
| `tags` | ID3v2.2/2.3/2.4, ID3v1 (CP1251), RIFF INFO, Vorbis comments (FLAC/Ogg), file fallback |
| `media` | WAV round trip, MP3 duration from Xing/VBRI/CBR, FLAC and Ogg decoding |
| `playlist` | scanning, natural order, navigation, missing directories |
| `frame` | clipping, wide characters, colours, resize |
| `cover` | BMP decoding, box scaling, cover discovery |
| `player` | state machine, end of track, navigation, seeking |

## Porting notes

Adding a platform means implementing two small interfaces:

- `term.h` вЂ” raw mode, size, key polling, output.
- `audio.h` (through `audio_internal.h`) вЂ” open/write/drain/close.

The rest of the tree builds unchanged. `thread.h` already has pthreads and
Win32 implementations; C11 `<threads.h>` could replace both if the compiler
supports it everywhere.
