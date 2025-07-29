#include "DoxygenParser.hpp"
#include "Doxygen.hpp"
#include "Utils.hpp"
#include "to_string.hpp"

#include "../Hover.h"

#include "clang/Tooling/Core/Replacement.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include <cctype>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace clang::clangd::c32::doxygen {

/* ------------------------------------------------------------ */

namespace {

struct ParsedTag
{
	/// Concrete Doxygen tag type (aliases are resolved to concrete types)
	TagType type;

	/// String following the tag initiator (e.g., `@` or `\`)
	std::string name;

	/// Optional attributes specified in `[]` following the tag (e.g., `@param[in]`)
	std::vector<std::string> attributes;

	std::string body;
};

namespace predicate {

struct Flags
{
	bool TAG : 1U;
	bool TAG_TERMINATOR : 1U;
	bool SPACE : 1U;
	bool BREAK : 1U;
	bool END : 1U;

	Flags()
		: TAG(false), TAG_TERMINATOR(false), SPACE(false), BREAK(false), END(false) {}

	Flags(bool TAG, bool TAG_TERMINATOR, bool SPACE, bool BREAK, bool END)
		: TAG(TAG), TAG_TERMINATOR(TAG_TERMINATOR), SPACE(SPACE), BREAK(BREAK), END(END) {}
};

const Flags tag(true, false, false, false, true);
const Flags tagOrEnd(true, false, false, false, true);
const Flags space(false, false, true, false, false);
const Flags breakTagOrEnd(true, false, false, true, true);
const Flags spaceTagOrEnd(true, false, true, true, true);

inline std::optional<size_t>
execute(std::string_view sv, Flags& flags)
{

	auto tag = [](std::string_view sv) -> std::optional<size_t>
	{
		for (size_t i = 0U; i < sv.size(); i++)
		{
			if (isEscaping(sv, i))
				continue;

			if (!isDoxygenTagInitiator(sv[i]))
				continue;

			if (auto t = getTag(sv, i, TagContext::Block))
			{
				if (!t->tag->flags.Inline)
					return i;
			}
		}

		return std::nullopt;
	};

	auto terminatingTag = [](std::string_view sv) -> std::optional<size_t>
	{
		for (size_t i = 0U; i < sv.size(); i++)
		{
			if (isEscaping(sv, i))
				continue;

			if (!isDoxygenTagInitiator(sv[i]))
				continue;

			if (auto t = getTag(sv, i, TagContext::Block))
			{
				if (t->tag->flags.Inline)
					continue;

				/* Consume the entire TAG this time! */
				return i + t->consumed;
			}
		}

		return std::nullopt;
	};

	auto space = [](std::string_view sv) -> std::optional<size_t>
	{
		for (size_t i = 0U; i < sv.size(); i++)
		{
			if (std::isspace(sv[i]))
				return i;
		}

		return std::nullopt;
	};

	auto blankLine = [](std::string_view sv) -> std::optional<size_t>
	{
		for (size_t i = 0U; i + 1U < sv.size(); i++)
		{
			if ('\n' != sv[i])
				continue;

			/* Now, sv[i] is a newline character. Check if the line is empty */

			size_t j = i + 1U;

			/* Allow spaces, tabs, and CR characters; anything else is not a blank line */
			while (j < sv.size() && (' ' == sv[j] || '\t' == sv[j] || '\r' == sv[j]))
				j++;

			/* If next character starts line, we found blank line. Return the position
				after our newline character. */

			if (j < sv.size() && '\n' == sv[j])
				return j + 1U;
		}

		return std::nullopt;
	};

	if (flags.TAG)
	{
		std::optional<size_t> ret = std::nullopt;

		if (flags.TAG_TERMINATOR)
			ret = terminatingTag(sv);
		else
			ret = tag(sv);

		if (auto p = ret)
			return *p;
	}

	if (flags.BREAK)
	{
		if (auto p = blankLine(sv))
			return *p;
	}

	if (flags.SPACE)
	{
		if (auto p = space(sv))
			return *p;
	}

	if (flags.END)
	{
		if (!sv.empty())
			return sv.size();
	}

	return std::nullopt;
}

} // namespace predicate

struct ConsumeContext
{
	/// Original content without any changes
	std::string_view original;

	/// Modified content that is advancing as its consumed
	std::string_view working;

	/// Counter of how many characters have been consumed from `working`
	size_t offset;

	explicit ConsumeContext(std::string_view original) : original(original), working(original), offset(0U) {}

	/// Given a relative position in `relative`, compute its index in `original`.
	size_t
	absolutePosition(size_t relative) const
	{
		return offset + relative;
	}

