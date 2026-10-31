# Vendored libraries

These single-header libraries are checked in so the player builds without any
downloaded dependency. They are compiled in exactly one translation unit,
`src/vendor_impl.c`.

| Header | Version | License | Upstream |
| --- | --- | --- | --- |
| `dr_wav.h` | 0.14.6 | Public domain (Unlicense) or MIT-0, at your choice | <https://github.com/mackron/dr_libs> |
| `dr_mp3.h` | 0.7.4 | Public domain (Unlicense) or MIT-0, at your choice | <https://github.com/mackron/dr_libs> |
| `stb_image.h` | 2.30 | MIT or public domain, at your choice | <https://github.com/nothings/stb> |

## Updating

```bash
curl -L -o vendor/dr_wav.h https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h
curl -L -o vendor/dr_mp3.h https://raw.githubusercontent.com/mackron/dr_libs/master/dr_mp3.h
curl -L -o vendor/stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
```

Then rebuild and run the tests; `vendor_impl.c` silences third-party warnings
with a scoped `#pragma GCC diagnostic` block, so keep it in place.

`stb_image` is configured with `STBI_NO_STDIO` (files are read by `smp/fs.h`,
which handles UTF-8 paths on Windows) and only the PNG, JPEG, BMP and GIF
decoders are compiled in.
