#include "runtime_error.hpp"

#include <utility>

namespace ds3ea
{
RuntimeError::RuntimeError(Error error)
	: std::runtime_error(error.message)
	, m_error(std::move(error))
{
}

const Error& RuntimeError::get_error() const
{
	return m_error;
}
} // namespace ds3ea
