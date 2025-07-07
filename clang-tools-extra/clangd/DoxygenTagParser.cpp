
#include "DoxygenTagParser.hpp"

#include "Hover.h"
#include "support/Markup.h"
#include "clang/Basic/CharInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace clang::clangd::c32 {

/**
 * @brief Consume the first line of the string, advancing `pContent` to the next line.
 *
 * @param pContent[in,out] Reference to the string
 *
 * @return `llvm::StringRef`
 */
static llvm::StringRef consumeLine(llvm::StringRef& pContent);
static llvm::StringRef peekLine(llvm::StringRef content);
static bool lineStartsWithTag(llvm::StringRef content);

static DoxygenTag consumeTag(llvm::StringRef& content);
static std::optional<DoxygenGenericTag> consumeGeneric(llvm::StringRef& content);
static std::string consumeBrief(llvm::StringRef& content);
static DoxygenParamTag consumeParam(llvm::StringRef& content);
static std::optional<DoxygenExampleTag> consumeExample(llvm::StringRef& content);

static std::optional<llvm::StringRef> getBacktickQuoteRange(llvm::StringRef Line, size_t Offset);
static void buildMarkdown(DoxygenData& data, markup::Document& pOutput);

static const std::map<DoxygenTag, std::string> SupportedDoxygenTags = {
	{ DoxygenTag::Brief, "brief" },
	{ DoxygenTag::Param, "param" },
	{ DoxygenTag::Example, "example" },
};

/* **************************************************************** */

void
parseDoxygenTags(const HoverInfo* hoverInfo, llvm::StringRef comment, markup::Document& pOutput)
{
	DoxygenData data;

	// MARK: - Helper Lambdas

	auto matchParamsToDoxygen = [&](DoxygenParamTag& param)
	{
		if (auto const hoverParams = hoverInfo->Parameters)
		{
			auto hoverParam = std::find_if((*hoverParams).begin(), (*hoverParams).end(), [&](const HoverInfo::Param& hoverParam) { return hoverParam.Name == param.name; });

			if (hoverParam != (*hoverParams).end())
			{
				param.type	  = hoverParam->Type->Type;
				param.typeAka = hoverParam->Type->AKA;
			}
		}
	};

	// MARK: - Parse Doxygen

	while (!comment.empty())
	{
		auto tag = consumeTag(comment);

		switch (tag)
		{
			case DoxygenTag::Generic:
			{
				auto generic = consumeGeneric(comment);

				if (auto& value = generic)
					data.genericTags.push_back(std::move(*value));

				break;
			}

			case DoxygenTag::Brief:
			{
				data.brief = consumeBrief(comment);

				break;
			}

			case DoxygenTag::Param:
			{
				DoxygenParamTag param = consumeParam(comment);

				matchParamsToDoxygen(param);

				data.params.push_back(std::move(param));
				break;
			}

			case DoxygenTag::Example:
			{
				data.example = consumeExample(comment);

				break;
			}

			case DoxygenTag::NoTag:
			default:
			{
				auto consumed = consumeLine(comment);

				data.userLines.push_back(consumed.str());
				break;
			}
		}
	}

	// MARK: - Build Markdown

	buildMarkdown(data, pOutput);
}

static llvm::StringRef
consumeLine(llvm::StringRef& content)
{
	if (content.empty())
		return {};

	size_t end = content.find('\n');

	if (end == content.npos)
	{
		auto line = content;

		content = {};

		return line;
	}

	auto line = content.substr(0, end);

	content = content.substr(end + 1U);

	return line;
}

static llvm::StringRef
peekLine(llvm::StringRef content)
{
	while (!content.empty())
	{
		// We can consume here since we have a copy, not reference to content
		auto line = consumeLine(content).ltrim();

		if (!line.empty())
			return line;
	}

	return {};
}

static bool
lineStartsWithTag(llvm::StringRef content)
{
	return content.starts_with("@") || content.starts_with("\\");
}

static DoxygenTag
consumeTag(llvm::StringRef& content)
{
	content = content.ltrim();

	if (!content.starts_with('@') && !content.starts_with('\\'))
	{
		return DoxygenTag::NoTag;
	}

	// Drop the Tag character ('@' or '\')
	content = content.drop_front(1U).ltrim();

	for (const auto& [key, value] : SupportedDoxygenTags)
	{
		if (content.starts_with(value))
		{
			content = content.drop_front(value.size()).ltrim();

			return key;
		}
	}

	// Generically handle non-special tag (e.g., @see)
	return DoxygenTag::Generic;
}

