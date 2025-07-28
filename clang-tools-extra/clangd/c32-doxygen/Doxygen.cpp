#include "Doxygen.hpp"
#include "Utils.hpp"

#include "../CodeComplete.h"
#include "../support/Markup.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::doxygen {

const std::vector<DoxygenTag> TagList = {
	{ TagType::Brief, false, "brief", "Summary of documented symbol." },
	{ TagType::Deprecated, false, "deprecated", "Mark usage of the documented symbol as deprecated." },
	{ TagType::Example, false, "example", "Example usage: @example[c] { ... }", { "usage" } },
	{ TagType::Member, true, "member", "Reference a member of the current class or struct.", { "m" } },
	{ TagType::P, true, "p", "Reference to a parameter defined by the @param tag.", { "pref" } },
	{ TagType::Param, false, "param", "Function parameter. Supports [in], [out], [in,out,optional], etc." },
	{ TagType::Ref, true, "ref", "Reference to a defined symbol.", { "r" } },
	{ TagType::Returns, false, "returns", "Description of return value.", { "return" } },
	{ TagType::Retval, false, "retval", "Description of a specific return value.", { "ret", "result" } },
	{ TagType::Warning, false, "warning", "Provide a warning to anyone using the documented symbol." },

	{ TagType::Custom, false, "note", "Additional notes or commentary." },
	{ TagType::Custom, false, "attention", "Highlights something that needs attention." },
	{ TagType::Custom, false, "author", "Specifies the author of the code or documentation." },
	{ TagType::Custom, false, "copyright", "Specifies copyright details." },
	{ TagType::Custom, false, "date", "Specifies the date of the documentation or change." },
	{ TagType::Custom, false, "details", "Provides detailed documentation following @brief." },
	{ TagType::Custom, false, "exception", "Documents an exception that may be thrown." },
	{ TagType::Custom, false, "ingroup", "Associates a symbol with a documentation group." },
	{ TagType::Custom, false, "li", "Represents a list item inside @par or similar sections." },
	{ TagType::Custom, false, "mainpage", "Specifies the main page content of the documentation." },
	{ TagType::Custom, false, "name", "Sets the name of a group or section." },
	{ TagType::Custom, false, "par", "Starts a paragraph block." },
	{ TagType::Custom, false, "post", "Postcondition for a function or method." },
	{ TagType::Custom, false, "pre", "Precondition for a function or method." },
	{ TagType::Custom, false, "remark", "Provides an additional remark or observation." },
	{ TagType::Custom, false, "remark", "Provides an additional remark or observation." },
	{ TagType::Custom, false, "see", "Cross-reference to another documented entity." },
	{ TagType::Custom, false, "since", "Documents when the symbol was added." },
	{ TagType::Custom, false, "throws", "Documents an exception a function may throw." },
	{ TagType::Custom, false, "todo", "Marks something that needs to be completed." },
	{ TagType::Custom, false, "tparam", "Describes a template parameter.", { "templateparam" } },
	{ TagType::Custom, false, "version", "Specifies version information." },
	{ TagType::Custom, false, "section", "Defines a named documentation section." },
	{ TagType::Custom, false, "subsection", "Defines a subsection inside a section." },
	{ TagType::Custom, false, "code", "Marks the beginning of a code block." },
	{ TagType::Custom, false, "endcode", "Marks the end of a code block." },
	{ TagType::Custom, false, "verbatim", "Begins a raw text block." },
	{ TagType::Custom, false, "endverbatim", "Ends a raw text block." },
	{ TagType::Custom, false, "defgroup", "Defines a named documentation group." },
	{ TagType::Custom, false, "addtogroup", "Adds symbols to an existing documentation group." },
	{ TagType::Custom, false, "anchor", "Marks a location for cross-referencing." },
	{ TagType::Custom, false, "link", "Starts an inline link to a documented symbol." },
	{ TagType::Custom, false, "endlink", "Ends an inline link block." },
	{ TagType::Custom, false, "htmlonly", "Section only rendered in HTML output." },
	{ TagType::Custom, false, "endhtmlonly", "Ends HTML-only section." },
	{ TagType::Custom, false, "latexonly", "Section only rendered in LaTeX output." },
	{ TagType::Custom, false, "endlatexonly", "Ends LaTeX-only section." },
};

constexpr char TagInitiatorList[] = {
	'@',
	'\\',
	'%'
};

/* ------------------------------------------------------------ */

const llvm::ArrayRef<DoxygenTag>
getAllTags()
{
	return llvm::ArrayRef(TagList);
}

const std::vector<std::string_view>
getAllTagNames()
{
	std::vector<std::string_view> ret;

	size_t size = TagList.size();

	for (const auto& tag : TagList)
		size += tag.aliases.size();

	ret.reserve(size);

	for (const auto& tag : TagList)
	{
		ret.push_back(tag.name);

		ret.insert(
			ret.end(),
			tag.aliases.begin(),
			tag.aliases.end()
		);
	}

	return ret;
}

