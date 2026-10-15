#include "utils/leak_check.hpp"
#include "utils/error.hpp"
#include "utils/runtime_error.hpp"

#include <Windows.h>
#include <crtdbg.h>
#include <fmt/format.h>
#include <fmt/std.h>

#include <system_error>

namespace ds3ea
{
void enable_leak_check(const std::filesystem::path& report_path)
{
	// A Win32 handle rather than a stream or our logger: the report is written during CRT shutdown, when those may be
	// gone. It is never closed, since the CRT writes to it after everything of ours has been destroyed.
	HANDLE const report = CreateFileW(report_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (report == INVALID_HANDLE_VALUE)
	{
		const std::error_code error(static_cast<int>(GetLastError()), std::system_category());
		throw RuntimeError({ ErrorCategory::io, fmt::format("cannot create {}: {}", report_path, error.message()) });
	}

	// The leak list is reported as _CRT_WARN; _CRT_ERROR and _CRT_ASSERT keep their usual reporting
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, report);
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
}
} // namespace ds3ea
