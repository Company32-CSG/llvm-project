
#include "DoxygenTagParser.hpp"

#include "Hover.h"
#include "support/Markup.h"
#include "clang/Tooling/Core/Replacement.h"
#include "llvm/ADT/StringRef.h"

#include "clang/Format/Format.h"
#include "llvm/Support/Path.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace clang::clangd::c32 {

template <typename T>
static auto
reversed(T& container)
{
	struct ReversedRange
	{
		T& container;
		auto
		begin()
		{
			return container.rbegin();
		}

		auto
		end()
		{
			return container.rend();
		}
	};

	return ReversedRange{ container };
}

static std::string
indentLines(std::string& input)
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

static llvm::StringRef consumeLine(llvm::StringRef& pContent);
static llvm::StringRef peekLine(llvm::StringRef content);
static bool lineStartsWithTag(llvm::StringRef content);

static DoxygenTag consumeTag(llvm::StringRef& content);
static std::optional<DoxygenGenericTag> consumeGenericTag(llvm::StringRef& content);
static std::optional<std::string> consumeUntilNextTag(llvm::StringRef& content);
static DoxygenParamTag consumeParam(llvm::StringRef& content);
static std::pair<std::string, std::string> consumeRetval(llvm::StringRef& content);
static std::optional<DoxygenExampleTag> consumeExample(llvm::StringRef& content);

static std::optional<llvm::StringRef> getBacktickQuoteRange(llvm::StringRef Line, size_t Offset);
static void buildMarkdown(const HoverInfo* hoverInfo, DoxygenData& data, markup::Document& pOutput);

static const std::map<DoxygenTag, std::string> SupportedDoxygenTags = {
	{ DoxygenTag::Md, "md" },
	{ DoxygenTag::Markdown, "markdown" },
	{ DoxygenTag::Brief, "brief" },
	{ DoxygenTag::Param, "param" },
	{ DoxygenTag::Returns, "returns" },
	{ DoxygenTag::Retval, "retval" },
	{ DoxygenTag::Result, "result" },
	{ DoxygenTag::Example, "example" },
	{ DoxygenTag::Warning, "warning" },
	{ DoxygenTag::Deprecated, "deprecated" },
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
				auto generic = consumeGenericTag(comment);

				if (auto& value = generic)
					data.genericTags.push_back(std::move(*value));

				break;
			}

			case DoxygenTag::Md:
			case DoxygenTag::Markdown:
			{
				auto consumed = consumeUntilNextTag(comment);

				if (auto& value = consumed)
					data.rawMarkdown.push_back(std::move(*value));

				break;
			}

			case DoxygenTag::Brief:
			{
				auto consumed = consumeUntilNextTag(comment);

				if (auto& value = consumed)
					data.brief = std::move(*value);

				break;
			}

			case DoxygenTag::Param:
			{
				DoxygenParamTag param = consumeParam(comment);

				matchParamsToDoxygen(param);

				data.params.push_back(std::move(param));
				break;
			}

			case DoxygenTag::Returns:
			{
				auto consumed = consumeUntilNextTag(comment);

				if (auto& value = consumed)
					data.returns = std::move(*value);

				break;
			}

			case DoxygenTag::Retval:
			case DoxygenTag::Result:
			{
				auto [k, v] = consumeRetval(comment);

				data.retvals[k] = v;
				break;
			}

			case DoxygenTag::Example:
			{
				data.example = consumeExample(comment);

				break;
			}

			case DoxygenTag::Warning:
			{
				auto consumed = consumeUntilNextTag(comment);

				if (auto& value = consumed)
					data.warnings.push_back(std::move(*value));

				break;
			}

			case DoxygenTag::Deprecated:
			{
				auto consumed = consumeUntilNextTag(comment);

				if (auto& value = consumed)
					data.deprecation = std::move(*value);

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

	buildMarkdown(hoverInfo, data, pOutput);
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
consumeGenericTag(llvm::StringRef& content)
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

static std::optional<std::string>
consumeUntilNextTag(llvm::StringRef& content)
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

	if (0U == retval.size())
		return std::nullopt;

	return retval;
}

