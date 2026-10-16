#pragma once

#include "utils/error.hpp"

#include <stdexcept>

namespace ds3ea
{
/**
 * Our one exception type: it carries an Error, and what() returns the Error's message. Catch it by const reference
 * and switch on the Error's category to handle some categories and rethrow the rest.
 */
class RuntimeError : public std::runtime_error
{
public:
	/** @param error Why it failed and where it was raised. */
	explicit RuntimeError(Error error);

	/** @returns Why it failed and where it was raised. */
	[[nodiscard]] const Error& get_error() const;

private:
	Error m_error;
};
} // namespace ds3ea
