#pragma once

namespace ds3ea
{
/**
 * True in Debug builds (the debug CRT defines _DEBUG). Debug-only code branches on it with if constexpr instead of
 * sitting inside #ifdef _DEBUG, so it is compiled, and kept compiling, in every configuration.
 */
#ifdef _DEBUG
constexpr bool k_debug_build = true;
#else
constexpr bool k_debug_build = false;
#endif

/** Builds the add-on with IntroSkipper patching the game to skip its intro logos. */
constexpr bool k_skip_intro_in_build = true;
} // namespace ds3ea
