#include "c32-doccam/DoccamCompletion.hpp"
#include "c32-doccam/Doccam.hpp"
#include "c32-doccam/Utils.hpp"

#include "CodeComplete.h"
#include "Protocol.h"
#include "SourceCode.h"
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

namespace clang::clangd::c32::doccam {

namespace {

// MARK: - Helper Types

enum class ContextKind {
  Tag,
  ParamName,
  ParamAttr,
  Reference,
  MemberReference,
  None
};

struct CompletionContext {
  /// Specific context we need to provide completion for (tag, param name, etc.)
  ContextKind Kind;

  /// The character that triggered the autocomplete (e.g., `^@`, `^\`, `^%`)
  char TriggerCharacter;

  /// Position where the trigger character (`^@`, `^\`) was found
  size_t TriggerPos;

  /// Offset of the user's cursor
  size_t CursorPos;

  /// Content the user has already typed
  std::string Prefix;

  /// Matched tag if parsed
  std::optional<MatchedTag> Matched;

  /// Begin position for LSP to insert our suggestion
  size_t ReplaceBegin;

  /// End position for LSP to insert our suggestion
  size_t ReplaceEnd;

  /// Existing attributes found when \m kind is \r ContextKind::ParamAttr
  std::vector<std::string> ExistingAttrs;
};

struct AttrBounds {
  /// Position at which the `[` character was found
  size_t Open;

