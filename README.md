# DS3 Enhancement Add-on (DS3EA)

*Note: This project was an experiment in agentic coding workflows and reverse engineering*

A [ReShade](https://reshade.me) add-on for Dark Souls 3 that adds new graphical enhancements and gives other modders a foundation
to build on. Coding rules live in [GUIDELINES.md](GUIDELINES.md).

## Currently supported features: 
* Tonemapper replacement with bloom control.
* Skipping intro screens.
* Borderless window mode.
* Frame rate unlocking & limiting.

## Getting started

1. Install the [prerequisites](#prerequisites).
2. Build and test from any shell:

   ```sh
   cmake --preset vs
   cmake --build --preset vs-debug    # or vs-release
   ctest --preset vs-debug            # or vs-release
   ```

   The add-on is written to `build/vs/<Config>/ds3ea.addon64`.
3. Run it in the game: `python scripts/launch_game.py` (see [Testing in the game](#testing-in-the-game)).

## Prerequisites

- Windows 10/11 x64
- Visual Studio 2026 (MSVC 14.5x, the "Desktop development with C++" workload). It also provides Ninja.
- Visual Studio's "C++ Clang tools for Windows" component, for the clang-tidy that runs on every MSVC build
  (`.clang-tidy`) and for the `clang-*` presets. Configure with `-DDS3EA_CLANG_TIDY=OFF` to build without clang-tidy.
- CMake 3.25 or newer (on PATH)
- Conan 2 (on PATH: `pip install -U conan`), recent enough to know MSVC 14.5 (`msvc 195`). 2.33 is tested; 2.19
  is too old. Run `conan profile detect` once if you have no default profile.
- Python 3, for `scripts/`

You don't need to run `conan install` yourself. CMake runs it during configure through
[cmake-conan](https://github.com/conan-io/cmake-conan) (`cmake/conan_provider.cmake`). It builds the packages in
`conanfile.txt` with the static CRT and C++20 to match the project.

### ReShade headers

The add-on API is header-only, and its overlay API needs the Dear ImGui header ReShade was built with.
`cmake/reshade.cmake` finds both in one of two ways:

- **Default:** downloads the pinned ReShade commit and its pinned ImGui commit (hash-checked, about 5 MB, once per
  build tree). A fresh clone or worktree configures with no setup.
- **Local checkout:** set `DS3EA_RESHADE_DIR` to the **absolute** path of a ReShade source tree with its submodules
  checked out (ImGui is `deps/imgui`), e.g.
  `S:\Internal\res_DS3EA_\ds3ea\repos\reshade`. A relative path resolves against wherever CMake runs from, which
  varies between IDEs. You can set it as an environment variable (the presets forward it) or pass
  `-DDS3EA_RESHADE_DIR=...`.

To bump ReShade, update `DS3EA_RESHADE_COMMIT` and `DS3EA_RESHADE_SHA256` together, and `DS3EA_IMGUI_COMMIT` and
`DS3EA_IMGUI_SHA256` to its `deps/imgui` submodule.

## Building

Every preset builds into `build/<configure preset>/`, treats warnings as errors (`/W4 /WX`) and has a build and a test
preset of the same name (`vs` has `vs-debug` and `vs-release`):

| Configure preset | Compiler, generator | Shell | Builds | clang-tidy |
|---|---|---|---|---|
| `vs` | MSVC, Visual Studio (multi-config) | any | add-on and tests | yes |
| `ninja-debug`, `ninja-release` | MSVC, Ninja | MSVC environment | add-on and tests | yes |
| `clang-debug`, `clang-release` | clang-cl, Ninja | MSVC environment | add-on and tests | no |
| `clang-sanitize` | clang-cl with ASan + UBSan, Ninja | MSVC environment | tests only | no |

"MSVC environment" means a *x64 Native Tools Command Prompt for VS*, or an IDE that sets it up (Visual Studio, CLion,
VS Code). For example:

```sh
cmake --preset clang-sanitize
cmake --build --preset clang-sanitize
ctest --preset clang-sanitize
```

### IDE

The presets work in any IDE that supports CMake presets:

- **Visual Studio:** *File > Open > Folder* on the repository, then pick a configure preset (`Ninja Debug (x64
  MSVC)` is the usual one) in the toolbar's configuration dropdown. Visual Studio configures on its own, which runs
  Conan the first time; *Build > Build All* builds and *Test > Test Explorer* runs the unit tests. Visual Studio must
  find Conan on PATH, so start it after installing Conan.
- **CLion:** *Settings > Build, Execution, Deployment > CMake* and enable the presets. Use the Visual Studio toolchain
  (x64) with the `ninja-*` and `clang-*` presets.
- **VS Code:** install the CMake Tools extension and pick a configure preset. Use the Ninja presets from a developer
  environment, or use `vs`.

### clang-cl and the sanitizers

MSVC builds the add-on for the game. The `clang-*` presets build with **clang-cl** too, from the same "C++ Clang tools"
component. A second compiler has different warnings and rejects some non-portable code MSVC accepts, so it finds
problems MSVC lets through.

- **`clang-debug` / `clang-release`:** everything, add-on included, with the same warnings-as-errors policy as MSVC.
- **`clang-sanitize`:** only the unit tests and `ds3ea_core`, built with the sanitizers, so a memory bug or undefined
  behaviour fails the test run with a report naming the line. The add-on isn't built: it isn't for the game.
  - **AddressSanitizer** catches out-of-bounds reads and writes on the heap, stack and globals (including past the
    size of a `std::vector` or `std::string`), use after free, double free and use of the stack after return.
  - **UndefinedBehaviorSanitizer** catches signed overflow, out-of-range shifts, null or misaligned access, invalid
    `bool` and enum values, integer division by zero and similar.
  - **Not caught:** leaks (LeakSanitizer doesn't exist on Windows; Debug builds of the add-on have the CRT leak
    check instead), data races and uninitialised reads (TSan and MSan aren't available either), and anything in code
    the tests don't run.

With clang-cl, Conan builds its own `compiler=clang` packages (and ASan ones for `clang-sanitize`), which takes about
half a minute the first time each preset is configured. clang-tidy is off in these presets, since the MSVC builds
already run it.

### Testing in the game

For development and testing: `python scripts/launch_game.py` installs the newest build, starts the game and loads
the last save, closes it after 60 seconds and prints the run's `ReShade.log`. `--title-only` stays on the title
screen. See [scripts/README.md](scripts/README.md).

A Debug build also checks for leaks: it writes `ds3ea_leaks.log` in the game folder, which lists every block the
add-on still had allocated when it unloaded. An empty file means no leaks, but only after a normal exit (Alt+F4, as
the launcher does) that unloads the add-on. If the game is killed, the report is never written. DS3 1.15.0 also
corrupts its own heap at exit, even without ReShade, and that sometimes ends the process (`0xc0000374`) before the
add-on unloads. `ReShade.log` then has no `Unregistered add-on "DS3EA"` line, and the empty report means nothing.

## Installing

1. Install ReShade **with full add-on support** (the "ReShade with full add-on support" installer from reshade.me)
   for `DarkSoulsIII.exe` with the DirectX 10/11/12 API.
2. Copy `ds3ea.addon64` next to `DarkSoulsIII.exe` (the `Game` folder), or into the folder set as
   *Add-on search path* in ReShade's settings.
3. Start the game. **DS3EA** shows up in the overlay's *Add-ons* tab, and `ReShade.log` has the line
   `DS3EA loaded, settings from ds3ea.ini`.

Settings live in `ds3ea.ini` in the game's working directory (the `Game` folder). The file is optional: missing
settings use their defaults. They can also be changed in game, under **DS3EA** in the overlay's *Add-ons* tab, and
changes are saved to `ds3ea.ini` within half a second. A Debug build also reloads `ds3ea.ini` when another program
edits it while the game runs.

## Attribution

- The frame rate unlock is based on [DS3DebugFPS](https://github.com/0dm/DS3DebugFPS) by 0dm, under the
  [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0): its byte patterns, field offsets and sprint fix
  maths.
