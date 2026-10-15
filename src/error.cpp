#include "error.hpp"

#include <string_view>

namespace ds3ea
{
std::string_view get_category_name(ErrorCategory category)
{
	switch (category)
	{
	case ErrorCategory::not_found:
		return "not_found";
	case ErrorCategory::invalid_data:
		return "invalid_data";
	case ErrorCategory::unsupported:
		return "unsupported";
	case ErrorCategory::io:
		return "io";
	case ErrorCategory::device:
		return "device";
	case ErrorCategory::internal:
		return "internal";
	}

	return "unknown";
}
} // namespace ds3ea

// fmt's formatter for our Error type (declared in error.hpp), so an Error can be passed to fmt::format or log::error
// like any value: log::error("{}", error) writes "[not_found] cannot open ds3ea.ini (ini_settings_store.cpp:42)"
fmt::format_context::iterator fmt::formatter<ds3ea::Error>::format(const ds3ea::Error& error,
	fmt::format_context& ctx) const
{
	// The file name without its directory; MSVC's function names are full signatures, too long to be worth showing
	const std::string_view path = error.location.file_name();
	const std::string_view file = path.substr(path.find_last_of("\\/") + 1);
	return fmt::format_to(ctx.out(), "[{}] {} ({}:{})", ds3ea::get_category_name(error.category), error.message, file,
		error.location.line());
}