static DoxygenParamTag
consumeParam(llvm::StringRef& content)
{
	DoxygenParamTag retval;

	/* We are passed everything directly after '@param' including the [in,out] specifier
		defined like '@param[in] name' */

	auto line = consumeLine(content);

	/* Initialize the specifiers to NONE */
	retval.specifiers = ParamSpecifier::NONE;

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
					retval.specifiers |= ParamSpecifier::IN;
				else if (direction.equals_insensitive("out"))
					retval.specifiers |= ParamSpecifier::OUT;

				if (qualifier.equals_insensitive("optional"))
					retval.specifiers |= ParamSpecifier::OPTIONAL;
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

static std::pair<std::string, std::string>
consumeRetval(llvm::StringRef& content)
{
	std::pair<std::string, std::string> ret;

	/* We are passed everything directly after '@retval' which is usually a value and
		optionally a description. E.g., '@retval ERR_INVALID_ARGS Invalid arguments passed' */

	auto line = consumeLine(content).ltrim();

	/* First token is the name */

	auto space = line.find(' ');

	if (space == llvm::StringRef::npos)
	{
		ret.first = line.str();
	}
	else
	{
		ret.first  = line.substr(0U, space).ltrim().str();
		ret.second = line.substr(space + 1U).ltrim().str();
	}

	while (true)
	{
		auto nextLine = peekLine(content);

		if (nextLine.empty() || lineStartsWithTag(nextLine))
			break;

		auto consumed = consumeLine(content);

		if (!consumed.empty())
		{
			ret.second += consumed.str();
		}
	}

	return ret;
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

	auto block = content.slice(1U, closingBracket).trim();
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

static std::string
formatCodeBlock(const std::string& contents, const format::FormatStyle& style)
{
	tooling::Replacements replacements = reformat(style, contents, { tooling::Range(0, contents.size()) });

	llvm::Expected<std::string> formatted = tooling::applyAllReplacements(contents, replacements);

	if (formatted)
		return *formatted;
	else
		return contents;
}

static void
buildMarkdown(const HoverInfo* hoverInfo, DoxygenData& data, markup::Document& pOutput)
{
	/* Append the brief */
	if (!data.brief.empty())
	{
		markup::Paragraph& paragraph = pOutput.addParagraph();

		appendTextOrCode(data.brief, paragraph);
	}

	/* Pile on the rulers to make a thick divider after the brief/signature */
	pOutput.addRuler();
	pOutput.addRuler();
	pOutput.addRuler();
	pOutput.addRuler();

	/* Append warnings to the top of the hover */
	if (!data.warnings.empty())
	{
		bool bulletList = (data.warnings.size() > 1U);

		pOutput.addHeading(2).appendText("⚠️ Warning");

		for (auto& warning : data.warnings)
		{
			markup::Paragraph& paragraph = pOutput.addParagraph();

			if (bulletList)
			{
				paragraph.appendMarkdown("-");
				paragraph.appendSpace();
			}

			paragraph.appendMarkdown("_" + warning + "_");
		}
	}

	/* Append the deprecated warning */
	if (!data.deprecation.empty())
	{
		pOutput.addHeading(3).appendMarkdown("~~Deprecated~~");

		markup::Paragraph& paragraph = pOutput.addParagraph();

		appendTextOrCode(data.deprecation, paragraph);
	}

	/* Pile on the rulers to make a thick divider after the warnings/deprecation */
	if (!data.warnings.empty() || !data.deprecation.empty())
	{
		pOutput.addRuler();
		pOutput.addRuler();
		pOutput.addRuler();
		pOutput.addRuler();
	}

	/* Append random non-tagged user lines */
	if (!data.userLines.empty())
	{
		for (auto& line : data.userLines)
		{
			markup::Paragraph& paragraph = pOutput.addParagraph();

			appendTextOrCode(line, paragraph);
		}
	}

	/* Append random raw markdown lines */
	if (!data.rawMarkdown.empty())
	{
		for (auto& line : data.rawMarkdown)
		{
			pOutput.addParagraph().appendMarkdown(line);
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
				bool in	 = (ParamSpecifier::NONE != (param.specifiers & ParamSpecifier::IN));
				bool out = (ParamSpecifier::NONE != (param.specifiers & ParamSpecifier::OUT));
				bool opt = (ParamSpecifier::NONE != (param.specifiers & ParamSpecifier::OPTIONAL));

				if (in && out)
					paragraph.appendCode(opt ? "⇳" : "↕︎");
				else if (in)
					paragraph.appendCode(opt ? "⇣" : "↓");
				else if (out)
					paragraph.appendCode(opt ? "⇡" : "↑");

				paragraph.appendSpace();
			}

			paragraph.appendCode(param.name);
			paragraph.appendSpace();
			paragraph.appendText("→");
			paragraph.appendSpace();

			appendTextOrCode(param.desc, paragraph);

			pOutput.addParagraph();
		}
	}

	/* Append the return description (@returns) */
	if (!data.returns.empty())
	{
		pOutput.addRuler();

		pOutput.addHeading(3).appendText("Returns");

		markup::Paragraph& paragraph = pOutput.addParagraph();

		appendTextOrCode(data.returns, paragraph);
	}

	/* Append the collected @retval/@result tags */
	if (!data.retvals.empty())
	{
		/* Add the 'Returns' heading if the user didn't specify a @returns tag */
		if (data.returns.empty())
		{
			pOutput.addRuler();

			pOutput.addHeading(3).appendText("Returns");
		}

		for (const auto& [key, val] : reversed(data.retvals))
		{
			markup::Paragraph& paragraph = pOutput.addParagraph();

			paragraph.appendMarkdown("-");
			paragraph.appendSpace();
			paragraph.appendMarkdown("__`" + key + "`__");
			paragraph.appendSpace();
			paragraph.appendText("→");
			paragraph.appendSpace();

			appendTextOrCode(val, paragraph);
		}
	}

	/* Append example code */
	if (auto const example = data.example)
	{
		auto formatted = formatCodeBlock((*example).contents, (hoverInfo->Style));

		pOutput.addRuler();

		pOutput.addHeading(3).appendText("Example");

		pOutput.addParagraph().appendMarkdown("__{__");
		pOutput.addCodeBlock(indentLines(formatted), (*example).language);
		pOutput.addParagraph().appendMarkdown("__}__");
	}

	pOutput.addParagraph();
}

} // namespace clang::clangd::c32