static std::optional<DoxygenGenericTag>
consumeGeneric(llvm::StringRef& content)
{
	DoxygenGenericTag result;

	/* Default to header size 3 ('### Title' in markdown) */
	result.headingSize = 3U;

	/* We are passed everything after the '@' of @name. */
	auto line = consumeLine(content).trim();

	/* Find the first space after 'name' in '@name' (might not be present) */
	auto space = line.find(' ');

	/* Nothing else was on the same line that the tag started on, content
		probably below the @name tag line */

	if (std::string::npos == space)
	{
		result.name = line;
	}
	else
	{
		/* Found content on the same line as the 'tag'. Start collecting the content immediately */

		result.name	   = line.substr(0U, space).ltrim().str();
		result.content = line.substr(space + 1U).ltrim().str();
	}

	if (result.name.empty())
		return std::nullopt;

	/* Consume the rest of the content until we reach a tag or an empty line */

	while (true)
	{
		auto nextLine = peekLine(content);

		if (nextLine.empty() || lineStartsWithTag(nextLine))
			break;

		auto consumed = consumeLine(content);

		if (!consumed.empty())
		{
			result.content += consumed.str();
		}
	}

	return result;
}

static std::string
consumeBrief(llvm::StringRef& content)
{
	std::string retval;

	while (!content.empty())
	{
		auto nextLine = peekLine(content);

		if (nextLine.empty() || lineStartsWithTag(nextLine))
			break;

		auto consumed = consumeLine(content);

		if (consumed.empty())
			break;

		retval += consumed.str();
	}

	return retval;
}

static DoxygenParamTag
consumeParam(llvm::StringRef& content)
{
	DoxygenParamTag retval;

	/* We are passed everything directly after '@param' including the [in,out] specifier
		defined like '@param[in] name' */

	auto line = consumeLine(content);

	/* Check for specifiers [in], [out], [in:optional,out], etc. */
	if (line.starts_with("["))
	{
		auto end = line.find(']');

		if (end != llvm::StringRef::npos)
		{
			auto block = line.slice(1, end);
			line	   = line.drop_front(end + 1).ltrim();

			llvm::SmallVector<llvm::StringRef, 4> parts;

			block.split(parts, ',');

			for (llvm::StringRef part : parts)
			{
				auto trimmed = part.trim();
				auto colon	 = trimmed.find(':');

				llvm::StringRef direction = trimmed;
				llvm::StringRef qualifier;

				if (colon != llvm::StringRef::npos)
				{
					direction = trimmed.slice(0, colon).trim();
					qualifier = trimmed.drop_front(colon + 1).trim();
				}

				if (direction.equals_insensitive("in"))
					retval.specifiers = retval.specifiers | ParamSpecifier::IN;
				else if (direction.equals_insensitive("out"))
					retval.specifiers = retval.specifiers | ParamSpecifier::OUT;

				if (qualifier.equals_insensitive("optional"))
					retval.specifiers = retval.specifiers | ParamSpecifier::OPTIONAL;
			}
		}
	}

	/* Next token is 'name' of '@param[in] name Description' */

	auto space = line.find(' ');

	if (space == llvm::StringRef::npos)
	{
		retval.name = line;
	}
	else
	{
		retval.name = line.substr(0U, space).ltrim().str();
		retval.desc = line.substr(space + 1U).ltrim().str();
	}

	while (true)
	{
		auto nextLine = peekLine(content);

		if (nextLine.empty() || lineStartsWithTag(nextLine))
			break;

		auto consumed = consumeLine(content);

		if (!consumed.empty())
		{
			retval.desc += consumed.str();
		}
	}

	return retval;
}

static std::optional<DoxygenExampleTag>
consumeExample(llvm::StringRef& content)
{
	DoxygenExampleTag retval;

	if (content.starts_with("["))
	{
		auto end = content.find(']');

		if (end != llvm::StringRef::npos)
		{
			auto block = content.slice(1U, end);
			content	   = content.drop_front(end + 1U).trim();

			retval.language = block;
		}
	}

	auto openingBracket = content.find("{");

	if (openingBracket == llvm::StringRef::npos)
		return std::nullopt;

	auto closingBracket = content.rfind("}");

	if (openingBracket == llvm::StringRef::npos)
		return std::nullopt;

	auto block = content.slice(1U, closingBracket);
	content	   = content.drop_front(closingBracket + 1U).trim();

	retval.contents = block.str();

	return retval;
}

