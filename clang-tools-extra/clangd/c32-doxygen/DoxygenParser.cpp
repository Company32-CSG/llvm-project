#include "DoxygenParser.hpp"
#include "Doxygen.hpp"
#include "Utils.hpp"
#include "support/Logger.h"

#include "clang/Format/Format.h"
#include "clang/Tooling/Core/Replacement.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace clang::clangd::c32::doxygen {

/* ------------------------------------------------------------ */

namespace {

struct ParsedTag {
  /// Concrete Doxygen tag type (aliases are resolved to concrete types)
  TagType Type;

  /// String following the tag initiator (e.g., `@` or `\`)
  std::string Name;

  /// Optional attributes specified in `[]` following the tag (e.g.,
  /// `@param[in]`)
  std::vector<std::string> Attributes;

  /// Body content of the tag
  std::string Body;
};

namespace predicate {

struct Flags {
  bool TAG : 1U;
  bool TAG_TERMINATOR : 1U;
  bool SPACE : 1U;
  bool BREAK : 1U;
  bool END : 1U;

  Flags()
      : TAG(false), TAG_TERMINATOR(false), SPACE(false), BREAK(false),
        END(false) {}

  Flags(bool TAG, bool TAG_TERMINATOR, bool SPACE, bool BREAK, bool END)
      : TAG(TAG), TAG_TERMINATOR(TAG_TERMINATOR), SPACE(SPACE), BREAK(BREAK),
        END(END) {}
};

const Flags Tag(true, false, false, false, true);
const Flags TagOrEnd(true, false, false, false, true);
const Flags Space(false, false, true, false, false);
const Flags BreakTagOrEnd(true, false, false, true, true);
const Flags SpaceTagOrEnd(true, false, true, true, true);

std::optional<size_t> execute(std::string_view SV, Flags &Flags) {
  std::optional<size_t> R = std::nullopt;

  auto TagFunc = [](std::string_view SV) -> std::optional<size_t> {
    for (size_t I = 0U; I < SV.size(); I++) {
      if (isEscaping(SV, I))
        continue;

      if (!isDoxygenTagInitiator(SV[I]))
        continue;

      if (auto T = getTag(SV, I, TagContext::Block)) {
        if (!T->Tag->Flags.Inline)
          return I;
      }
    }

    return std::nullopt;
  };

  auto TerminatingTagFunc = [](std::string_view SV) -> std::optional<size_t> {
    for (size_t I = 0U; I < SV.size(); I++) {
      if (isEscaping(SV, I))
        continue;

      if (!isDoxygenTagInitiator(SV[I]))
        continue;

      if (auto T = getTag(SV, I, TagContext::Block)) {
        if (T->Tag->Flags.Inline)
          continue;

        /* Consume the entire TAG this time! */
        return I + T->Consumed;
      }
    }

    return std::nullopt;
  };

  auto SpaceFunc = [](std::string_view SV) -> std::optional<size_t> {
    for (size_t I = 0U; I < SV.size(); I++) {
      if (std::isspace(SV[I]))
        return I;
    }

    return std::nullopt;
  };

  auto BlankLineFunc = [](std::string_view SV) -> std::optional<size_t> {
    for (size_t I = 0U; I + 1U < SV.size(); I++) {
      if ('\n' != SV[I])
        continue;

      /* Now, SV[I] is a newline character. Check if the line is empty */

      size_t J = I + 1U;

      /* Allow spaces, tabs, and CR characters; anything else is not a blank
       * line */
      while (J < SV.size() && (' ' == SV[J] || '\t' == SV[J] || '\r' == SV[J]))
        J++;

      /* If next character starts line, we found blank line. Return the position
              after our newline character. */

      if (J < SV.size() && '\n' == SV[J])
        return J + 1U;
    }

    return std::nullopt;
  };

  auto ConsiderFunc = [&](std::optional<size_t> Pos) {
    if (!Pos)
      return;

    if (!R || *Pos < *R)
      R = Pos;
  };

  if (Flags.TAG) {
    if (Flags.TAG_TERMINATOR)
      ConsiderFunc(TerminatingTagFunc(SV));
    else
      ConsiderFunc(TagFunc(SV));
  }

  if (Flags.SPACE) {
    ConsiderFunc(SpaceFunc(SV));
  }

  if (Flags.BREAK) {
    ConsiderFunc(BlankLineFunc(SV));
  }

  if (Flags.END) {
    if (!SV.empty())
      ConsiderFunc(SV.size());
  }

  return R;
}

} // namespace predicate

struct ConsumeContext {
  /// Original content without any changes
  std::string_view Original;