	/// Advance the `working` view by `relative` chars (+1 if strip==true), and update `base`.
	void
	advance(size_t relative, bool strip)
	{
		// how many chars to drop from the *front* of working
		size_t skip = strip ? (relative + 1) : relative;

		// clamp so we never go past the end:
		if (skip > working.size())
			skip = working.size();

		working.remove_prefix(skip);
		offset = original.size() - working.size();
	}
};

std::optional<std::string>
consumeUntil(ConsumeContext& context, predicate::Flags predicate)
{
	if (context.working.empty())
		return std::nullopt;

	auto position = predicate::execute(context.working, predicate);

	if (!position)
		return std::nullopt;

	auto piece = context.working.substr(0U, *position);

	context.advance(*position, false);

	std::string finalResult;

	finalResult.reserve(piece.size());

	for (size_t i = 0U; i < piece.size(); i++)
	{
		if (!isEscaping(piece, i) && isDoxygenTagInitiator(piece[i]))
		{
			if (auto tag = getTag(piece, i, TagContext::Inline))
			{
				/* Start just after the tag spelling (initiator + whitespace + name) */
				size_t cursor = i + tag->consumed;

				/* Skip all whitespace after the tag (\t, \n, ' ', etc.) */
				while (cursor < piece.size() && std::isspace(static_cast<unsigned char>(piece[cursor])))
					cursor++;

				/* Capture the tag's argument following the tag */
				size_t ident_start = cursor;

				/* Find the end of the tag's value */
				while (cursor < piece.size() && !isTagTerminator(piece[cursor]))
					cursor += 1U;

				if (ident_start < cursor)
				{
					switch (tag->tag->type)
					{
						case TagType::Member:
							finalResult.append("__Member__ ");
							break;

						case TagType::P:
							finalResult.append("__Param__ ");
							break;

						case TagType::Ref:
							finalResult.append("__Ref__ ");
							break;

						default:
							break;
					}

					finalResult.push_back('`');
					finalResult.append(piece.data() + ident_start, cursor - ident_start);
					finalResult.push_back('`');
				}

				/* Advance i manually so the main for-loop catches up to where we are now */
				i = cursor - 1U;

				continue;
			}
		}

		finalResult += piece[i];
	}

	return trim(finalResult);
}

std::optional<std::string>
consumeBetween(ConsumeContext& context, std::string_view lhs, std::string_view rhs, bool greedy, bool requireAtStart)
{
	auto lhsPos = context.working.find(lhs);

	if (std::string_view::npos == lhsPos)
		return std::nullopt;

	if (requireAtStart && 0U != lhsPos)
		return std::nullopt;

	/* Start search after the `lhs` string */
	auto start = lhsPos + lhs.size();

	/**
	 *  Greedy = Last occurrence of `rhs`,
	 * !Greedy = First occurrence of `rhs`
	 */

	auto rhsPos = greedy ? context.working.rfind(rhs) : context.working.find(rhs, start);

	/* `rhs` position must be found AFTER the position of `lhs` */
	if (std::string_view::npos == rhsPos || rhsPos < start)
		return std::nullopt;

	auto extracted = context.working.substr(start, rhsPos - start);

	context.advance(rhsPos + rhs.size(), false);

	return trim(extracted);
}

std::optional<ParsedTag>
consumeTag(ConsumeContext& context)
{
	ParsedTag ret;

	auto tag = getTag(context.working, 0U, TagContext::Block);

	if (!tag || tag->tag->flags.Inline)
		return std::nullopt;

	/* Advance past to cause the tag to be consumed */
	context.advance(tag->consumed, false);

	ret.type = tag->tag->type;
	ret.name = properNounCase(tag->name);

	auto predicate = predicate::tagOrEnd;

	if (tag->tag->flags.HasTerminatingTag)
		predicate.TAG_TERMINATOR = true;

	if (tag->tag->flags.AllowLineBreaks)
		predicate.BREAK = true;

	auto consumedBody = consumeUntil(context, predicate);

	if (!consumedBody)
		return std::nullopt;

	auto tagBody = std::string_view(*consumedBody);

	ConsumeContext attrContext(tagBody);

	/* Consume (optional) arguments located after name between the '[]' brackets */
	auto attrs = consumeBetween(attrContext, "[", "]", false, true);

	if (attrs)
	{
		while (!attrs->empty())
		{
			auto pos = attrs->find(',');

			std::string a;

			if (pos == std::string::npos)
			{
				a = trim(*attrs);

				/* Remove any other content since we found the last comma */
				attrs->clear();
			}
			else
			{
				a = trim(attrs->substr(0U, pos));

				/* Remove the attribute and the comma */
				attrs->erase(0U, pos + 1U);
			}

			if (!a.empty())
				ret.attributes.push_back(std::move(unescape(a)));
		}
	}

	ret.body = trim(attrContext.working);

	return ret;
}

} // namespace

