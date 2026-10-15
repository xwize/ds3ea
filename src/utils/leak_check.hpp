#pragma once

#include <filesystem>

namespace ds3ea
{
/**
 * Turns on the debug CRT's heap tracking and its leak report, written to report_path when the add-on unloads. Call it
 * first thing on process attach, so start-up allocations are tracked too. We link the CRT statically, so it sees only
 * our own heap, not ReShade's or the game's. In Release the CRT calls compile to nothing.
 *
 * The file is emptied here, and the CRT lists the leaks in it on DLL_PROCESS_DETACH, after our static objects are
 * destroyed. That needs a normal exit (Alt+F4 ends in ExitProcess): if the game is killed (TerminateProcess), no
 * detach runs and the file stays empty too. So an empty report means "no leaks" only after a normal exit.
 * @param report_path The leak report, e.g. ds3ea_leaks.log next to the add-on.
 * @throws RuntimeError (io) if the file can't be created; leaks then go to the debugger output, the CRT's default.
 */
void enable_leak_check(const std::filesystem::path& report_path);
} // namespace ds3ea
