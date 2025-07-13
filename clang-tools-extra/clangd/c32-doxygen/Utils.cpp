#include "Utils.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace clang::clangd::c32::doxygen {

/* ------------------------------------------------------------ */

bool
lineStartsWith(std::string_view contents, size_t cursorOffset, std::string_view prefix)
{
	if (cursorOffset > contents.size())
		return false;

	size_t lineStart = contents.rfind('\n', cursorOffset);

	if (std::string::npos == lineStart)
		lineStart = 0U; // Might be beginning of file, no '\n' found
	else
		lineStart += 1U; // Skip '\n' character

	if (cursorOffset < lineStart)
		return false;

	auto trimmed = std::string_view(contents).substr(lineStart, cursorOffset - lineStart);

	while (!trimmed.empty() && std::isspace(trimmed.front()))
		trimmed.remove_prefix(1U);

	return trimmed.size() >= prefix.size() && prefix == trimmed.substr(0U, prefix.size());
}

std::pair<size_t, std::string_view>
extractLine(std::string_view contents, size_t offset)
{
	if (offset > contents.size())
		return { std::string_view::npos, "" };

	size_t lineStart = contents.rfind('\n', 0U == offset ? 0U : (offset - 1U));

	if (std::string_view::npos == lineStart)
		lineStart = 0U; // Might be beginning of file, no '\n' found
	else
		lineStart += 1U; // Skip '\n' character

	if (offset < lineStart)
		return { std::string_view::npos, "" };

	return { lineStart, contents.substr(lineStart, offset - lineStart) };
}

} // namespace clang::clangd::c32::doxygen
