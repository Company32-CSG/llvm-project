#include "Doxygen.hpp"
#include "Utils.hpp"

#include "../CodeComplete.h"
#include "../support/Markup.h"

#include "llvm/ADT/ArrayRef.h"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::doxygen {

const std::vector<DoxygenTag> TagList = {
	{ "brief", "Summary of documented symbol." },
	{ "deprecated", "Mark usage of the documented symbol as deprecated." },
	{ "example", "Example usage: @example[c] { ... }", { "usage" } },
	{ "markdown", "Inline markdown", { "md" } },
	{ "param", "Function parameter. Supports [in], [out], [in:optional], etc." },
	{ "ref", "Reference to a defined symbol." },
	{ "returns", "Description of return value.", { "return" } },
	{ "retval", "Description of a specific return value.", { "ret", "result" } },
	{ "since", "API version this symbol was first available (e.g., `@since 1.5.0`)" },
	{ "warning", "Provide a warning to anyone using the documented symbol." },
};

constexpr std::string_view TagInitiatorList[] = {
	"@",
	"\\",
};

static std::pair<size_t, std::string_view> findClosestTagInitiator(std::string_view contents, size_t offset);

/* ------------------------------------------------------------ */

const llvm::ArrayRef<DoxygenTag>
getAllTags()
{
	return llvm::ArrayRef(TagList);
}

const llvm::ArrayRef<std::string_view>
getAllTagInitiators()
{
	return llvm::ArrayRef(TagInitiatorList, std::size(TagInitiatorList));
}

CodeCompleteResult
tagCompletion(std::string_view contents, size_t offset)
{
	CodeCompleteResult result;
	std::string prefix;

	/* Find the tag initiator closest to the cursor! */
	auto [tagPos, tagInitiator] = findClosestTagInitiator(contents, offset);

	if (std::string_view::npos == tagPos)
		return CodeCompleteResult();

	if ((tagPos + 1U) < offset)
		prefix = contents.substr(tagPos + 1U, offset - tagPos - 1U);

	Range completionRange = {
		offsetToPosition(contents, tagPos),
		offsetToPosition(contents, offset),
	};

	auto buildItemDoc = [](const DoxygenTag& tag)
	{
		markup::Document doc;

		doc.addParagraph().appendText(tag.description);

		return doc;
	};

	/* C++ 17 doesn't allow capturing structured bindings so we gotta pass tagInitiator explicitly by ref */
	auto buildItem = [&, &tagInitiator = tagInitiator](const DoxygenTag& tag)
	{
		std::vector<CodeCompletion> ret;

		ret.reserve(1 + tag.aliases.size());

		CodeCompletion& item = ret.emplace_back();

		item.Name				  = std::string(tagInitiator) + std::string(tag.name);
		item.FilterText			  = item.Name;
		item.Kind				  = CompletionItemKind::Property;
		item.Documentation		  = buildItemDoc(tag);
		item.CompletionTokenRange = completionRange;

		CodeCompletion itemAlias = item;

		for (const auto& alias : tag.aliases)
		{
			itemAlias.Name		 = std::string(tagInitiator) + std::string(alias);
			itemAlias.FilterText = itemAlias.Name;

			ret.push_back(itemAlias);
		}

		return ret;
	};

	/* Return all tags since we are matching on just initiator (e.g., '@') */
	if (prefix.empty())
	{
		for (const auto& tag : TagList)
		{
			auto tags = buildItem(tag);

			result.Completions.insert(result.Completions.end(), tags.begin(), tags.end());
		}

		return result;
	}

	/* With a prefix, loop through and find all matching tags */
	for (const auto& tag : TagList)
	{
		/* Does it match the tag name? */
		if (0U == tag.name.rfind(prefix, 0U))
		{
			auto tags = buildItem(tag);

			result.Completions.insert(result.Completions.end(), tags.begin(), tags.end());
			continue;
		}

		/* Does it match one of the tag's aliases? */
		for (const auto& alias : tag.aliases)
		{
			if (0U == alias.rfind(prefix, 0U))
			{
				auto tags = buildItem(tag);

				result.Completions.insert(result.Completions.end(), tags.begin(), tags.end());
				break;
			}
		}
	}

	return result;
}

bool
inDoxygenComment(std::string_view contents, size_t cursorOffset)
{
	size_t starBlockStart = contents.rfind("/**", cursorOffset);
	size_t exclBlockStart = contents.rfind("/*!", cursorOffset);
	size_t blockStart	  = std::string::npos;

	/* Select where to start. We want the comment block that is closest to the cursor! */

	if (std::string::npos != starBlockStart && std::string::npos != exclBlockStart)
		blockStart = std::max(starBlockStart, exclBlockStart);
	else if (std::string::npos != starBlockStart)
		blockStart = starBlockStart;
	else if (std::string::npos != exclBlockStart)
		blockStart = exclBlockStart;

	/* Found the start of a '/** or '/*!' Doxygen comment */
	if (std::string::npos != blockStart)
	{
		size_t blockEnd = contents.find("*/", blockStart);

		/* We are definitely in a Doxygen comment */
		if (std::string::npos == blockEnd || cursorOffset < blockEnd)
			return true;
	}

	bool startsWithThreeSlash = lineStartsWith(contents, cursorOffset, "///");
	bool startsWithExclSlash  = lineStartsWith(contents, cursorOffset, "//!");

	/* We are on a '///' or '//!' line */
	if (startsWithThreeSlash || startsWithExclSlash)
		return true;

	/* Not in a Doxygen comment */
	return false;
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

static std::pair<size_t, std::string_view>
findClosestTagInitiator(std::string_view contents, size_t offset)
{
	std::pair<size_t, std::string_view> ret = { std::string_view::npos, "" };

	for (const auto& initiator : TagInitiatorList)
	{
		size_t pos = contents.rfind(initiator, offset);

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

} // namespace clang::clangd::c32::doxygen
