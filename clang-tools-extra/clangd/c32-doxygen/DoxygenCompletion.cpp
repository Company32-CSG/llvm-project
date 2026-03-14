#include "DoxygenCompletion.hpp"

#include "Doxygen.hpp"
#include "Protocol.h"
#include "SourceCode.h"
#include "Utils.hpp"

#include "../CodeComplete.h"
#include "c32-doxygen/Markdown.hpp"
#include "support/Logger.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/Support/Casting.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::doxygen {

namespace {

// MARK: - Helper Types

enum class ContextKind
{
	Tag,
	ParamName,
	ParamAttr,
	Reference,
	MemberReference,
	None
};

struct CompletionContext
{
	/// Specific context we need to provide completion for (tag, param name, etc.)
	ContextKind kind;

	/// The character that triggered the autocomplete (e.g., `^@`, `^\`, `^%`)
	char triggerCharacter;

	/// Position where the trigger character (`^@`, `^\`) was found
	size_t triggerPos;

	/// Offset of the user's cursor
	size_t cursorPos;

	/// Content the user has already typed
	std::string prefix;

	/// Matched tag if parsed
	std::optional<MatchedTag> matchedTag;

	/// Begin position for LSP to insert our suggestion
	size_t replaceBegin;

	/// End position for LSP to insert our suggestion
	size_t replaceEnd;

	/// Existing attributes found when \m kind is \r ContextKind::ParamAttr
	std::vector<std::string> existingAttrs;
};

struct AttrBounds
{
	/// Position at which the `[` character was found
	size_t open;