  /// Position at which the `]` character was found; \ref std::string_view::npos
  /// if not found
  size_t Close;
};

// MARK: - Helper Functions

std::pair<size_t, char> rfindClosestTagInitiator(std::string_view Contents,
                                                 size_t Offset) {
  std::pair<size_t, char> R = {std::string_view::npos, ' '};

  for (const auto INI : getAllTagInitiators()) {
    size_t Pos = Contents.rfind(INI, Offset);

    if (std::string_view::npos != Pos) {
      if (R.first == std::string_view::npos) {
        R.first = Pos;
        R.second = INI;
      } else if (R.first < Pos) {
        R.first = Pos;
        R.second = INI;
      }
    }
  }

  return R;
}

inline bool isSpace(const char C) {
  return 0 != std::isspace(static_cast<unsigned char>(C));
}

inline size_t skipSpace(std::string_view SV, size_t Off) {
  while (Off < SV.size() && isSpace(SV[Off]))
    Off += 1U;

  return Off;
}

std::optional<AttrBounds> findAttrBounds(std::string_view SV, size_t Offset) {
  if (Offset < SV.size() && '[' == SV[Offset]) {
    size_t Pos = SV.find(']', Offset + 1U);
    size_t Close = (std::string_view::npos == Pos) ? SV.size() : Pos;

    return AttrBounds{Offset, Close};
  }

  return std::nullopt;
}

std::vector<std::string> splitAttrs(std::string_view SV) {
  std::vector<std::string> R;

  size_t Pos = 0U;

  while (Pos < SV.size()) {
    auto Next = SV.find(',', Pos);
    auto Part =
        trim(SV.substr(Pos, (std::string_view::npos == Next) ? (SV.size() - Pos)
                                                             : (Next - Pos)));

    if (!Part.empty())
      R.push_back(Part);

    if (std::string_view::npos == Next)
      break;

    Pos = Next + 1U;
  }

  return R;
}

std::vector<CodeCompletion> buildTagItems(const DoccamTag &T, char INI,
                                          const Range &CompletionRange) {
  std::vector<CodeCompletion> R;

  R.reserve(T.Aliases.size() + 1U);

  CodeCompletion &Item = R.emplace_back();

  Item.Name = INI + std::string(T.Name);
  Item.FilterText = Item.Name;
  Item.Kind = CompletionItemKind::Property;
  Item.RawDocumentation = "💡 " + std::string(T.Description);
  Item.CompletionTokenRange = CompletionRange;

  CodeCompletion AliasItem = Item;

  for (const auto &Alias : T.Aliases) {
    AliasItem.Name = INI + std::string(Alias);
    AliasItem.FilterText = Item.Name;

    R.push_back(AliasItem);
  }

  return R;
}

const FunctionDecl *findOwningFunctionDecl(const ParsedAST *AST,
                                           size_t CursorOffset) {
  typedef std::function<const FunctionDecl *(const DeclContext *)> fLookupFunc;

  if (nullptr == AST)
    return nullptr;

  auto &Ctx = AST->getASTContext();
  auto &SM = Ctx.getSourceManager();

  FileID MainID = SM.getMainFileID();

  SourceLocation CursorLoc = SM.getLocForStartOfFile(MainID).getLocWithOffset(
      static_cast<int>(CursorOffset));

  fLookupFunc Lookup = [&](const DeclContext *DC) -> const FunctionDecl * {
    for (auto *D : DC->decls()) {
      /* Is this a function? */
      if (auto *FD = llvm::dyn_cast<FunctionDecl>(D)) {
        /* Does it have an attached raw comment? */
        if (auto *RC = Ctx.getRawCommentForDeclNoCache(FD)) {
          auto SR = RC->getSourceRange();

          if (SR.isValid() &&
              SM.isWrittenInSameFile(SR.getBegin(), CursorLoc) &&
              CursorLoc >= SR.getBegin() && CursorLoc <= SR.getEnd()) {
            return FD;
          }
        }
      }

      /* If it's a namespace (or any other nested context), recurse. */
      if (auto *ND = llvm::dyn_cast<DeclContext>(D)) {
        if (auto *Found = Lookup(ND))
          return Found;
      }
    }

    return nullptr;
  };

  /* Start the search from the translation unit (the root). */
  return Lookup(Ctx.getTranslationUnitDecl());
}

const RecordDecl *findOwningRecordDecl(const ParsedAST *AST,
                                       size_t CursorOffset) {
  if (AST == nullptr)
    return nullptr;

  auto &Ctx = AST->getASTContext();
  auto &SM = Ctx.getSourceManager();

  FileID MainID = SM.getMainFileID();
  SourceLocation CursorLoc = SM.getLocForStartOfFile(MainID).getLocWithOffset(
      static_cast<int>(CursorOffset));

  auto ContainsCursor = [&](SourceRange SR) -> bool {
    if (!SR.isValid())
      return false;

    if (!SM.isWrittenInSameFile(SR.getBegin(), CursorLoc))
      return false;

    return CursorLoc >= SR.getBegin() && CursorLoc <= SR.getEnd();
  };

  auto EnclosingRecord = [](const Decl *D) -> const RecordDecl * {
    if (D == nullptr)
      return nullptr;

    /* If the decl itself is a record, return it directly. */
    if (const auto *RD = llvm::dyn_cast<RecordDecl>(D))
      return RD;

    /* Otherwise walk upward through DeclContext to find nearest record. */
    const DeclContext *DC = D->getDeclContext();
    while (DC != nullptr) {
      if (const auto *RD = llvm::dyn_cast<RecordDecl>(DC))
        return RD;

      DC = DC->getParent();
    }

    return nullptr;
  };

  std::function<const RecordDecl *(const DeclContext *)> Lookup =
      [&](const DeclContext *DC) -> const RecordDecl * {
    for (const Decl *D : DC->decls()) {
      /* Check whether this declaration owns a raw comment containing the
       * cursor. */
      if (const auto *RC = Ctx.getRawCommentForDeclNoCache(D)) {
        if (ContainsCursor(RC->getSourceRange()))
          return EnclosingRecord(D);
      }

      /* Recurse into nested declaration contexts. */
      if (const auto *NestedDC = llvm::dyn_cast<DeclContext>(D)) {
        if (const RecordDecl *Found = Lookup(NestedDC))
          return Found;
      }
    }

    return nullptr;
  };

  return Lookup(Ctx.getTranslationUnitDecl());
}

// MARK: - Context Builder

static std::optional<CompletionContext> buildContext(std::string_view Contents,
                                                     size_t CursorOffset) {
  auto [TagPos, initiator] = rfindClosestTagInitiator(Contents, CursorOffset);

  if (std::string_view::npos == TagPos)
    return std::nullopt;

  /// Check if CursorOffset is adjacent to the end of TagPos.
  auto IsTagAdjacent = [&, &TagPos = TagPos, &INI = initiator]() {
    bool InSpace = false;
    size_t NumSpaces = 0U;

    /* The '%' initiator for references doesn't allow ANY spaces */
    if ('%' == INI) {
      for (size_t I = TagPos; I < CursorOffset; I++) {
        if (isSpace(Contents[I]))
          return false;
      }

      return true;
    }

    /* Ensure there is only a single space after the tag and no spaces after the
     * argument */
    for (size_t I = TagPos; I < CursorOffset; I++) {
      const char C = Contents[I];

      if (isSpace(C)) {
        if (InSpace)
          continue;

        InSpace = true;
        NumSpaces += 1U;

        if (NumSpaces > 1U)
          return false;

        continue;
      }

      InSpace = false;
    }

    return true;
  };

  /**
   * Ensure the cursor is directly to the right of the tag
   * 		(e.g., '@someTag |' NOT '@someTag <existing arg> |')
   */

  if (!IsTagAdjacent())
    return std::nullopt;

  CompletionContext Context;

  Context.TriggerCharacter = initiator;
  Context.TriggerPos = TagPos;
  Context.CursorPos = CursorOffset;
  Context.ReplaceBegin = CursorOffset;
  Context.ReplaceEnd = CursorOffset;

  /// Helper to set the prefix and replacement range and trim whitespace
  /// @param[in] Begin
  ///			The beginning position of the range
  /// @param[in] End
  ///			The ending position of the range
  auto SetPrefixRange = [&](size_t Begin, size_t End) {
    if (End < Begin)
      End = Begin;

    auto RB = Begin;

    while (RB < End && isSpace(Contents[RB]))
      RB += 1U;

    auto RE = End;

    while (RE > RB && isSpace(Contents[RE - 1U]))
      RE -= 1U;

    Context.Prefix = Contents.substr(RB, RE - RB);
    Context.ReplaceBegin = RB;
    Context.ReplaceEnd = RE;
  };

  /* Attempt to match the tag */
  if (auto Tag = getTag(Contents.substr(TagPos), 0U, TagContext::Any)) {
    size_t After = TagPos + Tag->Consumed;

    Context.Matched = *Tag;

    /* Handle the reference initiator shortcut ('%') since there is no delimiter
     * (no space after '%') */
    if (Tag->Tag->Type == TagType::Ref && Tag->INI == '%') {
      Context.Kind = ContextKind::Reference;

      SetPrefixRange(After, CursorOffset);

      return Context;
    }

    /// Check if we are still typing the tag name itself
    /// @retval true
    /// 		We are still typing the tag name itself
    /// @retval false
    /// 		We have moved past the tag name and are typing the
    /// body/arguments
    auto FindTagBodyDelimiter = [&]() -> bool {
      for (size_t I = After; I < CursorOffset; I++) {
        const char C = Contents[I];

        if (isSpace(C))
          return true;

        if (Tag->Tag->Type == TagType::Param && C == '[')
          return true;
      }

      return false;
    };

    /* Check if we are completing the tag itself */
    if (!FindTagBodyDelimiter()) {
      Context.Kind = ContextKind::Tag;

      SetPrefixRange(TagPos + 1U, CursorOffset);

      return Context;
    }

    /* We have a complete tag; see if we can offer completion for the tag's body
     */
    switch (Tag->Tag->Type) {
    case TagType::P: {
      size_t NameStart = skipSpace(Contents, After);

      Context.Kind = ContextKind::ParamName;

      if (CursorOffset <= NameStart) {
        SetPrefixRange(CursorOffset, CursorOffset);

        return Context;
      }

      SetPrefixRange(NameStart, CursorOffset);

      return Context;
    }

    case TagType::Param: {
      size_t P = skipSpace(Contents, After);

      if (auto Bounds = findAttrBounds(Contents, P)) {
        /** We are inside `[...]` */
        if (CursorOffset > Bounds->Open && CursorOffset <= Bounds->Close) {
          size_t SearchStart = Bounds->Open + 1U;
          size_t LastComma =
              Contents.rfind(',', CursorOffset ? CursorOffset - 1U : 0U);

          if (std::string_view::npos == LastComma || LastComma < SearchStart)
            LastComma = SearchStart - 1U;

          size_t TokStart = LastComma + 1U;

          while (TokStart < CursorOffset && isSpace(Contents[TokStart]))
            TokStart += 1U;

          Context.Kind = ContextKind::ParamAttr;

          SetPrefixRange(TokStart, CursorOffset);

          auto RawList = Contents.substr(Bounds->Open + 1U,
                                         Bounds->Close - (Bounds->Open + 1U));

          Context.ExistingAttrs = splitAttrs(RawList);

          return Context;
        }

        /// Skip past the entire `[...]` attribute block
        P = skipSpace(Contents, (Bounds->Close < Contents.size())
                                    ? Bounds->Close + 1U
                                    : Bounds->Close);
      }

      Context.Kind = ContextKind::ParamName;

      /** Now, `p` should be at the param name (or whitespace before it) */
      if (CursorOffset <= P) {
        SetPrefixRange(CursorOffset, CursorOffset);

        return Context;
      }

      SetPrefixRange(P, CursorOffset);

      return Context;
    }

    case TagType::Ref: {
      size_t P = skipSpace(Contents, After);

      Context.Kind = ContextKind::Reference;

      if (CursorOffset <= P) {
        SetPrefixRange(CursorOffset, CursorOffset);

        return Context;
      }

      SetPrefixRange(P, CursorOffset);

      return Context;
    }

    case TagType::Member: {
      size_t P = skipSpace(Contents, After);

      Context.Kind = ContextKind::MemberReference;

      if (CursorOffset <= P) {
        SetPrefixRange(CursorOffset, CursorOffset);

        return Context;
      }

      SetPrefixRange(P, CursorOffset);

      return Context;
    }

    default:
      return std::nullopt;
    }
  }

  /* Could not match tag; assume we are completing the tag itself */

  Context.Kind = ContextKind::Tag;

  SetPrefixRange(TagPos + 1U, CursorOffset);

  return Context;
}

// MARK: - Completion Builder

CodeCompleteResult completeTags(std::string_view Contents, size_t CursorOffset,
                                std::string_view Prefix, size_t TagPos,
                                char INI) {
  CodeCompleteResult R;

  Range TokenRange = {offsetToPosition(Contents, TagPos),
                      offsetToPosition(Contents, CursorOffset)};

  /* No prefix typed by user: return all tags */
  if (Prefix.empty()) {
    for (const auto &T : getAllTags()) {
      auto TagList = buildTagItems(T, INI, TokenRange);

      R.Completions.insert(R.Completions.end(), TagList.begin(), TagList.end());
    }

    return R;
  }

  /* With a prefix, loop through and find all matching tags */
  for (const auto &T : getAllTags()) {
    /* Does it match the tag name? */
    if (0U == T.Name.rfind(Prefix, 0U)) {
      auto TagList = buildTagItems(T, INI, TokenRange);

      R.Completions.insert(R.Completions.end(), TagList.begin(), TagList.end());
      continue;
    }

    /// Does it match one of the tag's aliases?
    for (const auto &Alias : T.Aliases) {
      if (0U == Alias.rfind(Prefix, 0U)) {
        auto TagList = buildTagItems(T, INI, TokenRange);

        R.Completions.insert(R.Completions.end(), TagList.begin(),
                             TagList.end());
        break;
      }
    }
  }

  return R;
}

CodeCompleteResult completeParamAttrs(const CompletionContext &Context,
                                      std::string_view Contents) {
  static constexpr const char *Candidates[] = {"in", "out", "opt"};

  CodeCompleteResult R;

  auto AlreadyContains = [&](std::string_view S) {
    return llvm::is_contained(Context.ExistingAttrs, S);
  };

  if (Context.Prefix.empty()) {
    for (const auto *Candidate : Candidates) {
      CodeCompletion C;

      C.Name = Candidate;
      C.FilterText = Candidate;
      C.Kind = CompletionItemKind::EnumMember;

      C.CompletionTokenRange = {
          offsetToPosition(Contents, Context.ReplaceBegin),
          offsetToPosition(Contents, Context.ReplaceEnd)};

      R.Completions.push_back(std::move(C));
    }

    return R;
  }

  for (const auto *Candidate : Candidates) {
    /* Check if this attribute is already in the attribute list */
    if (AlreadyContains(Candidate))
      continue;

    /* Does the prefix typed by the user match this candidate? */
    if (0U == std::string_view(Candidate).rfind(Context.Prefix, 0U)) {
      CodeCompletion C;

      C.Name = Candidate;
      C.FilterText = Candidate;
      C.Kind = CompletionItemKind::EnumMember;

      C.CompletionTokenRange = {
          offsetToPosition(Contents, Context.ReplaceBegin),
          offsetToPosition(Contents, Context.ReplaceEnd)};

      R.Completions.push_back(std::move(C));
    }
  }

  return R;
}

CodeCompleteResult completeParamNames(const CompletionContext &Context,
                                      std::string_view Contents,
                                      const ParsedAST *AST) {
  CodeCompleteResult R;

  if (nullptr == AST)
    return R;

  const FunctionDecl *Func = findOwningFunctionDecl(AST, Context.CursorPos);

  if (nullptr == Func)
    return R;

  // TODO: Filter out already documented parameters

  std::string_view Prefix = Context.Prefix;

  if (Prefix.empty()) {
    for (const auto *P : Func->parameters()) {
      /* Ensure we have a name */

      if (!P->getIdentifier())
        continue;

      std::string Name = P->getName().str();

      CodeCompletion C;

      C.Name = Name;
      C.FilterText = Name;
      C.Kind = CompletionItemKind::Variable;

      C.CompletionTokenRange = {
          offsetToPosition(Contents, Context.ReplaceBegin),
          offsetToPosition(Contents, Context.ReplaceEnd)};

      R.Completions.push_back(std::move(C));
    }

    return R;
  }

  for (const auto *P : Func->parameters()) {
    /* Ensure we have a name */

    if (!P->getIdentifier())
      continue;

    std::string Name = P->getName().str();

    if (!Prefix.empty() && 0U != Name.rfind(Prefix, 0U))
      continue;

    CodeCompletion C;

    C.Name = Name;
    C.FilterText = Name;
    C.Kind = CompletionItemKind::Variable;

    C.CompletionTokenRange = {offsetToPosition(Contents, Context.ReplaceBegin),
                              offsetToPosition(Contents, Context.ReplaceEnd)};

    R.Completions.push_back(std::move(C));
  }

  return R;
}

CodeCompleteResult completeReferences(const CompletionContext &Context,
                                      const CodeCompleteArgs &Args) {
  CodeCompleteResult R;

  if (!Context.Matched && '%' != Context.TriggerCharacter)
    return R;

  auto PatchedContents = std::string(Args.Contents);

  auto FindCommentStart = [&]() {
    auto MaxP = [&](size_t A, size_t B) {
      if (std::string_view::npos == A)
        return B;

      if (std::string_view::npos == B)
        return A;

      return (A > B) ? (A) : (B);
    };

    size_t S1 = PatchedContents.rfind("/**", Args.Offset),
           S2 = PatchedContents.rfind("/*!", Args.Offset),
           S3 = PatchedContents.rfind("///", Args.Offset),
           S4 = PatchedContents.rfind("//!", Args.Offset);

    return MaxP(MaxP(S1, S2), MaxP(S3, S4));
  };

  auto CommentStart = FindCommentStart();

  if (std::string_view::npos == CommentStart)
    return R;

  for (size_t I = CommentStart; I < Context.ReplaceBegin; I++) {
    if (!isSpace(PatchedContents[I]))
      PatchedContents[I] = ' ';
  }

  if ('%' == Context.TriggerCharacter) {
    PatchedContents[Context.TriggerPos] = ' ';
  }

  auto CommentEnd = PatchedContents.find("*/", Args.Offset);

  if (std::string_view::npos != CommentEnd) {
    for (size_t I = CommentEnd + 1U; I >= Context.ReplaceEnd; I--) {
      if (!isSpace(PatchedContents[I]))
        PatchedContents[I] = ' ';
    }
  }

  return codeCompleteFlowHook(Args.FileName, Args.Offset, Args.Preamble,
                              Args.ParseInput, Args.Opts, Args.SpecFuzzyFind,
                              PatchedContents);
}

CodeCompleteResult completeMemberReferences(const CompletionContext &Context,
                                            std::string_view Contents,
                                            const ParsedAST *AST) {
  CodeCompleteResult R;

  if (nullptr == AST)
    return R;

  const RecordDecl *Record = findOwningRecordDecl(AST, Context.CursorPos);

  if (nullptr == Record) {
    elog("No owning record found for member reference completion: '{0}'",
         Context.Prefix);
    return R;
  }

  const std::string_view Prefix = Context.Prefix;

  for (const FieldDecl *Field : Record->fields()) {
    if (nullptr == Field->getIdentifier())
      continue;

    std::string Name = Field->getName().str();

    if (!Prefix.empty() && Name.rfind(Prefix, 0U) != 0U)
      continue;

    CodeCompletion C;

    C.Name = Name;
    C.FilterText = Name;
    C.Kind = CompletionItemKind::Field;
    C.CompletionTokenRange = {offsetToPosition(Contents, Context.ReplaceBegin),
                              offsetToPosition(Contents, Context.ReplaceEnd)};

    R.Completions.push_back(std::move(C));
  }

  return R;
}

} // namespace

bool inDoccamComment(std::string_view Contents, size_t CursorOffset) {
  size_t StarBlockStart = Contents.rfind("/**", CursorOffset);
  size_t ExclBlockStart = Contents.rfind("/*!", CursorOffset);
  size_t BlockStart = std::string::npos;

  /* Select where to start. We want the comment block that is closest to the
   * cursor! */

  if (std::string::npos != StarBlockStart &&
      std::string::npos != ExclBlockStart)
    BlockStart = std::max(StarBlockStart, ExclBlockStart);
  else if (std::string::npos != StarBlockStart)
    BlockStart = StarBlockStart;
  else if (std::string::npos != ExclBlockStart)
    BlockStart = ExclBlockStart;

  /* Found the start of a '/** or '/*!' Doccam comment */
  if (std::string::npos != BlockStart) {
    size_t BlockEnd = Contents.find("*/", BlockStart);

    /* We are definitely in a Doccam comment */
    if (std::string::npos == BlockEnd || CursorOffset < BlockEnd)
      return true;
  }

  bool StartsWithThreeSlash = lineStartsWith(Contents, CursorOffset, "///");
  bool StartsWithExclSlash = lineStartsWith(Contents, CursorOffset, "//!");

  /** We are on a '///' or '//!' line */
  if (StartsWithThreeSlash || StartsWithExclSlash)
    return true;

  /* Not in a Doccam comment */
  return false;
}

bool shouldRunCompletion(std::string_view Contents, size_t CursorOffset,
                         std::string_view TriggerCharacter) {
  auto Context = buildContext(Contents, CursorOffset);
  if (!Context)
    return false;

  const char Trigger =
      TriggerCharacter.empty() ? '\0' : TriggerCharacter.front();
  const bool Manual = ('\0' == Trigger);

  const bool LastIsSpace =
      (CursorOffset > 0U) && isSpace(Contents[CursorOffset - 1U]);
  const bool EmptyPrefix = Context->Prefix.empty();
  const bool FinishedToken = (!EmptyPrefix && LastIsSpace);

  switch (Context->Kind) {
  case ContextKind::Tag: {
    return Manual ||
           (!TriggerCharacter.empty() && isDoccamTagInitiator(Trigger));
  }

  case ContextKind::ParamAttr: {
    if (Manual)
      return true;

    if (',' == Trigger)
      return true;

    if ('[' == Trigger)
      return true;

    if (' ' == Trigger)
      return true;

    return !FinishedToken;
  }

  case ContextKind::ParamName: {
    if (Manual)
      return true;

    if (' ' == Trigger)
      return EmptyPrefix;

    return !FinishedToken;
  }

  case ContextKind::Reference: {
    if (Manual)
      return true;

    if ('%' == Trigger)
      return true;

    if (' ' == Trigger)
      return EmptyPrefix;

    return !FinishedToken;
  }

  case ContextKind::MemberReference: {
    if (Manual)
      return true;

    if ('%' == Trigger)
      return true;

    if (' ' == Trigger)
      return EmptyPrefix;

    return !FinishedToken;
  }

  default:
    return false;
  }
}

std::optional<CodeCompleteResult> maybeCompleteDoccamComment(
    PathRef FileName, const PreambleData *Preamble,
    const ParseInputs &ParseInput, CodeCompleteOptions Opts,
    SpeculativeFuzzyFind *SpecFuzzyFind, StringRef Content, size_t Offset) {
  const ParsedAST *ASTPtr = nullptr;
  std::optional<ParsedAST> BuiltAST;
  std::shared_ptr<const PreambleData> PreambleSP;

  if (!inDoccamComment(Content, Offset))
    return std::nullopt;

  if (nullptr != Preamble &&
      CodeCompleteOptions::NeverParse != Opts.RunParser) {
    PreambleSP = std::shared_ptr<const PreambleData>(
        Preamble, [](const PreambleData *) {});
    clang::IgnoringDiagConsumer DC;
    std::unique_ptr<CompilerInvocation> CI =
        buildCompilerInvocation(ParseInput, DC, nullptr);

    if (CI) {
      std::vector<Diag> EmptyDiags;

      BuiltAST = ParsedAST::build(FileName, ParseInput, std::move(CI),
                                  EmptyDiags, PreambleSP);

      if (BuiltAST)
        ASTPtr = &*BuiltAST;
    }
  }

  CodeCompleteResult Empty;
  CodeCompleteArgs Args{
      FileName,      ParseInput, Preamble, Opts,
      SpecFuzzyFind, Content,    Offset,   ASTPtr,
  };

  auto Context = buildContext(Args.Contents, Args.Offset);

  if (!Context)
    return Empty;

  switch (Context->Kind) {
  case ContextKind::Tag: {
    auto [tagPos, ini] = rfindClosestTagInitiator(Args.Contents, Args.Offset);

    return completeTags(Args.Contents, Args.Offset, Context->Prefix, tagPos,
                        ini);
  }

  case ContextKind::ParamAttr:
    return completeParamAttrs(*Context, Args.Contents);

  case ContextKind::ParamName:
    return completeParamNames(*Context, Args.Contents, Args.AST);

  case ContextKind::Reference:
    return completeReferences(*Context, Args);

  case ContextKind::MemberReference:
    return completeMemberReferences(*Context, Args.Contents, Args.AST);

  default:
    return Empty;
  }
}

} // namespace clang::clangd::c32::doccam
