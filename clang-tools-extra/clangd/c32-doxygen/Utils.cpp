#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace clang::clangd::c32 {

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

std::string
indentLines(std::string_view input)
{
	// Indent first line
	std::string result = "\t";

	for (auto c : input)
	{
		result += c;

		if ('\n' == c)
			result += '\t';
	}

	return result;
}

std::string
canonicalizeWhitespace(std::string_view contents, bool preserveNewlines)
{
	std::string ret;

	ret.reserve(contents.size());

	bool   inSpace		= false;
	size_t newlineCount = 0;

	for (const char c : contents)
	{
		unsigned const char uc = static_cast<unsigned char>(c);

		/* Preserve new lines */
		if ('\n' == c && preserveNewlines)
		{
			newlineCount += 1U;
			inSpace = false;

			/* Allow up to TWO newlines in a row (one blank line) */
			if (newlineCount <= 2U)
				ret.push_back('\n');

			/* Skip any further newlines */
			continue;
		}

		/* If not a newline, reset counter */
		if ('\n' != c)
			newlineCount = 0;

		/* Collapse other whitespace to single ' ' */
		if (std::isspace(uc))
		{
			/* Skip '\n' here (we handled it above) so this is spaces/tabs/etc. */
			if (inSpace)
				continue;

			inSpace = true;

			ret.push_back(' ');
			continue;
		}

		/* Normal character: emit and clear flags */
		inSpace = false;

		ret.push_back(c);
	}

	/* Trim trailing space (but leave trailing newline(s)) */
	if (!ret.empty() && ' ' == ret.back())
		ret.pop_back();

	return ret;
}

std::string_view::size_type
findFirstSpace(std::string_view contents)
{
	const auto* itr = std::find_if(
		contents.begin(),
		contents.end(),
		[](char c)
		{ return std::isspace(static_cast<unsigned char>(c)); }
	);

	return itr == contents.end()
		? std::string_view::npos
		: std::distance(contents.begin(), itr);
}

std::string
ltrim(std::string_view contents)
{
	size_t n = contents.size();
	size_t i = 0U;

	while (i < n && std::isspace(static_cast<unsigned char>(contents[i])))
		i += 1U;

	return std::string(contents.substr(i));
}

std::string
rtrim(std::string_view contents)
{
	size_t n = contents.size();

	while (n > 0U && std::isspace(static_cast<unsigned char>(contents[n - 1U])))
		n -= 1U;

	return std::string(contents.substr(0U, n));
}

std::string
trim(std::string_view contents)
{
	size_t start = 0U;
	size_t end	 = contents.size();

	while (start < end && std::isspace(static_cast<unsigned char>(contents[start])))
		start++;

	while (end > start && std::isspace(static_cast<unsigned char>(contents[end - 1U])))
		end--;

	return std::string(contents.substr(start, end - start));
}

std::vector<std::string>
split(std::string_view contents, std::string_view splitter)
{
	std::vector<std::string> ret;

	if (contents.empty() || splitter.empty() || splitter.size() > contents.size())
		return {};

	size_t pos;

	while (1)
	{
		pos = contents.find(splitter);

		if (std::string_view::npos == pos)
			break;

		auto sub = contents.substr(0U, pos);

		ret.push_back(std::string(sub));

		contents.remove_prefix(pos + splitter.size());
	}

	/* There might be a piece remaining after the last split */

	if (!contents.empty())
		ret.push_back(std::string(contents));

	return ret;
}

std::string
lowercase(std::string_view contents)
{
	std::string ret(contents.size(), '\0');

	std::transform(contents.begin(), contents.end(), ret.begin(), ::tolower);

	return ret;
}

std::string
uppercase(std::string_view contents)
{
	std::string ret(contents.size(), '\0');

	std::transform(contents.begin(), contents.end(), ret.begin(), ::toupper);

	return ret;
}

std::string
properNounCase(std::string_view contents)
{
	std::string ret(contents.size(), '\0');

	std::transform(contents.begin(), contents.end(), ret.begin(), ::tolower);

	if (!ret.empty())
		ret[0U] = ::toupper(ret[0U]);

	return ret;
}

} // namespace clang::clangd::c32