	/// Position at which the `]` character was found; \ref std::string_view::npos if not found
	size_t close;
};

// MARK: - Helper Functions

std::pair<size_t, char>
rfindClosestTagInitiator(std::string_view contents, size_t offset)
{
	std::pair<size_t, char> ret = { std::string_view::npos, ' ' };

	for (const auto initiator : getAllTagInitiators())
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

inline bool
isSpace(const char c)
{
	return 0 != std::isspace(static_cast<unsigned char>(c));
}

inline size_t
skipSpace(std::string_view sv, size_t off)
{
	while (off < sv.size() && isSpace(sv[off]))
		off += 1U;

	return off;
}

std::optional<AttrBounds>
findAttrBounds(std::string_view sv, size_t offset)
{
	if (offset < sv.size() && '[' == sv[offset])
	{
		size_t pos	 = sv.find(']', offset + 1U);
		size_t close = (std::string_view::npos == pos) ? sv.size() : pos;

		return AttrBounds{ offset, close };
	}

	return std::nullopt;
}

std::vector<std::string>
splitAttrs(std::string_view sv)
{
	std::vector<std::string> ret;

	size_t pos = 0U;

	while (pos < sv.size())
	{
		auto next = sv.find(',', pos);
		auto part = trim(sv.substr(pos, (std::string_view::npos == next) ? (sv.size() - pos) : (next - pos)));

		if (!part.empty())
			ret.push_back(part);

		if (std::string_view::npos == next)
			break;

		pos += next + 1U;
	}

	return ret;
}

markdown::Document
buildItemDoc(const DoxygenTag& tag)
{
	markdown::Document output;

	output.heading(3U)
		.text("💡");

	output.paragraph()
		.text(std::string(tag.description));

	return output;
}

std::vector<CodeCompletion>
buildTagItems(const DoxygenTag& tag, char initiator, const Range& completionRange)
{
	std::vector<CodeCompletion> ret;

	ret.reserve(tag.aliases.size() + 1U);

	CodeCompletion& item = ret.emplace_back();

	item.Name				  = initiator + std::string(tag.name);
	item.FilterText			  = item.Name;
	item.Kind				  = CompletionItemKind::Property;
	item.Documentation		  = buildItemDoc(tag);
	item.CompletionTokenRange = completionRange;

	CodeCompletion aliasItem = item;

	for (const auto& alias : tag.aliases)
	{
		aliasItem.Name		 = initiator + std::string(alias);
		aliasItem.FilterText = item.Name;

		ret.push_back(aliasItem);
	}

	return ret;
}

const FunctionDecl*
findOwningFunctionDecl(const ParsedAST* ast, size_t cursorOffset)
{
	typedef std::function<const FunctionDecl*(const DeclContext*)> fLookupFunc;

	if (nullptr == ast)
		return nullptr;

	auto& Ctx = ast->getASTContext();
	auto& SM  = Ctx.getSourceManager();

	FileID mainID = SM.getMainFileID();

	SourceLocation cursorLoc = SM.getLocForStartOfFile(mainID).getLocWithOffset(static_cast<int>(cursorOffset));

	fLookupFunc lookup = [&](const DeclContext* DC) -> const FunctionDecl*
	{
		for (auto* D : DC->decls())
		{
			/* Is this a function? */
			if (auto* FD = llvm::dyn_cast<FunctionDecl>(D))
			{
				/* Does it have an attached raw comment? */
				if (auto* RC = Ctx.getRawCommentForDeclNoCache(FD))
				{
					auto SR = RC->getSourceRange();

					if (SR.isValid() && SM.isWrittenInSameFile(SR.getBegin(), cursorLoc) && cursorLoc >= SR.getBegin() && cursorLoc <= SR.getEnd())
					{
						return FD;
					}
				}
			}

			/* If it's a namespace (or any other nested context), recurse. */
			if (auto* ND = llvm::dyn_cast<DeclContext>(D))
			{
				if (auto* Found = lookup(ND))
					return Found;
			}
		}

		return nullptr;
	};

	/* Start the search from the translation unit (the root). */
	return lookup(Ctx.getTranslationUnitDecl());
}

const RecordDecl*
findOwningRecordDecl(const ParsedAST* ast, size_t cursorOffset)
{
	if (ast == nullptr)
		return nullptr;

	auto& Ctx = ast->getASTContext();
	auto& SM  = Ctx.getSourceManager();

	FileID		   mainID = SM.getMainFileID();
	SourceLocation cursorLoc =
		SM.getLocForStartOfFile(mainID).getLocWithOffset(static_cast<int>(cursorOffset));

	auto containsCursor = [&](SourceRange SR) -> bool
	{
		if (!SR.isValid())
			return false;

		if (!SM.isWrittenInSameFile(SR.getBegin(), cursorLoc))
			return false;

		return cursorLoc >= SR.getBegin() && cursorLoc <= SR.getEnd();
	};

	auto enclosingRecord = [](const Decl* D) -> const RecordDecl*
	{
		if (D == nullptr)
			return nullptr;

		/* If the decl itself is a record, return it directly. */
		if (const auto* RD = llvm::dyn_cast<RecordDecl>(D))
			return RD;

		/* Otherwise walk upward through DeclContext to find nearest record. */
		const DeclContext* DC = D->getDeclContext();
		while (DC != nullptr)
		{
			if (const auto* RD = llvm::dyn_cast<RecordDecl>(DC))
				return RD;

			DC = DC->getParent();
		}

		return nullptr;
	};

	std::function<const RecordDecl*(const DeclContext*)> lookup =
		[&](const DeclContext* DC) -> const RecordDecl*
	{
		for (const Decl* D : DC->decls())
		{
			/* Check whether this declaration owns a raw comment containing the cursor. */
			if (const auto* RC = Ctx.getRawCommentForDeclNoCache(D))
			{
				if (containsCursor(RC->getSourceRange()))
					return enclosingRecord(D);
			}

			/* Recurse into nested declaration contexts. */
			if (const auto* nestedDC = llvm::dyn_cast<DeclContext>(D))
			{
				if (const RecordDecl* found = lookup(nestedDC))
					return found;
			}
		}

		return nullptr;
	};

	return lookup(Ctx.getTranslationUnitDecl());
}

// MARK: - Context Builder

static std::optional<CompletionContext>
buildContext(std::string_view contents, size_t cursorOffset)
{
	auto [tagPos, initiator] = rfindClosestTagInitiator(contents, cursorOffset);

	if (std::string_view::npos == tagPos)
		return std::nullopt;

	/// Check if cursorOffset is adjacent to the end of tagPos.
	auto isTagAdjacent = [&, &tagPos = tagPos, &initiator = initiator]()
	{
		bool   inSpace = false;
		size_t nSpaces = 0U;

		/* The '%' initiator for references doesn't allow ANY spaces */
		if ('%' == initiator)
		{
			for (size_t i = tagPos; i < cursorOffset; i++)
			{
				if (isSpace(contents[i]))
					return false;
			}

			return true;
		}

		/* Ensure there is only a single space after the tag and no spaces after the argument */
		for (size_t i = tagPos; i < cursorOffset; i++)
		{
			const char c = contents[i];

			if (isSpace(c))
			{
				if (inSpace)
					continue;

				inSpace = true;
				nSpaces += 1U;

				if (nSpaces > 1U)
					return false;

				continue;
			}

			inSpace = false;
		}

		return true;
	};

	/**
	 * Ensure the cursor is directly to the right of the tag
	 * 		(e.g., '@someTag |' NOT '@someTag <existing arg> |')
	 */

	if (!isTagAdjacent())
		return std::nullopt;

	CompletionContext context;

	context.triggerCharacter = initiator;
	context.triggerPos		 = tagPos;
	context.cursorPos		 = cursorOffset;
	context.replaceBegin	 = cursorOffset;
	context.replaceEnd		 = cursorOffset;

	/// Helper to set the prefix and replacement range and trim whitespace
	/// @param[in] begin
	///			The beginning position of the range
	/// @param[in] end
	///			The ending position of the range
	auto setPrefixRange = [&](size_t begin, size_t end)
	{
		if (end < begin)
			end = begin;

		auto rb = begin;

		while (rb < end && isSpace(contents[rb]))
			rb += 1U;

		auto re = end;

		while (re > rb && isSpace(contents[re - 1U]))
			re -= 1U;

		context.prefix		 = contents.substr(rb, re - rb);
		context.replaceBegin = rb;
		context.replaceEnd	 = re;
	};

	/* Attempt to match the tag */
	if (auto tag = getTag(contents.substr(tagPos), 0U, TagContext::Any))
	{
		size_t after = tagPos + tag->consumed;

		context.matchedTag = *tag;

		/* Handle the reference initiator shortcut ('%') since there is no delimiter (no space after '%') */
		if (tag->tag->type == TagType::Ref && tag->initiator == '%')
		{
			context.kind = ContextKind::Reference;

			setPrefixRange(after, cursorOffset);

			return context;
		}

		/// Check if we are still typing the tag name itself
		/// @retval true
		/// 		We are still typing the tag name itself
		/// @retval false
		/// 		We have moved past the tag name and are typing the body/arguments
		auto findTagBodyDelimiter = [&]() -> bool
		{
			for (size_t i = after; i < cursorOffset; i++)
			{
				const char c = contents[i];

				if (isSpace(c))
					return true;

				if (tag->tag->type == TagType::Param && c == '[')
					return true;
			}

			return false;
		};

		/* Check if we are completing the tag itself */
		if (!findTagBodyDelimiter())
		{
			context.kind = ContextKind::Tag;

			setPrefixRange(tagPos + 1U, cursorOffset);

			return context;
		}

		/* We have a complete tag; see if we can offer completion for the tag's body */
		switch (tag->tag->type)
		{
			case TagType::P:
			{
				size_t nameStart = skipSpace(contents, after);

				context.kind = ContextKind::ParamName;

				if (cursorOffset <= nameStart)
				{
					setPrefixRange(cursorOffset, cursorOffset);

					return context;
				}

				setPrefixRange(nameStart, cursorOffset);

				return context;
			}

			case TagType::Param:
			{
				size_t p = skipSpace(contents, after);

				if (auto bounds = findAttrBounds(contents, p))
				{
					/** We are inside `[...]` */
					if (cursorOffset > bounds->open && cursorOffset <= bounds->close)
					{
						size_t searchStart = bounds->open + 1U;
						size_t lastComma   = contents.rfind(',', cursorOffset ? cursorOffset - 1U : 0U);

						if (std::string_view::npos == lastComma || lastComma < searchStart)
							lastComma = searchStart - 1U;

						size_t tokStart = lastComma + 1U;

						while (tokStart < cursorOffset && isSpace(contents[tokStart]))
							tokStart += 1U;

						context.kind = ContextKind::ParamAttr;

						setPrefixRange(tokStart, cursorOffset);

						auto rawList = contents.substr(bounds->open + 1U, bounds->close - (bounds->open + 1U));

						context.existingAttrs = splitAttrs(rawList);

						return context;
					}

					/// Skip past the entire `[...]` attribute block
					p = skipSpace(contents, (bounds->close < contents.size()) ? bounds->close + 1U : bounds->close);
				}

				context.kind = ContextKind::ParamName;

				/** Now, `p` should be at the param name (or whitespace before it) */
				if (cursorOffset <= p)
				{
					setPrefixRange(cursorOffset, cursorOffset);

					return context;
				}

				setPrefixRange(p, cursorOffset);

				return context;
			}

			case TagType::Ref:
			{
				size_t p = skipSpace(contents, after);

				context.kind = ContextKind::Reference;

				if (cursorOffset <= p)
				{
					setPrefixRange(cursorOffset, cursorOffset);

					return context;
				}

				setPrefixRange(p, cursorOffset);

				return context;
			}

			case TagType::Member:
			{
				size_t p = skipSpace(contents, after);

				context.kind = ContextKind::MemberReference;

				if (cursorOffset <= p)
				{
					setPrefixRange(cursorOffset, cursorOffset);

					return context;
				}

				setPrefixRange(p, cursorOffset);

				return context;
			}

			default:
				return std::nullopt;
		}
	}

	/* Could not match tag; assume we are completing the tag itself */

	context.kind = ContextKind::Tag;

	setPrefixRange(tagPos + 1U, cursorOffset);

	return context;
}

// MARK: - Completion Builder

CodeCompleteResult
completeTags(std::string_view contents, size_t cursorOffset, std::string_view prefix, size_t tagPos, char initiator)
{
	CodeCompleteResult ret;

	Range tokenRange = { offsetToPosition(contents, tagPos), offsetToPosition(contents, cursorOffset) };

	/* No prefix typed by user: return all tags */
	if (prefix.empty())
	{
		for (const auto& tag : getAllTags())
		{
			auto tags = buildTagItems(tag, initiator, tokenRange);

			ret.Completions.insert(ret.Completions.end(), tags.begin(), tags.end());
		}

		return ret;
	}

	/* With a prefix, loop through and find all matching tags */
	for (const auto& tag : getAllTags())
	{
		/* Does it match the tag name? */
		if (0U == tag.name.rfind(prefix, 0U))
		{
			auto tags = buildTagItems(tag, initiator, tokenRange);

			ret.Completions.insert(ret.Completions.end(), tags.begin(), tags.end());
			continue;
		}

		/// Does it match one of the tag's aliases?
		for (const auto& alias : tag.aliases)
		{
			if (0U == alias.rfind(prefix, 0U))
			{
				auto tags = buildTagItems(tag, initiator, tokenRange);

				ret.Completions.insert(ret.Completions.end(), tags.begin(), tags.end());
				break;
			}
		}
	}

	return ret;
}

CodeCompleteResult
completeParamAttrs(const CompletionContext& context, std::string_view contents)
{
	static constexpr const char* Candidates[] = { "in", "out", "opt" };

	CodeCompleteResult ret;

	auto alreadyContains = [&](std::string_view s)
	{
		return llvm::is_contained(context.existingAttrs, s);
	};

	if (context.prefix.empty())
	{
		for (const auto* candidate : Candidates)
		{
			CodeCompletion c;

			c.Name		 = candidate;
			c.FilterText = candidate;
			c.Kind		 = CompletionItemKind::EnumMember;

			c.CompletionTokenRange = {
				offsetToPosition(contents, context.replaceBegin),
				offsetToPosition(contents, context.replaceEnd)
			};

			ret.Completions.push_back(std::move(c));
		}

		return ret;
	}

	for (const auto* candidate : Candidates)
	{
		/* Check if this attribute is already in the attribute list */
		if (alreadyContains(candidate))
			continue;

		/* Does the prefix typed by the user match this candidate? */
		if (0U == std::string_view(candidate).rfind(context.prefix, 0U))
		{
			CodeCompletion c;

			c.Name		 = candidate;
			c.FilterText = candidate;
			c.Kind		 = CompletionItemKind::EnumMember;

			c.CompletionTokenRange = {
				offsetToPosition(contents, context.replaceBegin),
				offsetToPosition(contents, context.replaceEnd)
			};

			ret.Completions.push_back(std::move(c));
		}
	}

	return ret;
}

CodeCompleteResult
completeParamNames(const CompletionContext& context, std::string_view contents, const ParsedAST* ast)
{
	CodeCompleteResult ret;

	if (nullptr == ast)
		return ret;

	const FunctionDecl* func = findOwningFunctionDecl(ast, context.cursorPos);

	if (nullptr == func)
		return ret;

	// TODO: Filter out already documented parameters

	std::string_view prefix = context.prefix;

	if (prefix.empty())
	{
		for (const auto* p : func->parameters())
		{
			/* Ensure we have a name */

			if (!p->getIdentifier())
				continue;

			std::string name = p->getName().str();

			CodeCompletion c;

			c.Name		 = name;
			c.FilterText = name;
			c.Kind		 = CompletionItemKind::Variable;

			c.CompletionTokenRange = {
				offsetToPosition(contents, context.replaceBegin),
				offsetToPosition(contents, context.replaceEnd)
			};

			ret.Completions.push_back(std::move(c));
		}

		return ret;
	}

	for (const auto* p : func->parameters())
	{
		/* Ensure we have a name */

		if (!p->getIdentifier())
			continue;

		std::string name = p->getName().str();

		if (!prefix.empty() && 0U != name.rfind(prefix, 0U))
			continue;

		CodeCompletion c;

		c.Name		 = name;
		c.FilterText = name;
		c.Kind		 = CompletionItemKind::Variable;

		c.CompletionTokenRange = {
			offsetToPosition(contents, context.replaceBegin),
			offsetToPosition(contents, context.replaceEnd)
		};

		ret.Completions.push_back(std::move(c));
	}

	return ret;
}

CodeCompleteResult
completeReferences(const CompletionContext& context, const CodeCompleteArgs& args)
{
	CodeCompleteResult ret;

	if (!context.matchedTag && '%' != context.triggerCharacter)
		return ret;

	auto patchedContents = std::string(args.contents);

	auto findCommentStart = [&]()
	{
		auto maxp = [&](size_t a, size_t b)
		{
			if (std::string_view::npos == a)
				return b;

			if (std::string_view::npos == b)
				return a;

			return (a > b) ? (a) : (b);
		};

		size_t s1 = patchedContents.rfind("/**", args.offset), s2 = patchedContents.rfind("/*!", args.offset), s3 = patchedContents.rfind("///", args.offset), s4 = patchedContents.rfind("//!", args.offset);

		return maxp(maxp(s1, s2), maxp(s3, s4));
	};

	auto commentStart = findCommentStart();

	if (std::string_view::npos == commentStart)
		return ret;

	for (size_t i = commentStart; i < context.replaceBegin; i++)
	{
		if (!isSpace(patchedContents[i]))
			patchedContents[i] = ' ';
	}

	if ('%' == context.triggerCharacter)
	{
		patchedContents[context.triggerPos] = ' ';
	}

	auto commentEnd = patchedContents.find("*/", args.offset);

	if (std::string_view::npos != commentEnd)
	{
		for (size_t i = commentEnd + 1U; i >= context.replaceEnd; i--)
		{
			if (!isSpace(patchedContents[i]))
				patchedContents[i] = ' ';
		}
	}

	return codeCompleteFlowHook(args.fileName, args.offset, args.preamble, args.parseInput, args.opts, args.specFuzzyFind, patchedContents);
}

CodeCompleteResult
completeMemberReferences(const CompletionContext& context, std::string_view contents, const ParsedAST* ast)
{
	CodeCompleteResult ret;

	if (nullptr == ast)
		return ret;

	const RecordDecl* record = findOwningRecordDecl(ast, context.cursorPos);

	if (nullptr == record)
	{
		elog("No owning record found for member reference completion: '{0}'", context.prefix);
		return ret;
	}

	const std::string_view prefix = context.prefix;

	for (const FieldDecl* field : record->fields())
	{
		if (nullptr == field->getIdentifier())
			continue;

		std::string name = field->getName().str();

		if (!prefix.empty() && name.rfind(prefix, 0U) != 0U)
			continue;

		CodeCompletion c;

		c.Name				   = name;
		c.FilterText		   = name;
		c.Kind				   = CompletionItemKind::Field;
		c.CompletionTokenRange = {
			offsetToPosition(contents, context.replaceBegin),
			offsetToPosition(contents, context.replaceEnd)
		};

		ret.Completions.push_back(std::move(c));
	}

	return ret;
}

} // namespace

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