  /// Modified content that is advancing as its consumed
  std::string_view Working;

  /// Counter of how many characters have been consumed from `Working`
  size_t Offset;

  explicit ConsumeContext(std::string_view Original)
      : Original(Original), Working(Original), Offset(0U) {}

  /// Given a relative position in `Relative`, compute its index in `Original`.
  size_t absolutePosition(size_t Relative) const { return Offset + Relative; }

  /// Advance the \m working view by `Relative` chars (+1 if Strip==true), and
  /// update \m Offset.
  void advance(size_t Relative, bool Strip) {
    // how many chars to drop from the *front* of Working
    size_t Skip = Strip ? (Relative + 1) : Relative;

    // clamp so we never go past the end:
    if (Skip > Working.size())
      Skip = Working.size();

    Working.remove_prefix(Skip);

    Offset = Original.size() - Working.size();
  }
};

std::optional<std::string> consumeUntil(ConsumeContext &Context,
                                        predicate::Flags Predicate) {
  if (Context.Working.empty())
    return std::nullopt;

  auto Position = predicate::execute(Context.Working, Predicate);

  if (!Position)
    return std::nullopt;

  auto Piece = Context.Working.substr(0U, *Position);

  Context.advance(*Position, false);

  std::string FinalResult;

  FinalResult.reserve(Piece.size());

  for (size_t I = 0U; I < Piece.size(); I++) {
    if (!isEscaping(Piece, I) && isDoxygenTagInitiator(Piece[I])) {
      if (auto T = getTag(Piece, I, TagContext::Inline)) {
        /* Start just after the tag spelling (initiator + whitespace + name) */
        size_t Cursor = I + T->Consumed;

        /* Skip all whitespace after the tag (\t, \n, ' ', etc.) */
        while (Cursor < Piece.size() &&
               std::isspace(static_cast<unsigned char>(Piece[Cursor])))
          Cursor++;

        /* Capture the tag's argument following the tag */
        size_t IdentStart = Cursor;

        /* Find the end of the tag's value */
        while (Cursor < Piece.size() && !isTagTerminator(Piece[Cursor], true))
          Cursor += 1U;

        if (IdentStart < Cursor) {
          const char *C = "`";

          switch (T->Tag->Type) {
          case TagType::A:
            C = "*";
            break;

          case TagType::B:
            C = "**";
            break;

          case TagType::Member:
            FinalResult.append("__Member__ ");
            break;

          case TagType::P:
            FinalResult.append("__Param__ ");
            break;

          case TagType::Ref:
            FinalResult.append("__Ref__ ");
            break;

          default:
            break;
          }

          FinalResult.append(C);
          FinalResult.append(Piece.data() + IdentStart, Cursor - IdentStart);
          FinalResult.append(C);
        }

        /* Advance i manually so the main for-loop catches up to where we are
         * now */
        I = Cursor - 1U;

        continue;
      }
    }

    FinalResult += Piece[I];
  }

  return trim(FinalResult);
}

/**
 * @brief
 *    Consume content until explicit \c @end terminating tag is found.
 *
 * @param[in,out] Context
 *    Parse context; advances past the \c @end tag.
 *
 * @returns
 *    Content between current position and \c @end, or rest of context if \c
 * @end not found.
 */
std::optional<std::string> consumeUntilTerminatingTag(ConsumeContext &Context) {
  // Scan for the next block-level tag, looking specifically for @end
  for (size_t I = 0U; I < Context.Working.size(); I++) {
    if (isEscaping(Context.Working, I))
      continue;

    if (!isDoxygenTagInitiator(Context.Working[I]))
      continue;

    if (auto T = getTag(Context.Working, I, TagContext::Block)) {
      if (T->Tag->Flags.Inline)
        continue;

      // Found a block tag - extract content before it
      auto Piece = Context.Working.substr(0U, I);

      if (T->Tag->Type == TagType::End) {
        // Properly terminated block - advance past @end
        Context.advance(I + T->Consumed, false);

        elog("Found end tag! - returning: '{0}'", trim(Piece));

        return trim(Piece);
      }

      // Unexpected block tag (missing @end)
      // Still extract up to this tag for robustness
      Context.advance(I, false);
      return trim(Piece);
    }
  }

  // No terminating tag found - consume to EOF
  auto Piece = Context.Working;
  Context.advance(Context.Working.size(), false);
  return trim(Piece);
}

std::optional<std::string> consumeBetween(ConsumeContext &Context,
                                          std::string_view LHS,
                                          std::string_view RHS, bool Greedy,
                                          bool RequireAtStart) {
  auto LhsPos = Context.Working.find(LHS);

  if (std::string_view::npos == LhsPos)
    return std::nullopt;

  if (RequireAtStart && 0U != LhsPos)
    return std::nullopt;

  /* Start search after the `LHS` string */
  auto Start = LhsPos + LHS.size();

  /**
   *  Greedy = Last occurrence of `RHS`,
   * !Greedy = First occurrence of `RHS`
   */

  auto RhsPos =
      Greedy ? Context.Working.rfind(RHS) : Context.Working.find(RHS, Start);

  /// `RHS` position must be found AFTER the position of `LHS`
  if (std::string_view::npos == RhsPos || RhsPos < Start)
    return std::nullopt;

  auto Extracted = Context.Working.substr(Start, RhsPos - Start);

  Context.advance(RhsPos + RHS.size(), false);

  return trim(Extracted);
}

std::optional<ParsedTag> consumeTag(ConsumeContext &Content) {
  ParsedTag R;

  auto Tag = getTag(Content.Working, 0U, TagContext::Block);

  if (!Tag || Tag->Tag->Flags.Inline)
    return std::nullopt;

  /* Advance past to cause the tag to be consumed */
  Content.advance(Tag->Consumed, false);

  R.Type = Tag->Tag->Type;
  R.Name = properNounCase(Tag->Name);

  std::optional<std::string> ConsumedBody;

  if (Tag->Tag->Flags.HasTerminatingTag) {
    /* Use explicit @end terminator */
    ConsumedBody = consumeUntilTerminatingTag(Content);
  } else {
    auto Predicate = predicate::BreakTagOrEnd;

    /* Disable line-break detection */
    if (Tag->Tag->Flags.AllowLineBreaks)
      Predicate.BREAK = false;

    ConsumedBody = consumeUntil(Content, Predicate);
  }

  if (!ConsumedBody)
    return std::nullopt;

  auto TagBody = std::string_view(*ConsumedBody);

  ConsumeContext AttrContext(TagBody);

  /* Consume (optional) arguments located after name between the '[]' brackets
   */
  auto Attrs = consumeBetween(AttrContext, "[", "]", false, true);

  if (Attrs) {
    while (!Attrs->empty()) {
      auto Pos = Attrs->find(',');

      std::string A;

      if (Pos == std::string::npos) {
        A = trim(*Attrs);

        /* Remove any other content since we found the last comma */
        Attrs->clear();
      } else {
        A = trim(Attrs->substr(0U, Pos));

        /* Remove the attribute and the comma */
        Attrs->erase(0U, Pos + 1U);
      }

      if (!A.empty())
        R.Attributes.push_back(std::move(unescape(A)));
    }
  }

  R.Body = trim(AttrContext.Working);

  return R;
}

} // namespace

