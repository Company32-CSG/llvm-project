#include "DoxygenHover.hpp"

#include "../Hover.h"
#include "clang-include-cleaner/Analysis.h"

namespace clang::clangd::c32::doxygen {

std::optional<EnhancedHoverInfo>
getEnhancedHover(ParsedAST &AST, Position Pos, const format::FormatStyle &Style,
                 const SymbolIndex *Index) {
  static constexpr trace::Metric HoverCountMetric(
      "hover", trace::Metric::Counter, "case");

  PrintingPolicy PP =
      getPrintingPolicy(AST.getASTContext().getPrintingPolicy());
  const SourceManager &SM = AST.getSourceManager();
  auto CurLoc = sourceLocationInMainFile(SM, Pos);
  if (!CurLoc) {
    llvm::consumeError(CurLoc.takeError());
    return std::nullopt;
  }
  const auto &TB = AST.getTokens();
  auto TokensTouchingCursor = syntax::spelledTokensTouching(*CurLoc, TB);
  // Early exit if there were no tokens around the cursor.
  if (TokensTouchingCursor.empty())
    return std::nullopt;

  // Show full header file path if cursor is on include directive.
  for (const auto &Inc : AST.getIncludeStructure().MainFileIncludes) {
    if (Inc.Resolved.empty() || Inc.HashLine != Pos.line)
      continue;
    HoverCountMetric.record(1, "include");
    HoverInfo HI;

    HI.Style = Style;
    HI.Name = std::string(llvm::sys::path::filename(Inc.Resolved));

    // FIXME: We don't have a fitting value for Kind.
    HI.Definition =
        URIForFile::canonicalize(Inc.Resolved, AST.tuPath()).file().str();
    HI.DefinitionLanguage = "";

    maybeAddProvidedSymbols(AST, HI, Inc, PP);

    return HI;
  }

  // To be used as a backup for highlighting the selected token, we
  // use back as it aligns better with biases elsewhere (editors tend
  // to send the position for the left of the hovered token).
  CharSourceRange HighlightRange =
      TokensTouchingCursor.back().range(SM).toCharRange(SM);
  std::optional<HoverInfo> HI;
  // Macros and deducedtype only works on identifiers and
  // auto/decltype keywords respectively. Therefore they are only
  // triggered on whichever works for them, similar to
  // SelectionTree::create().
  for (const auto &Tok : TokensTouchingCursor) {
    if (Tok.kind() == tok::identifier) {
      // Prefer the identifier token as a fallback highlighting range.
      HighlightRange = Tok.range(SM).toCharRange(SM);
      if (auto M = locateMacroAt(Tok, AST.getPreprocessor())) {
        HoverCountMetric.record(1, "macro");
        HI = getHoverContents(*M, Tok, AST);
        if (auto DefLoc = M->Info->getDefinitionLoc(); DefLoc.isValid()) {
          include_cleaner::Macro IncludeCleanerMacro{
              AST.getPreprocessor().getIdentifierInfo(Tok.text(SM)), DefLoc};
          maybeAddSymbolProviders(AST, *HI,
                                  include_cleaner::Symbol{IncludeCleanerMacro});
        }

        break;
      }
    } else if (Tok.kind() == tok::kw_auto || Tok.kind() == tok::kw_decltype) {
      HoverCountMetric.record(1, "keyword");
      if (auto Deduced = getDeducedType(AST.getASTContext(), Tok.location())) {
        HI = getDeducedTypeHoverContents(*Deduced, Tok, AST.getASTContext(), PP,
                                         Index);
        HighlightRange = Tok.range(SM).toCharRange(SM);
        break;
      }

      // If we can't find interesting hover information for this
      // auto/decltype keyword, return nothing to avoid showing
      // irrelevant or incorrect informations.
      return std::nullopt;
    }
  }

  // If it wasn't auto/decltype or macro, look for decls and expressions.
  if (!HI) {
    auto Offset = SM.getFileOffset(*CurLoc);

    // Editors send the position on the left of the hovered
    // character. So our selection tree should be biased right.
    // (Tested with VSCode).
    SelectionTree ST =
        SelectionTree::createRight(AST.getASTContext(), TB, Offset, Offset);
    if (const SelectionTree::Node *N = ST.commonAncestor()) {
      // FIXME: Fill in HighlightRange with range coming from
      // N->ASTNode.
      auto Decls = explicitReferenceTargets(N->ASTNode, DeclRelation::Alias,
                                            AST.getHeuristicResolver());
      if (const auto *DeclToUse = pickDeclToUse(Decls)) {
        HoverCountMetric.record(1, "decl");
        HI = getHoverContents(DeclToUse, PP, Index, TB);
        // Layout info only shown when hovering on the field/class
        // itself.
        if (DeclToUse == N->ASTNode.get<Decl>())
          addLayoutInfo(*DeclToUse, *HI);

        // Look for a close enclosing expression to show the value of.
        if (!HI->Value)
          HI->Value = printExprValue(N, AST.getASTContext()).PrintedValue;
        maybeAddCalleeArgInfo(N, *HI, PP);

        if (!isa<NamespaceDecl>(DeclToUse))
          maybeAddSymbolProviders(AST, *HI,
                                  include_cleaner::Symbol{*DeclToUse});
      } else if (const Expr *E = N->ASTNode.get<Expr>()) {
        HoverCountMetric.record(1, "expr");
        HI = getHoverContents(N, E, AST, PP, Index);
      } else if (const Attr *A = N->ASTNode.get<Attr>()) {
        HoverCountMetric.record(1, "attribute");
        HI = getHoverContents(A, AST);
      }
      // FIXME: support hovers for other nodes?
      //  - built-in types
    }
  }

  if (!HI)
    return std::nullopt;

  // MARK: - C32 Begin
  HI->Style = Style;
  // MARK: - C32 End

  HI->DefinitionLanguage = getMarkdownLanguage(AST.getASTContext());
  HI->SymRange = halfOpenToRange(SM, HighlightRange);

  return HI;
}

c32::markdown::Document EnhancedHoverInfo::present() const {
  c32::markdown::Document output;

  auto symbolKind = concreteKind();

  // MARK: - Helper Lambdas

  auto maybeAddHoveringOver = [&]() {
    if (index::SymbolKind::Unknown == symbolKind)
      return;

    if (!Config::current().C32.Hover.ShowHoveringOver)
      return;

    output.paragraph()
        .text("Hovering Over:")
        .italic()
        .space()
        .code(std::to_string(symbolKind));
  };

  auto maybeAddScopeAndProvider = [&]() {
    if (Provider.empty() && LocalScope.empty() &&
        (!NamespaceScope || NamespaceScope->empty()))
      return;

    /* Append the header file that provides this symbol */
    if (!Provider.empty() && Config::current().C32.Hover.ShowProvider) {
      output.blockQuote().text("Provided by:").italic().space().code(Provider);
    }

    if (!Config::current().C32.Hover.ShowScope)
      return;

    if (NamespaceScope && !NamespaceScope->empty()) {
      output.blockQuote()
          .text("Namespace:")
          .italic()
          .space()
          .code(llvm::StringRef(*(NamespaceScope)).rtrim(':').str());
    }

    if (!LocalScope.empty()) {
      output.blockQuote().text("Scope:").italic().space().code(
          llvm::StringRef(LocalScope).rtrim(':').str());
    }
  };

  auto maybeAddSizeAndOffset = [&]() {
    if (!Offset && !Size)
      return;

    if (!Config::current().C32.Hover.ShowSizeAndOffset)
      return;

    std::string contextName = "Field";

    if (index::SymbolKind::Class == symbolKind)
      contextName = "Class";
    else if (index::SymbolKind::Struct == symbolKind)
      contextName = "Struct";

    output.line(4U);

    output.heading(3U).text(contextName + " Info");

    auto &table = output.table();

    if (Offset)
      table.column("Offset");

    if (Size) {
      table.column("Size");

      if (Padding && 0U != *Padding)
        table.column("Padding");

      if (Align)
        table.column("Alignment");
    }

    auto row = table.row();

    if (Offset) {
      row["Offset"].code(formatOffset(*Offset));
    }

    if (Size) {
      row["Size"].code(formatSize(*Size));

      if (Padding && 0U != *Padding) {
        row["Padding"].code(formatSize(*Padding));
      }

      if (Align) {
        row["Alignment"].code(formatSize(*Align));
      }
    }
  };

  auto maybeAddCalleeArgInfo = [&]() {
    if (!CalleeArgInfo || !CallPassType)
      return;

    if (!Config::current().C32.Hover.ShowCalleeInfo)
      return;

    auto &paragraph = output.paragraph();

    paragraph.text("Passing").space().code(Name).space();

    if (CallPassType->PassBy != HoverInfo::PassType::Value) {
      paragraph.text("by").space();

      if (CallPassType->PassBy == HoverInfo::PassType::ConstRef) {
        paragraph.text("const").bold().italic().space();
      }

      paragraph.text("reference").bold().italic().space();
    }

    if (CalleeArgInfo->Name) {
      paragraph.text("as").space().code(*(CalleeArgInfo->Name)).space();
    } else if (CallPassType->PassBy == HoverInfo::PassType::Value) {
      paragraph.text("by").space().text("value").italic().space();
    }

    if (CallPassType->Converted && CalleeArgInfo->Type) {
      paragraph.text("(converted to")
          .space()
          .code(CalleeArgInfo->Type->Type)
          .text(")");
    }
  };

  auto addFullDoxygen = [&](c32::doxygen::ParsedDoxygen &parsed) {
    bool keepDivider = false;

    /* Append the brief/description */
    if (!parsed.brief.empty()) {
      output.paragraph().text(parsed.brief);
    }

    /* Append untagged user lines */
    if (!parsed.untaggedLines.empty()) {
      auto &paragraph = output.paragraph();

      for (const auto &line : parsed.untaggedLines) {
        paragraph.text(line).newline();
      }
    }

    /* Thick divider between the hover's heading with signature+brief and the
     * rest */
    auto &divider = output.line(4U);

    /* Append all warning tags */
    if (!parsed.warnings.empty()) {
      keepDivider = true;

      output.heading(2U).text("⚠️ Warning");

      auto &list = output.list().compact();

      for (const auto &warning : parsed.warnings) {
        list.item().text(warning).italic();
      }
    }

    /* Append deprecated warning */
    if (!parsed.deprecated.empty()) {
      keepDivider = true;

      output.heading(3U).text("Deprecated").strikethrough();

      output.paragraph().text(parsed.deprecated);
    }

    /* Append a thick divider to separate warnings from the rest of the content
     */
    if (!parsed.warnings.empty() || !parsed.deprecated.empty()) {
      output.line(/* Thickness */ 4U);
    }

    /* Append function parameters */
    if (!parsed.parameters.empty()) {
      if (keepDivider)
        output.line();
      else
        keepDivider = true;

      output.heading(3U).text("Parameters");

      for (const auto &param : parsed.parameters) {
        auto &paragraph = output.paragraph();

        if (c32::doxygen::ParameterTag::Specifier::None != param.specifiers) {
          paragraph.code(std::to_string(param.specifiers)).space();
        }

        paragraph.code(param.name)
            .space()
            .text("→")
            .space()
            .text(param.description);
      }
    }

    /* Append the collected @throw/@throws tags */
    if (!parsed.tparams.empty()) {
      if (keepDivider)
        output.line();
      else
        keepDivider = true;

      output.heading(3U).text("Template Params");

      auto &list = output.list();

      for (const auto &[key, val] : llvm::reverse(parsed.tparams)) {
        auto &item = list.item();

        item.code(key).bold();

        if (!val.empty()) {
          item.space().text("→").space().text(val);
        }
      }
    }

    /* Append the return type description */
    if (!parsed.returns.empty()) {
      if (keepDivider)
        output.line();
      else
        keepDivider = true;

      output.heading(3U).text("Returns");
      output.paragraph().text(parsed.returns);
    }

    /* Append the collected @retval/@result tags */
    if (!parsed.retvals.empty()) {
      /* Add the 'Returns' heading if the user didn't specify a @returns tag */
      if (parsed.returns.empty()) {
        if (keepDivider)
          output.line();
        else
          keepDivider = true;

        output.heading(3U).text("Returns");
      }

      auto &list = output.list();

      for (const auto &[key, val] : llvm::reverse(parsed.retvals)) {
        auto &item = list.item();

        item.code(key).bold();

        if (!val.empty()) {
          item.space().text("→").space().text(val);
        }
      }
    }

    /* Append the collected @throw/@throws tags */
    if (!parsed.throws.empty()) {
      if (keepDivider)
        output.line();
      else
        keepDivider = true;

      output.heading(3U).text("Throws");

      auto &list = output.list();

      for (const auto &[key, val] : llvm::reverse(parsed.throws)) {
        auto &item = list.item();

        item.code(key).bold();

        if (!val.empty()) {
          item.space().text("→").space().text(val);
        }
      }
    }

    /* Append the @version/@available/@availability tag */
    if (!parsed.version.first.empty()) {
      if (keepDivider)
        output.line();
      else
        keepDivider = true;

      output.heading(3U).text("Availability");

      auto &paragraph = output.paragraph();

      paragraph.code(parsed.version.first);

      if (!parsed.version.second.empty()) {
        paragraph.space().text("→").space().text(parsed.version.second);
      }
    }

    /* Append custom Doxygen tags that we don't intercept */
    if (!parsed.customTags.empty()) {
      if (keepDivider)
        output.line();
      else
        keepDivider = true;

      for (const auto &customTag : parsed.customTags) {
        output.heading(3U).text(customTag.name);

        output.paragraph().text(customTag.body);
      }
    }

    /* Append any examples */
    if (!parsed.codeExamples.empty()) {
      if (keepDivider)
        output.line();
      else
        keepDivider = true;

      for (const auto &exp : parsed.codeExamples) {
        output.heading(3U).text("Example").space().code(exp.lang);

        output.paragraph().text("{").bold();

        output.codeBlock(exp.lang, c32::indentLines(exp.code));

        output.paragraph().text("}").bold();
      }
    }

    if (!keepDivider)
      divider.thickness(0U);
  };

  auto addParameterDoxygen = [&](c32::doxygen::ParsedDoxygen &parsed) {
    const c32::doxygen::ParameterTag *param = nullptr;

    /* Find our matching parameter */
    for (const auto &p : parsed.parameters) {
      if (p.name == Name)
        param = &p;
    }

    /* No parameter documentation? Bail early */
    if (nullptr == param)
      return;

    auto &paragraph = output.paragraph();

    if (c32::doxygen::ParameterTag::Specifier::None != param->specifiers) {
      paragraph.code(std::to_string(param->specifiers)).space();
    }

    paragraph.code(param->name)
        .space()
        .text("→")
        .space()
        .text(param->description);
  };

  // MARK: - Build Output

  /* Maybe add the 'Hovering Over' details */
  maybeAddHoveringOver();

  /* Append the symbol's definition (e.g., 'c32::markdown::Document
   * HoverInfo::presentC32Doxygen() const') */
  if (!Definition.empty()) {
    std::string def;

    if (!AccessSpecifier.empty())
      def += AccessSpecifier + ": ";

    def += Definition;

    if (index::SymbolKind::EnumConstant == symbolKind) {
      std::string def = Definition;

      if (const auto &val = Value)
        def += " = " + *val;

      output.codeBlock(DefinitionLanguage, def);

      if (!Documentation.empty()) {
        /* Thick divider between the 'KEY=n' definition and any doxygen */
        output.line(/* Thickness */ 4U);
      }
    } else {
      output.codeBlock(DefinitionLanguage, def);
    }
  }

  /* Maybe add the provider, namespace, and local scope details */
  maybeAddScopeAndProvider();

  /* Maybe add the callee argument details */
  maybeAddCalleeArgInfo();

  /* Parse and build out Doxygen content */
  if (!Documentation.empty()) {
    auto parsed = c32::doxygen::parse(Documentation, Style);

    switch (symbolKind) {
    case index::SymbolKind::Parameter:
      addParameterDoxygen(parsed);
      break;

    default:
      addFullDoxygen(parsed);
      break;
    }
  }

  /* Build out the enum values */
  if (index::SymbolKind::Enum == symbolKind) {
    if (const auto &Members = EnumMembers) {
      output.line(/* Thickness */ 4U);

      output.heading(3U).text("Values");

      auto &list = output.list();

      for (const auto &member : *Members) {
        list.item()
            .code(member.Name)
            .space()
            .text("=")
            .space()
            .code(member.Value);
      }
    }
  }

  /* Maybe add the size, offset, and alignment details */
  maybeAddSizeAndOffset();

  /* List symbols provided by the header when hovering over `#include`
   * directives */
  if (!ProvidedSymbols.empty()) {
    output.line();

    output.heading(4U).text("Provides");

    auto &list = output.list();

    auto symbols = llvm::ArrayRef(ProvidedSymbols).take_front(20U);

    for (const auto &sym : symbols) {
      std::string kind = std::to_string(sym.Kind);

      if (const auto &type = sym.Type)
        kind = (*type).Type;

      list.item()
          .code(sym.Name + (sym.isFunction() ? "()" : ""))
          .bold()
          .space()
          .text("of type")
          .italic()
          .space()
          .code(kind);
    }

    if (ProvidedSymbols.size() > symbols.size())
      list.item().text("+" +
                       std::to_string(ProvidedSymbols.size() - symbols.size()) +
                       " more");
  }

  return output;
}

} // namespace clang::clangd::c32::doxygen