	/** We are on a '///' or '//!' line */
	if (startsWithThreeSlash || startsWithExclSlash)
		return true;

	/* Not in a Doxygen comment */
	return false;
}

bool
shouldRunCompletion(std::string_view contents, size_t cursorOffset, std::string_view triggerCharacter)
{
	auto context = buildContext(contents, cursorOffset);
	if (!context)
		return false;

	const char trigger = triggerCharacter.empty() ? '\0' : triggerCharacter.front();
	const bool manual  = ('\0' == trigger);

	const bool lastIsSpace	 = (cursorOffset > 0U) && isSpace(contents[cursorOffset - 1U]);
	const bool emptyPrefix	 = context->prefix.empty();
	const bool finishedToken = (!emptyPrefix && lastIsSpace);

	switch (context->kind)
	{
		case ContextKind::Tag:
		{
			return manual || (!triggerCharacter.empty() && isDoxygenTagInitiator(trigger));
		}

		case ContextKind::ParamAttr:
		{
			if (manual)
				return true;

			if (',' == trigger)
				return true;

			if ('[' == trigger)
				return true;

			if (' ' == trigger)
				return true;

			return !finishedToken;
		}

		case ContextKind::ParamName:
		{
			if (manual)
				return true;

			if (' ' == trigger)
				return emptyPrefix;

			return !finishedToken;
		}

		case ContextKind::Reference:
		{
			if (manual)
				return true;

			if ('%' == trigger)
				return true;

			if (' ' == trigger)
				return emptyPrefix;

			return !finishedToken;
		}

		case ContextKind::MemberReference:
		{
			if (manual)
				return true;

			if ('%' == trigger)
				return true;

			if (' ' == trigger)
				return emptyPrefix;

			return !finishedToken;
		}

		default:
			return false;
	}
}

CodeCompleteResult
completion(const CodeCompleteArgs& args)
{
	CodeCompleteResult empty;

	auto context = buildContext(args.contents, args.offset);

	if (!context)
		return empty;

	switch (context->kind)
	{
		case ContextKind::Tag:
		{
			auto [tagPos, ini] = rfindClosestTagInitiator(args.contents, args.offset);

			return completeTags(args.contents, args.offset, context->prefix, tagPos, ini);
		}

		case ContextKind::ParamAttr:
			return completeParamAttrs(*context, args.contents);

		case ContextKind::ParamName:
			return completeParamNames(*context, args.contents, args.ast);

		case ContextKind::Reference:
			return completeReferences(*context, args);

		case ContextKind::MemberReference:
			return completeMemberReferences(*context, args.contents, args.ast);

		default:
			return empty;
	}
}

} // namespace clang::clangd::c32::doxygen