ParsedDoxygen
parse(const HoverInfo& info)
{
	ConsumeContext context(info.Documentation);
	ParsedDoxygen  doxygen;

	while (auto consumed = consumeUntil(context, predicate::tagOrEnd))
	{
		if (!consumed->empty())
		{
			auto printed = unescape(*consumed);
			auto lines	 = split(printed, "\n");

			doxygen.untaggedLines.insert(doxygen.untaggedLines.end(), lines.begin(), lines.end());
		}

		if (auto tag = consumeTag(context))
		{
			switch (tag->type)
			{
				case TagType::Brief:
				{
					auto unesc = unescape(tag->body);
					auto canon = canonicalizeWhitespace(unesc, true);

					if (canon.empty())
						break;

					doxygen.brief = canon;
					break;
				}

				case TagType::Code:
				{
					CodeTag t;

					ConsumeContext tagContext(tag->body);

					auto code = consumeUntil(tagContext, predicate::tagOrEnd);

					llvm::errs() << "<><><><><><><> No code!\n";
					if (!code)
						break;

					llvm::errs() << "<><><><><><><><><><><><> Code: '" << *code << "'\n";

					auto printedCode = unescape(*code);

					if (tag->attributes.size() < 1U)
						t.lang = "c";
					else
						t.lang = tag->attributes.front();

					tooling::Replacements replacements = reformat(info.Style, printedCode, { tooling::Range(0, printedCode.size()) });

					llvm::Expected<std::string> formatted = tooling::applyAllReplacements(printedCode, replacements);

					if (formatted)
						t.code = *formatted;
					else
						t.code = printedCode;

					doxygen.codeBlocks.push_back(std::move(t));
					break;
				}

				case TagType::Deprecated:
				{
					auto unesc = unescape(tag->body);
					auto canon = canonicalizeWhitespace(unesc, true);

					if (canon.empty())
						break;

					doxygen.deprecated = canon;
					break;
				}

				case TagType::Example:
				{
					ExampleTag t;

					ConsumeContext tagContext(tag->body);

					auto code = consumeBetween(tagContext, "{", "}", true, false);

					if (!code)
						break;

					auto printedCode = unescape(*code);

					if (tag->attributes.size() < 1U)
						t.lang = "c";
					else
						t.lang = tag->attributes.front();

					tooling::Replacements replacements = reformat(info.Style, printedCode, { tooling::Range(0, printedCode.size()) });

					llvm::Expected<std::string> formatted = tooling::applyAllReplacements(printedCode, replacements);

					if (formatted)
						t.code = *formatted;
					else
						t.code = printedCode;

					doxygen.examples.push_back(std::move(t));
					break;
				}

				case TagType::Param:
				{
					ParameterTag t;

					ConsumeContext tagContext(tag->body);

					auto name = consumeUntil(tagContext, predicate::space);

					if (!name)
						break;

					auto description = consumeUntil(tagContext, predicate::breakTagOrEnd);

					t.name		  = unescape(*name);
					t.description = description ? canonicalizeWhitespace(unescape(*description), true) : "";
					t.specifiers  = ParameterTag::Specifier::None;

					for (const auto& attr : tag->attributes)
					{
						auto lower = lowercase(attr);

						if ("in" == lower)
							t.specifiers |= ParameterTag::Specifier::In;

						if ("out" == lower)
							t.specifiers |= ParameterTag::Specifier::Out;

						/* Only set the OPT bit when at least one directional specifier bit is set. */
						if (ParameterTag::Specifier::None != t.specifiers)
						{
							if ("opt" == lower || "optional" == lower)
								t.specifiers |= ParameterTag::Specifier::Optional;
						}
					}

					doxygen.parameters.push_back(std::move(t));
					break;
				}

				case TagType::Returns:
				{
					auto unesc = unescape(tag->body);
					auto canon = canonicalizeWhitespace(unesc, true);

					if (canon.empty())
						break;

					doxygen.returns = canon;
					break;
				}

				case TagType::Retval:
				{
					ConsumeContext tagContext(tag->body);

					auto value = consumeUntil(tagContext, predicate::spaceTagOrEnd);

					if (!value)
						break;

					auto description = consumeUntil(tagContext, predicate::tagOrEnd);

					doxygen.retvals[unescape(*value)] = description ? canonicalizeWhitespace(unescape(*description), true) : "";
					break;
				}

				case TagType::Warning:
				{
					auto unesc = unescape(tag->body);
					auto canon = canonicalizeWhitespace(unesc, true);

					if (canon.empty())
						break;

					doxygen.warnings.push_back(canon);
					break;
				}

				case TagType::Custom:
				default:
				{
					CustomTag t;

					t.name = tag->name;
					t.body = canonicalizeWhitespace(unescape(tag->body), true);

					doxygen.customTags.push_back(std::move(t));
					break;
				}
			}
		}
	}

	return doxygen;
}

} // namespace clang::clangd::c32::doxygen
