#include "Doxygen.hpp"
#include "Utils.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

#include <cctype>
#include <cstddef>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::doxygen {

const std::vector<DoxygenTag> TagList = {
	{ TagType::Brief, TagParsingFlags(false, true), "brief", "Summary of documented symbol." },
	{ TagType::Code, TagParsingFlags(false, true, true), "code", "Marks the beginning of a code block." },
	{ TagType::EndCode, TagParsingFlags(false, false), "endcode", "Marks the end of a code block." },
	{ TagType::Deprecated, TagParsingFlags(false, false), "deprecated", "Mark usage of the documented symbol as deprecated." },
	{ TagType::Example, TagParsingFlags(false, true), "example", "Example usage: @example[c] { ... }", { "usage" } },
	{ TagType::Member, TagParsingFlags(true, false), "member", "Reference a member of the current class or struct.", { "m" } },
	{ TagType::P, TagParsingFlags(true, false), "p", "Reference to a parameter defined by the @param tag.", { "pref" } },
	{ TagType::Param, TagParsingFlags(false, false), "param", "Function parameter. Supports [in], [out], [in,out,optional], etc." },
	{ TagType::Ref, TagParsingFlags(true, false), "ref", "Reference to a defined symbol.", { "r" } },
	{ TagType::Returns, TagParsingFlags(false, false), "returns", "Description of return value.", { "return" } },
	{ TagType::Retval, TagParsingFlags(false, false), "retval", "Description of a specific return value.", { "ret", "result" } },
	{ TagType::Warning, TagParsingFlags(false, false), "warning", "Provide a warning to anyone using the documented symbol." },

	{ TagType::Custom, TagParsingFlags(false, true), "note", "Additional notes or commentary." },
	{ TagType::Custom, TagParsingFlags(false, false), "attention", "Highlights something that needs attention." },
	{ TagType::Custom, TagParsingFlags(false, false), "author", "Specifies the author of the code or documentation." },
	{ TagType::Custom, TagParsingFlags(false, false), "copyright", "Specifies copyright details." },
	{ TagType::Custom, TagParsingFlags(false, false), "date", "Specifies the date of the documentation or change." },
	{ TagType::Custom, TagParsingFlags(false, true), "details", "Provides detailed documentation following @brief." },
	{ TagType::Custom, TagParsingFlags(false, false), "exception", "Documents an exception that may be thrown." },
	{ TagType::Custom, TagParsingFlags(false, false), "ingroup", "Associates a symbol with a documentation group." },
	{ TagType::Custom, TagParsingFlags(false, false), "li", "Represents a list item inside @par or similar sections." },
	{ TagType::Custom, TagParsingFlags(false, false), "mainpage", "Specifies the main page content of the documentation." },
	{ TagType::Custom, TagParsingFlags(false, false), "name", "Sets the name of a group or section." },
	{ TagType::Custom, TagParsingFlags(false, false), "par", "Starts a paragraph block." },
	{ TagType::Custom, TagParsingFlags(false, false), "post", "Postcondition for a function or method." },
	{ TagType::Custom, TagParsingFlags(false, false), "pre", "Precondition for a function or method." },
	{ TagType::Custom, TagParsingFlags(false, true), "remark", "Provides an additional remark or observation." },
	{ TagType::Custom, TagParsingFlags(false, false), "see", "Cross-reference to another documented entity." },
	{ TagType::Custom, TagParsingFlags(false, false), "since", "Documents when the symbol was added." },
	{ TagType::Custom, TagParsingFlags(false, false), "throws", "Documents an exception a function may throw." },
	{ TagType::Custom, TagParsingFlags(false, false), "todo", "Marks something that needs to be completed." },
	{ TagType::Custom, TagParsingFlags(false, false), "tparam", "Describes a template parameter.", { "templateparam" } },
	{ TagType::Custom, TagParsingFlags(false, false), "version", "Specifies version information." },
	{ TagType::Custom, TagParsingFlags(false, false), "section", "Defines a named documentation section." },
	{ TagType::Custom, TagParsingFlags(false, false), "subsection", "Defines a subsection inside a section." },
	{ TagType::Custom, TagParsingFlags(false, true), "verbatim", "Begins a raw text block." },
	{ TagType::Custom, TagParsingFlags(false, false), "endverbatim", "Ends a raw text block." },
	{ TagType::Custom, TagParsingFlags(false, false), "defgroup", "Defines a named documentation group." },
	{ TagType::Custom, TagParsingFlags(false, false), "addtogroup", "Adds symbols to an existing documentation group." },
	{ TagType::Custom, TagParsingFlags(false, false), "anchor", "Marks a location for cross-referencing." },
	{ TagType::Custom, TagParsingFlags(false, false), "link", "Starts an inline link to a documented symbol." },
	{ TagType::Custom, TagParsingFlags(false, false), "endlink", "Ends an inline link block." },
	{ TagType::Custom, TagParsingFlags(false, false), "htmlonly", "Section only rendered in HTML output." },
	{ TagType::Custom, TagParsingFlags(false, false), "endhtmlonly", "Ends HTML-only section." },
	{ TagType::Custom, TagParsingFlags(false, false), "latexonly", "Section only rendered in LaTeX output." },
	{ TagType::Custom, TagParsingFlags(false, false), "endlatexonly", "Ends LaTeX-only section." },
};

constexpr char TagInitiatorList[] = { '@', '\\', '%' };

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

		ret.insert(ret.end(), tag.aliases.begin(), tag.aliases.end());
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

	/* Handle the ref initiator specially */
	if ('%' == ret.initiator)
	{
		if (TagContext::Inline != context && TagContext::Any != context)
			return std::nullopt;

		/* Loop through and find the REF tag descriptor */
		for (const auto& tag : TagList)
		{
			if (TagType::Ref == tag.type)
				ret.tag = &tag;
		}

		ret.name	 = ret.tag->name;
		ret.consumed = 1U;

		return ret;
	}

	/* Advance past whitespace */

	while (pos < sv.size() && std::isspace(static_cast<unsigned char>(sv[pos])))
		pos += 1U;

	for (const auto& tag : TagList)
	{
		if (TagContext::Any != context)
		{
			bool inlineOnly = (TagContext::Inline == context);
			bool blockOnly	= (TagContext::Block == context);

			if ((inlineOnly && !tag.flags.Inline) || (blockOnly && tag.flags.Inline))
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
