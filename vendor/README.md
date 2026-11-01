# Vendored libraries

These single-header libraries are checked in so the player builds without any
downloaded dependency. They are compiled in exactly one translation unit,
`src/vendor_impl.c` (miniaudio is compiled by `src/audio/audio_miniaudio.c`,
which is its only consumer).

| Header | Version | License | Upstream |
| --- | --- | --- | --- |
| `miniaudio.h` | 0.11.26 | MIT-0 or public domain | <https://github.com/mackron/miniaudio> |
| `dr_wav.h` | 0.14.6 | Public domain (Unlicense) or MIT-0 | <https://github.com/mackron/dr_libs> |
| `dr_mp3.h` | 0.7.4 | Public domain (Unlicense) or MIT-0 | <https://github.com/mackron/dr_libs> |
| `dr_flac.h` | 0.13.4 | Public domain (Unlicense) or MIT-0 | <https://github.com/mackron/dr_libs> |
| `stb_vorbis.c` | 1.22 | Public domain (Unlicense) or MIT | <https://github.com/nothings/stb> |
| `stb_image.h` | 2.30 | MIT or public domain | <https://github.com/nothings/stb> |

## What each one does

- **miniaudio** — audio device I/O and enumeration. Gives the player WASAPI
  (Windows), ALSA/PulseAudio (Linux), CoreAudio (macOS) and a silent null
  device from a single code path, plus the lock-free ring buffer used between
  the playback thread and the device callback.
- **dr_wav / dr_mp3 / dr_flac** — streaming decoders producing interleaved
  signed 16-bit PCM. They are the fastest widely used single-file decoders for
  these formats and share one API shape, which keeps `src/media.c` small.
- **stb_vorbis** — Ogg Vorbis decoding. The file is read into memory because
  stb_vorbis has no portable streaming file abstraction; the player opens one
  track at a time.
- **stb_image** — cover art (PNG/JPEG/BMP/GIF).

## Updating

```bash
curl -L -o vendor/miniaudio.h  https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
curl -L -o vendor/dr_wav.h     https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h
curl -L -o vendor/dr_mp3.h     https://raw.githubusercontent.com/mackron/dr_libs/master/dr_mp3.h
curl -L -o vendor/dr_flac.h    https://raw.githubusercontent.com/mackron/dr_libs/master/dr_flac.h
curl -L -o vendor/stb_vorbis.c https://raw.githubusercontent.com/nothings/stb/master/stb_vorbis.c
curl -L -o vendor/stb_image.h  https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
```

Then rebuild and run the tests. Third-party warnings are silenced with scoped
`#pragma GCC diagnostic` blocks in `vendor_impl.c` and `audio_miniaudio.c`;
keep those in place.

`stb_vorbis` leaks an internal macro called `L`, which collides with a local
variable in `stb_image`; `vendor_impl.c` undefines it after including
stb_vorbis. Keep that `#undef` when updating.

`stb_image` is configured with `STBI_NO_STDIO` (files are read by `smp/fs.h`,
which handles UTF-8 paths on Windows) and only the PNG, JPEG, BMP and GIF
decoders are compiled in.