static std::optional<llvm::StringRef>
getBacktickQuoteRange(llvm::StringRef content, size_t offset)
{
	assert(content[offset] == '`');

	// The open-quote is usually preceded by whitespace.
	llvm::StringRef Prefix						   = content.substr(0, offset);
	constexpr llvm::StringLiteral BeforeStartChars = " \t(=";

	if (!Prefix.empty() && !BeforeStartChars.contains(Prefix.back()))
		return std::nullopt;

	// The quoted string must be nonempty and usually has no
	// leading/trailing ws.
	auto Next = content.find('`', offset + 1);

	if (Next == llvm::StringRef::npos)
		return std::nullopt;

	llvm::StringRef Contents = content.slice(offset + 1, Next);

	if (Contents.empty() || isWhitespace(Contents.front()) || isWhitespace(Contents.back()))
		return std::nullopt;

	// The close-quote is usually followed by whitespace or punctuation.
	llvm::StringRef Suffix						= content.substr(Next + 1);
	constexpr llvm::StringLiteral AfterEndChars = " \t)=.,;:";

	if (!Suffix.empty() && !AfterEndChars.contains(Suffix.front()))
		return std::nullopt;

	return content.slice(offset, Next + 1);
}

static void
appendTextOrCode(llvm::StringRef content, markup::Paragraph& pOutput)
{
	size_t pos = 0;

	while (pos < content.size())
	{
		// Find next backtick
		size_t tick = content.find('`', pos);

		if (tick == llvm::StringRef::npos)
		{
			// No more ticks: emit the remainder as plain text
			pOutput.appendText(content.substr(pos)).appendSpace();

			break;
		}

		// Try to see if this starts a real quoted range
		if (auto Range = getBacktickQuoteRange(content, tick))
		{
			// Emit text before the code span
			if (tick > pos)
				pOutput.appendText(content.substr(pos, tick - pos));

			// Emit the code span (trim the backticks)
			llvm::StringRef code = Range->trim("`");

			pOutput.appendCode(code, /*Preserve=*/true);

			// Advance past this entire range
			pos = tick + Range->size();
		}
		else
		{
			// Not a valid code span, treat this backtick as normal text
			pOutput.appendText(content.substr(pos, tick - pos + 1));

			pos = tick + 1;
		}
	}
}

static void
buildMarkdown(DoxygenData& data, markup::Document& pOutput)
{
	/* Append the brief (if present) */
	if (!data.brief.empty())
	{
		pOutput.addRuler();

		markup::Paragraph& paragraph = pOutput.addParagraph();

		appendTextOrCode(data.brief, paragraph);
	}

	/* Append random non-tagged user lines */
	if (!data.userLines.empty())
	{
		pOutput.addRuler();

		for (auto& line : data.userLines)
		{
			markup::Paragraph& paragraph = pOutput.addParagraph();

			appendTextOrCode(line, paragraph);
		}
	}

	/* Append generic tags we don't handle specially (such as @see) */
	if (!data.genericTags.empty())
	{
		pOutput.addRuler();

		for (auto& genericTag : data.genericTags)
		{
			/* Normalize name to lowercase */
			std::transform(genericTag.name.begin(), genericTag.name.end(), genericTag.name.begin(), ::tolower);

			/* Treat as proper noun and capitalize first letter */
			genericTag.name[0] = ::toupper(genericTag.name[0]);

			pOutput.addHeading(genericTag.headingSize).appendText(genericTag.name);

			markup::Paragraph& paragraph = pOutput.addParagraph();

			appendTextOrCode(genericTag.content, paragraph);
		}
	}

	/* Append all provided parameters */
	if (!data.params.empty())
	{
		pOutput.addRuler();

		pOutput.addHeading(3).appendText("Parameters");

		for (auto param : data.params)
		{
			markup::Paragraph& paragraph = pOutput.addParagraph();

			if (ParamSpecifier::NONE != param.specifiers)
			{
				bool in	 = (ParamSpecifier::IN == (param.specifiers & ParamSpecifier::IN));
				bool out = (ParamSpecifier::OUT == (param.specifiers & ParamSpecifier::OUT));
				bool opt = (ParamSpecifier::OPTIONAL == (param.specifiers & ParamSpecifier::OPTIONAL));

				if (in && out)
					paragraph.appendCode(opt ? "⇳" : "↕︎");
				else if (in)
					paragraph.appendCode(opt ? "⇣" : "↓");
				else if (out)
					paragraph.appendCode(opt ? "⇡" : "↑");

				paragraph.appendSpace();
			}

			paragraph.appendCode(param.name);
			paragraph.appendText(" → ");

			appendTextOrCode(param.desc, paragraph);

			pOutput.addParagraph();
		}
	}

	/* Append example code */
	if (auto const example = data.example)
	{
		pOutput.addRuler();

		pOutput.addHeading(3).appendText("Example");
		pOutput.addHeading(3).appendText("{");

		auto contents = llvm::StringRef((*example).contents).trim();
		std::string normalized;

		while (!contents.empty())
		{
			auto line = consumeLine(contents).trim();

			normalized += "\t" + line.str() + "\n";
		}

		pOutput.addCodeBlock(normalized, (*example).language);

		pOutput.addHeading(3).appendText("}");
	}

	pOutput.addParagraph();
}

} // namespace clang::clangd::c32
