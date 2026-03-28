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
    // MARK: - Lightbulb Doxygen Tags
    {TagType::A, TagParsingFlags(true, false, false), "a",
     "Render the argument following this tag in italics."},
    {TagType::B, TagParsingFlags(true, false, false), "b",
     "Render the argument following this tag in bold."},
    {TagType::Brief, TagParsingFlags(false, true), "brief",
     "Summary of documented symbol."},
    {TagType::C, TagParsingFlags(true, false, false), "c",
     "Render the argument following this tag in an inline code block."},
    {TagType::Code, TagParsingFlags(false, true, true), "code",
     "Marks the beginning of a code block. @code[<lang>] ... @end"},
    {TagType::End, TagParsingFlags(false, false), "end",
     "Marks the end of block."},
    {TagType::Deprecated, TagParsingFlags(false, false), "deprecated",
     "Mark usage of the documented symbol as deprecated."},
    {TagType::Example,
     TagParsingFlags(false, true, true),
     "example",
     "Example usage code block: @example[<lang>] ... @end",
     {"usage"}},
    {TagType::Member,
     TagParsingFlags(true, false),
     "member",
     "Reference a member of the current class or struct.",
     {"m"}},
    {TagType::P,
     TagParsingFlags(true, false),
     "p",
     "Reference to a parameter defined by the @param tag.",
     {"pref"}},
    {TagType::Param, TagParsingFlags(false, false), "param",
     "Function parameter. Supports [in], [out], [in,out:optional], etc."},
    {TagType::Ref,
     TagParsingFlags(true, false),
     "ref",
     "Reference to a defined symbol.",
     {"r"}},
    {TagType::Returns,
     TagParsingFlags(false, false),
     "returns",
     "Description of return value.",
     {"return"}},
    {TagType::Retval,
     TagParsingFlags(false, false),
     "retval",
     "Description of a specific return value.",
     {"ret", "result"}},
    {TagType::Throw,
     TagParsingFlags(false, false),
     "throw",
     "Documents an exception a function may throw.",
     {"throws"}},
    {TagType::TParam,
     TagParsingFlags(false, false),
     "tparam",
     "Describes a template parameter.",
     {"templateparam"}},
    {TagType::Version,
     TagParsingFlags(false, false),
     "version",
     "Specifies version information.",
     {"available", "availability"}},
    {TagType::Warning, TagParsingFlags(false, false), "warning",
     "Provide a warning to anyone using the documented symbol."},

    // MARK: - Unhandled Doxygen Tags
    {TagType::Custom, TagParsingFlags(false, true), "note",
     "Additional notes or commentary."},
    {TagType::Custom, TagParsingFlags(false, false), "attention",
     "Highlights something that needs attention."},
    {TagType::Custom, TagParsingFlags(false, false), "author",
     "Specifies the author of the code or documentation."},
    {TagType::Custom, TagParsingFlags(false, false), "copyright",
     "Specifies copyright details."},
    {TagType::Custom, TagParsingFlags(false, false), "date",
     "Specifies the date of the documentation or change."},
    {TagType::Custom, TagParsingFlags(false, true), "details",
     "Provides detailed documentation following @brief."},
    {TagType::Custom, TagParsingFlags(false, false), "exception",
     "Documents an exception that may be thrown."},
    {TagType::Custom, TagParsingFlags(false, false), "ingroup",
     "Associates a symbol with a documentation group."},
    {TagType::Custom, TagParsingFlags(false, false), "li",
     "Represents a list item inside @par or similar sections."},
    {TagType::Custom, TagParsingFlags(false, false), "mainpage",
     "Specifies the main page content of the documentation."},
    {TagType::Custom, TagParsingFlags(false, false), "name",
     "Sets the name of a group or section."},
    {TagType::Custom, TagParsingFlags(false, false), "par",
     "Starts a paragraph block."},
    {TagType::Custom, TagParsingFlags(false, false), "post",
     "Postcondition for a function or method."},
    {TagType::Custom, TagParsingFlags(false, false), "pre",
     "Precondition for a function or method."},
    {TagType::Custom, TagParsingFlags(false, true), "remark",
     "Provides an additional remark or observation."},
    {TagType::Custom, TagParsingFlags(false, false), "see",
     "Cross-reference to another documented entity."},
    {TagType::Custom, TagParsingFlags(false, false), "since",
     "Documents when the symbol was added."},
    {TagType::Custom, TagParsingFlags(false, false), "todo",
     "Marks something that needs to be completed."},
    {TagType::Custom,
     TagParsingFlags(false, false),
     "tparam",
     "Describes a template parameter.",
     {"templateparam"}},
    {TagType::Custom, TagParsingFlags(false, false), "section",
     "Defines a named documentation section."},
    {TagType::Custom, TagParsingFlags(false, false), "subsection",
     "Defines a subsection inside a section."},
    {TagType::Custom, TagParsingFlags(false, true), "verbatim",
     "Begins a raw text block."},
    {TagType::Custom, TagParsingFlags(false, false), "endverbatim",
     "Ends a raw text block."},
    {TagType::Custom, TagParsingFlags(false, false), "defgroup",
     "Defines a named documentation group."},
    {TagType::Custom, TagParsingFlags(false, false), "addtogroup",
     "Adds symbols to an existing documentation group."},
    {TagType::Custom, TagParsingFlags(false, false), "anchor",
     "Marks a location for cross-referencing."},
    {TagType::Custom, TagParsingFlags(false, false), "link",
     "Starts an inline link to a documented symbol."},
    {TagType::Custom, TagParsingFlags(false, false), "endlink",
     "Ends an inline link block."},
    {TagType::Custom, TagParsingFlags(false, false), "htmlonly",
     "Section only rendered in HTML output."},
    {TagType::Custom, TagParsingFlags(false, false), "endhtmlonly",
     "Ends HTML-only section."},
    {TagType::Custom, TagParsingFlags(false, false), "latexonly",
     "Section only rendered in LaTeX output."},
    {TagType::Custom, TagParsingFlags(false, false), "endlatexonly",
     "Ends LaTeX-only section."},
};

