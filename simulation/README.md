# Host Simulation

Host-side simulation runs the same expression logic as on device, rendered through
the `esp_emote_gfx` SDL backend. This directory holds host-only entry points and
is outside the ESP-IDF component build path.

## Layout

| Path | Role |
| --- | --- |
| `host/gfx_host_expression_demo.c` | SDL demo that mounts assets and runs the expression script |
| `../test_apps/spiffs/esp32_s3_assets.bin` | Default asset pack for host runs |

For GFX host simulation details (SDL runner, smoke scripts), see
[`esp_emote_gfx` simulation docs](https://github.com/espressif2022/esp_emote_gfx/blob/feat/sdl_support/simulation/README.md).

## Dependencies

Install CMake, a C compiler, SDL2 or SDL3 development files, and libjpeg
development files.

Ubuntu example:

```bash
sudo apt install build-essential cmake pkg-config libsdl2-dev libjpeg-dev
```

Host CMake fetches third-party sources from GitHub on configure:

| Dependency | Default ref | CMake variable |
| --- | --- | --- |
| `esp_emote_gfx` | `feat/sdl_support` | `ESP_EMOTE_GFX_REF` |
| `laride/heatshrink` | `0.4.1` (commit pin) | — |
| `cJSON` | `v1.7.19` | `CJSON_REF` |

You only need to clone `esp_emote_expression`; first configure downloads gfx,
heatshrink, and cJSON into the build tree.

Optional local gfx override for host development:

```bash
cmake -S . -B build-host-sdl -DESP_EMOTE_GFX_ROOT=/path/to/esp_emote_gfx
```

## Build

From the `esp_emote_expression` repository root:

```bash
cmake -S . -B build-host-sdl -DBUILD_TESTING=ON
cmake --build build-host-sdl --target gfx_host_expression_demo -j
```

Optional version overrides:

```bash
cmake -S . -B build-host-sdl -DESP_EMOTE_GFX_REF=feat/sdl_support -DCJSON_REF=v1.7.19
```

## Run

```bash
./build-host-sdl/gfx_host_expression_demo
```

By default the demo loads `test_apps/spiffs/esp32_s3_assets.bin`. Override with:

```bash
GFX_EXPRESSION_FS_ROOT=/path/to/esp32_s3_assets.bin ./build-host-sdl/gfx_host_expression_demo
```

You can also pass a loose asset directory if files are laid out by name.

The demo cycles through listen / speak / emoji / QR code / battery / custom
widgets. Close the SDL window to exit.

## Smoke Test

```bash
ctest --test-dir build-host-sdl --output-on-failure
```

This runs a headless SDL dummy smoke test on `gfx_host_expression_demo` (no
display required).

Or:

```bash
cmake --build build-host-sdl --target gfx_host_expression_smoke
```
