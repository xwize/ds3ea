# DS3 Enhancement Add-on (DS3EA)

A [ReShade](https://reshade.me) add-on for Dark Souls 3 that improves the game and gives other modders a foundation
to build on. Coding rules live in [GUIDELINES.md](GUIDELINES.md).

## Prerequisites

- Windows 10/11 x64
- Visual Studio 2026 (MSVC 14.5x, the "Desktop development with C++" workload). It also provides Ninja.
- CMake 3.25 or newer (on PATH)
- Conan 2 (on PATH: `pip install -U conan`), recent enough to know MSVC 14.5 (`msvc 195`). 2.33 is tested; 2.19
  is too old. Run `conan profile detect` once if you have no default profile.

You don't need to run `conan install` yourself. CMake runs it during configure through
[cmake-conan](https://github.com/conan-io/cmake-conan) (`cmake/conan_provider.cmake`). It builds `fmt`, `gtest` and
`inipp` with the static CRT and C++17 to match the project.

### ReShade headers

The add-on API is header-only. `cmake/reshade.cmake` finds the headers in one of two ways:

- **Default:** downloads the pinned ReShade commit (hash-checked, about 3 MB, once per build tree). A fresh clone or
  worktree configures with no setup.
- **Local checkout:** set `DS3EA_RESHADE_DIR` to the **absolute** path of a ReShade source tree, e.g.
  `S:\Internal\res_DS3EA_\ds3ea\repos\reshade`. A relative path resolves against wherever CMake runs from, which
  varies between IDEs. You can set it as an environment variable (the presets forward it) or pass
  `-DDS3EA_RESHADE_DIR=...`.

To bump ReShade, update `DS3EA_RESHADE_COMMIT` and `DS3EA_RESHADE_SHA256` together.

## Building

### Command line

The `vs` preset uses the Visual Studio generator, so any shell works (no developer prompt needed):

```sh
cmake --preset vs
cmake --build --preset vs-release    # or vs-debug
ctest --preset vs-release            # or vs-debug
```

The add-on is written to `build/vs/<Config>/ds3ea.addon64`.

### IDE

The `CMakePresets.json` presets work in any IDE that supports CMake presets:

- **Visual Studio:** *File > Open > Folder*, then pick `Ninja Debug` / `Ninja Release` (or `Visual Studio 2026`) in the
  configuration dropdown.
- **CLion:** *Settings > Build, Execution, Deployment > CMake* and enable the presets. Use the Visual Studio toolchain
  (x64) with the `ninja-*` presets.
- **VS Code:** install the CMake Tools extension and pick a configure preset. Use the `ninja-*` presets from a
  developer environment, or use `vs`.

The `ninja-debug` / `ninja-release` presets need the MSVC environment, which the IDEs above set up for you. On the
command line, run them from a *x64 Native Tools* prompt. Their output goes to `build/ninja-<config>/ds3ea.addon64`.

## Installing

1. Install ReShade **with full add-on support** (the "ReShade with full add-on support" installer from reshade.me)
   for `DarkSoulsIII.exe` with the DirectX 10/11/12 API.
2. Copy `ds3ea.addon64` next to `DarkSoulsIII.exe` (the `Game` folder), or into the folder set as
   *Add-on search path* in ReShade's settings.
3. Start the game. **DS3EA** shows up in the overlay's *Add-ons* tab, and `ReShade.log` has a line like
   `DS3EA hello world (device api 0xb000)`.

Settings live in `ds3ea.ini` next to `ds3ea.addon64`. The file is optional: missing settings use their defaults.