constexpr char TagInitiatorList[] = {'@', '\\', '%'};

// MARK: - Functions

const llvm::ArrayRef<DoxygenTag> getAllTags() {
  return llvm::ArrayRef(TagList);
}

const std::vector<std::string_view> getAllTagNames() {
  std::vector<std::string_view> R;

  size_t Size = TagList.size();

  for (const auto &T : TagList)
    Size += T.Aliases.size();

  R.reserve(Size);

  for (const auto &T : TagList) {
    R.push_back(T.Name);

    R.insert(R.end(), T.Aliases.begin(), T.Aliases.end());
  }

  return R;
}

const llvm::ArrayRef<char> getAllTagInitiators() {
  return llvm::ArrayRef(TagInitiatorList, std::size(TagInitiatorList));
}

const DoxygenTag *getTagByType(TagType Type) {
  for (const auto &T : TagList) {
    if (T.Type == Type)
      return &T;
  }

  return nullptr;
}

bool isDoxygenTagInitiator(std::string_view Contents) {
  for (const auto &INI : TagInitiatorList) {
    if (0U == Contents.rfind(INI, 0U))
      return true;
  }

  return false;
}

bool isDoxygenTagInitiator(char C) {
  for (const auto &INI : TagInitiatorList) {
    if (INI == C)
      return true;
  }

  return false;
}

const DoxygenTag *getDoxygenTagByName(std::string_view Name) {
  auto Lower = lowercase(Name);

  for (const auto &T : TagList) {
    if (T.Name == Lower)
      return &T;

    for (const auto &Alias : T.Aliases) {
      if (Alias == Lower)
        return &T;
    }
  }

  return nullptr;
}

std::pair<size_t, char> findClosestTagInitiator(std::string_view Contents) {
  std::pair<size_t, char> R = {std::string_view::npos, ' '};

  for (const auto INI : TagInitiatorList) {
    size_t P = Contents.find(INI);

    if (std::string_view::npos != P) {
      if (R.first == std::string_view::npos) {
        R.first = P;
        R.second = INI;
      } else if (R.first < P) {
        R.first = P;
        R.second = INI;
      }
    }
  }

  return R;
}