ParsedDoxygen parse(std::string_view Contents,
                    const format::FormatStyle &Style) {
  ConsumeContext Context(Contents);
  ParsedDoxygen Doxygen;

  while (auto Consumed = consumeUntil(Context, predicate::TagOrEnd)) {
    if (!Consumed->empty()) {
      auto Printed = unescape(*Consumed);
      auto Lines = split(Printed, "\n");

      Doxygen.UntaggedLines.insert(Doxygen.UntaggedLines.end(), Lines.begin(),
                                   Lines.end());
    }

    if (auto Tag = consumeTag(Context)) {
      switch (Tag->Type) {
      case TagType::Brief: {
        auto Unesc = unescape(Tag->Body);
        auto Canon = canonicalizeWhitespace(Unesc, true);

        if (Canon.empty())
          break;

        Doxygen.Brief = Canon;
        break;
      }

      case TagType::Code:
      case TagType::Example: {
        CodeExampleTag T;

        // tag->body already contains only the content between opener and @end
        auto PrintedCode = unescape(Tag->Body);

        if (Tag->Attributes.size() < 1U)
          T.Lang = "c";
        else
          T.Lang = Tag->Attributes.front();

        tooling::Replacements Replacements = reformat(
            Style, PrintedCode, {tooling::Range(0, PrintedCode.size())});

        llvm::Expected<std::string> Formatted =
            tooling::applyAllReplacements(PrintedCode, Replacements);

        if (Formatted)
          T.Code = *Formatted;
        else
          T.Code = PrintedCode;

        Doxygen.CodeExamples.push_back(std::move(T));
        break;
      }

      case TagType::Deprecated: {
        auto Unesc = unescape(Tag->Body);
        auto Canon = canonicalizeWhitespace(Unesc, true);

        if (Canon.empty())
          break;

        Doxygen.Deprecated = Canon;
        break;
      }

      case TagType::Param: {
        ParameterTag T;

        ConsumeContext TagContext(Tag->Body);

        auto Name = consumeUntil(TagContext, predicate::Space);

        if (!Name)
          break;

        auto Description = consumeUntil(TagContext, predicate::BreakTagOrEnd);

        T.Name = unescape(*Name);
        T.Description =
            Description ? canonicalizeWhitespace(unescape(*Description), true)
                        : "";
        T.Specifiers = ParameterTag::Specifier::None;

        for (const auto &Attr : Tag->Attributes) {
          auto Lower = lowercase(Attr);

          if ("in" == Lower)
            T.Specifiers |= ParameterTag::Specifier::In;

          if ("out" == Lower)
            T.Specifiers |= ParameterTag::Specifier::Out;

          /* Only set the OPT bit when at least one directional specifier bit is
           * set. */
          if (ParameterTag::Specifier::None != T.Specifiers) {
            if ("opt" == Lower)
              T.Specifiers |= ParameterTag::Specifier::Optional;
          }
        }

        Doxygen.Parameters.push_back(std::move(T));
        break;
      }

      case TagType::Returns: {
        auto Unesc = unescape(Tag->Body);
        auto Canon = canonicalizeWhitespace(Unesc, true);

        if (Canon.empty())
          break;

        Doxygen.Returns = Canon;
        break;
      }

      case TagType::Retval: {
        ConsumeContext TagContext(Tag->Body);

        auto Value = consumeUntil(TagContext, predicate::SpaceTagOrEnd);

        if (!Value)
          break;

        auto Description = consumeUntil(TagContext, predicate::TagOrEnd);

        Doxygen.Retvals[unescape(*Value)] =
            Description ? canonicalizeWhitespace(unescape(*Description), true)
                        : "";
        break;
      }

      case TagType::Throw: {
        ConsumeContext TagContext(Tag->Body);

        auto Value = consumeUntil(TagContext, predicate::SpaceTagOrEnd);

        if (!Value)
          break;

        auto Description = consumeUntil(TagContext, predicate::TagOrEnd);

        Doxygen.Throws[unescape(*Value)] =
            Description ? canonicalizeWhitespace(unescape(*Description), true)
                        : "";
        break;
      }

      case TagType::TParam: {
        ConsumeContext TagContext(Tag->Body);

        auto Value = consumeUntil(TagContext, predicate::SpaceTagOrEnd);

        if (!Value)
          break;

        auto Description = consumeUntil(TagContext, predicate::TagOrEnd);

        Doxygen.TParams[unescape(*Value)] =
            Description ? canonicalizeWhitespace(unescape(*Description), true)
                        : "";
        break;
      }

      case TagType::Version: {
        ConsumeContext TagContext(Tag->Body);

        auto Value = consumeUntil(TagContext, predicate::SpaceTagOrEnd);

        if (!Value)
          break;

        auto Description = consumeUntil(TagContext, predicate::TagOrEnd);

        Doxygen.Version.first = unescape(*Value);
        Doxygen.Version.second =
            Description ? canonicalizeWhitespace(unescape(*Description), true)
                        : "";

        break;
      }

      case TagType::Warning: {
        auto Unesc = unescape(Tag->Body);
        auto Canon = canonicalizeWhitespace(Unesc, true);

        if (Canon.empty())
          break;

        Doxygen.Warnings.push_back(Canon);
        break;
      }

      case TagType::Custom:
      default: {
        CustomTag T;

        T.Name = Tag->Name;
        T.Body = canonicalizeWhitespace(unescape(Tag->Body), true);

        Doxygen.CustomTags.push_back(std::move(T));
        break;
      }
      }
    }
  }

  return Doxygen;
}

} // namespace clang::clangd::c32::doxygen