const llvm::ArrayRef<char>
getAllTagInitiators()
{
	return llvm::ArrayRef(TagInitiatorList, std::size(TagInitiatorList));
}

bool
isDoxygenTagInitiator(std::string_view contents)
{
	for (const auto& initiator : TagInitiatorList)
	{
		if (0U == contents.rfind(initiator, 0U))
			return true;
	}

	return false;
}

bool
isDoxygenTagInitiator(char c)
{
	for (const auto& ini : TagInitiatorList)
	{
		if (ini == c)
			return true;
	}

	return false;
}

const DoxygenTag*
getDoxygenTagByName(std::string_view name)
{
	auto lower = lowercase(name);

	for (const auto& tag : TagList)
	{
		if (tag.name == lower)
			return &tag;

		for (const auto& alias : tag.aliases)
		{
			if (alias == lower)
				return &tag;
		}
	}

	return nullptr;
}

std::pair<size_t, char>
findClosestTagInitiator(std::string_view contents)
{
	std::pair<size_t, char> ret = { std::string_view::npos, ' ' };

	for (const auto initiator : TagInitiatorList)
	{
		size_t pos = contents.find(initiator);

		if (std::string_view::npos != pos)
		{
			if (ret.first == std::string_view::npos)
			{
				ret.first  = pos;
				ret.second = initiator;
			}
			else if (ret.first < pos)
			{
				ret.first  = pos;
				ret.second = initiator;
			}
		}
	}

	return ret;
}

bool
isEscaping(std::string_view contents, size_t offset)
{
	if (0U == offset || offset >= contents.size())
		return false;

	return ('^' == contents[offset - 1U]);
}

bool
isDoxygenEscape(const char c)
{
	return ('^' == c);
}

std::string
unescape(std::string_view sv)
{
	std::string result;

	result.reserve(sv.size());

	bool escape = false;

	for (size_t i = 0U; i < sv.size(); i++)
	{
		if (isDoxygenEscape(sv[i]) && !escape)
		{
			escape = true;

			continue;
		}

		result += sv[i];
		escape = false;
	}

	return result;
}

bool
tagStartsLine(std::string_view sv, size_t pos)
{
	if (pos >= sv.size())
		return false;

	auto lineStart = sv.rfind('\n', pos);

	if (std::string_view::npos == lineStart)
		lineStart = 0U;
	else
		lineStart += 1U;

	if (lineStart == pos)
		return true;

	for (const auto c : sv.substr(lineStart, pos - lineStart))
	{
		if (!(std::isspace(static_cast<unsigned char>(c))))
			return false;
	}

	return true;
}

std::optional<MatchedTag>
getTag(std::string_view sv, size_t pos, TagContext context)
{
	MatchedTag ret;

	auto initialPosition = pos;

	if (pos >= sv.size())
		return std::nullopt;

	if (0U != pos && isDoxygenEscape(sv[pos - 1U]))
		return std::nullopt;

	if (!isDoxygenTagInitiator(sv[pos]))
		return std::nullopt;

	ret.initiator = sv[pos];

	/* Advance past the tag initiator character */
	pos += 1U;

	/* Advance past whitespace */

	while (pos < sv.size() && std::isspace(static_cast<unsigned char>(sv[pos])))
		pos += 1U;

	for (const auto& tag : TagList)
	{
		if (TagContext::Any != context)
		{
			bool inlineOnly = (TagContext::Inline == context);
			bool blockOnly	= (TagContext::Block == context);

			if ((inlineOnly && !tag.isInline) || (blockOnly && tag.isInline))
				continue;
		}

		/* Check for aliases first */
		for (const auto& alias : tag.aliases)
		{
			size_t nameLen = alias.size();

			if (0 != sv.compare(pos, nameLen, alias))
				continue;

			auto after = pos + nameLen;

			if (after != sv.size() && !isTagTerminator(sv[after]))
				continue;

			ret.tag		 = &tag;
			ret.name	 = alias;
			ret.consumed = after - initialPosition;

			return ret;
		}

		/* Now check the canonical tag name */
		size_t nameLen = tag.name.size();

		if (0 != sv.compare(pos, nameLen, tag.name))
			continue;

		auto after = pos + nameLen;

		if (after != sv.size() && !isTagTerminator(sv[after]))
			continue;

		ret.tag		 = &tag;
		ret.name	 = tag.name;
		ret.consumed = after - initialPosition;

		return ret;
	}

	return std::nullopt;
}

bool
isTagTerminator(char c)
{
	unsigned char uc = static_cast<unsigned char>(c);

	// letters, digits or underscore are _not_ terminators:
	return !(std::isalnum(uc) || c == '_' || c == ':');
}

} // namespace clang::clangd::c32::doxygen