bool isEscaping(std::string_view Contents, size_t Offset) {
  if (0U == Offset || Offset >= Contents.size())
    return false;

  return ('^' == Contents[Offset - 1U]);
}

bool isDoxygenEscape(const char C) { return ('^' == C); }

std::string unescape(std::string_view SV) {
  std::string R;

  R.reserve(SV.size());

  bool Escape = false;

  for (size_t I = 0U; I < SV.size(); I++) {
    if (isDoxygenEscape(SV[I]) && !Escape) {
      Escape = true;

      continue;
    }

    R += SV[I];
    Escape = false;
  }

  return R;
}

bool tagStartsLine(std::string_view SV, size_t Pos) {
  if (Pos >= SV.size())
    return false;

  auto LineStart = SV.rfind('\n', Pos);

  if (std::string_view::npos == LineStart)
    LineStart = 0U;
  else
    LineStart += 1U;

  if (LineStart == Pos)
    return true;

  for (const auto C : SV.substr(LineStart, Pos - LineStart)) {
    if (!(std::isspace(static_cast<unsigned char>(C))))
      return false;
  }

  return true;
}

std::optional<MatchedTag> getTag(std::string_view SV, size_t Pos,
                                 TagContext Context) {
  MatchedTag R;

  auto InitialPosition = Pos;

  if (Pos >= SV.size())
    return std::nullopt;

  if (0U != Pos && isDoxygenEscape(SV[Pos - 1U]))
    return std::nullopt;

  if (!isDoxygenTagInitiator(SV[Pos]))
    return std::nullopt;

  R.INI = SV[Pos];

  /* Advance past the tag initiator character */
  Pos += 1U;

  /* Handle the ref initiator specially */
  if ('%' == R.INI) {
    if (TagContext::Inline != Context && TagContext::Any != Context)
      return std::nullopt;

    /* Loop through and find the REF tag descriptor */
    for (const auto &T : TagList) {
      if (TagType::Ref == T.Type)
        R.Tag = &T;
    }

    R.Name = R.Tag->Name;
    R.Consumed = 1U;

    return R;
  }

  /* Advance past whitespace */

  while (Pos < SV.size() && std::isspace(static_cast<unsigned char>(SV[Pos])))
    Pos += 1U;

  for (const auto &T : TagList) {
    if (TagContext::Any != Context) {
      bool InlineOnly = (TagContext::Inline == Context);
      bool BlockOnly = (TagContext::Block == Context);

      if ((InlineOnly && !T.Flags.Inline) || (BlockOnly && T.Flags.Inline))
        continue;
    }

    /* Check for aliases first */
    for (const auto &Alias : T.Aliases) {
      size_t NameLen = Alias.size();

      if (0 != SV.compare(Pos, NameLen, Alias))
        continue;

      auto After = Pos + NameLen;

      if (After != SV.size() && !isTagTerminator(SV[After], T.Flags.Inline))
        continue;

      R.Tag = &T;
      R.Name = Alias;
      R.Consumed = After - InitialPosition;

      return R;
    }

    /* Now check the canonical tag name */
    size_t NameLen = T.Name.size();

    if (0 != SV.compare(Pos, NameLen, T.Name))
      continue;

    auto After = Pos + NameLen;

    if (After != SV.size() && !isTagTerminator(SV[After], T.Flags.Inline))
      continue;

    R.Tag = &T;
    R.Name = T.Name;
    R.Consumed = After - InitialPosition;

    return R;
  }

  return std::nullopt;
}

bool isTagTerminator(char C, bool IsInline) {
  unsigned char UC = static_cast<unsigned char>(C);

  bool OK = (std::isalnum(UC) || '_' == C || ':' == C || ';' == C);

  if (IsInline)
    OK |= ('(' == C || ')' == C || '[' == C || ']' == C || '{' == C ||
           '}' == C || '^' == C || '\\' == C || '@' == C);

  return (false == OK);
}

} // namespace clang::clangd::c32::doxygen
