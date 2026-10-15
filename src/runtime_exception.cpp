#include "runtime_exception.hpp"

#include <utility>

namespace ds3ea
{
RuntimeException::RuntimeException(Error error)
	: std::runtime_error(error.message)
	, m_error(std::move(error))
{
}

const Error& RuntimeException::get_error() const
{
	return m_error;
}
} // namespace ds3ea
