#include "c32-doccam/DoccamMarkdown.hpp"
#include "c32-doccam/DoccamMarkdownHelpers.hpp"
#include "c32-doccam/DoccamParser.hpp"
#include "c32-doccam/DoccamUtils.hpp"
#include "c32-doccam/to_string.hpp"

#include "llvm/Support/FormatVariadic.h"

#include "Config.h"
#include "Hover.h"

namespace clang::clangd {

// MARK: - Helper Functions
namespace {

static std::string
formatSize(uint64_t SizeInBits)
{
	uint64_t	Value = SizeInBits % 8 == 0 ? SizeInBits / 8 : SizeInBits;
	const char* Unit  = Value != 0 && Value == SizeInBits ? "bit" : "byte";
	return llvm::formatv("{0} {1}{2}", Value, Unit, Value == 1 ? "" : "s").str();
}

static std::string
formatOffset(uint64_t OffsetInBits)
{
	const auto Bytes  = OffsetInBits / 8;
	const auto Bits	  = OffsetInBits % 8;
	auto	   Offset = formatSize(Bytes * 8);
	if (Bits != 0)
		Offset += " and " + formatSize(Bits);
	return Offset;
}

static index::SymbolKind
concreteSymbolKind(const HoverInfo& HI)
{
	if (HI.Kind == index::SymbolKind::TypeAlias && HI.EnhancedInfo.UnderlyingKind)
		return *HI.EnhancedInfo.UnderlyingKind;
	return HI.Kind;
}

} // namespace

// MARK: - Doccam Presentation

c32::doccam::markdown::Document
HoverInfo::presentDoccam() const
{
	const unsigned int				TitleHeadingLevel	= 2U;
	const unsigned int				SectionHeadingLevel = 3U;
	c32::doccam::markdown::Document Output;
	const Config&					Cfg = Config::current();

	auto ConcreteSymbolKind = concreteSymbolKind(*this);

	// MARK: - Helper Lambdas

	auto AddTitleFunc = [&]()
	{
		if (index::SymbolKind::Unknown == ConcreteSymbolKind)
		{
			if (auto LK = EnhancedInfo.LiteralValueKind)
			{
				Output.heading(TitleHeadingLevel)
					.symbol(c32::doccam::markdown::markdownSymbolFromLiteralKind(*LK)->Identifier)
					.space()
					.text(c32::doccam::to_string(*LK))
					.semanticAll(c32::doccam::markdown::semanticStyleFromLiteralKind(*LK));

				return;
			}

			Output.heading(TitleHeadingLevel)
				.symbol(c32::doccam::markdown::MarkdownSymbol::ID::Unknown)
				.space()
				.text("Unknown Kind")
				.space()
				.code(std::to_string(static_cast<int>(Kind)))
				.tintAll(c32::doccam::markdown::TintStyle::Muted);

			return;
		}

		Output.heading(TitleHeadingLevel)
			.symbol(c32::doccam::markdown::markdownSymbolFromClangSymbolKind(ConcreteSymbolKind)->Identifier)
			.space()
			.text(c32::doccam::to_string(ConcreteSymbolKind))
			.semanticAll(c32::doccam::markdown::semanticStyleFromSymbolKind(ConcreteSymbolKind));
	};

	auto MaybeAddIncludeHeaderFunc = [&]()
	{
		if (Provider.empty() || !Cfg.C32.Doccam.Hover.ShowProvider)
			return;

		/* Append the header file that provides this symbol */
		Output.codeBlock(DefinitionLanguage, "#include " + Provider);
	};

	auto AddSymbolDefinitionFunc = [&]()
	{
		std::string WorkingDefinition;

		/* Prepend the scope if ShowScope is enabled */
		if (Cfg.C32.Doccam.Hover.ShowScope)
		{
			if (NamespaceScope && !NamespaceScope->empty())
			{
				WorkingDefinition +=
					"namespace " + llvm::StringRef(*(NamespaceScope)).rtrim(':').str() +
					" {";
			}

			if (!LocalScope.empty())
			{
				WorkingDefinition +=
					llvm::StringRef(LocalScope).rtrim(':').str() + " {";
			}
		}

		/* Don't use the upstream definition for macros, we store the raw expansion
		 * text in the hover info and want to show that instead. */
		if (index::SymbolKind::Macro == ConcreteSymbolKind)
		{

			/* Use the raw macro definition we stored instead of the normal Definition
			 * to avoid altering the way upstream formats Definition for macros. */
			if (EnhancedInfo.RawMacroDefinition)
			{
				auto Formatted = c32::doccam::formatCode(
					EnhancedInfo.Style, *EnhancedInfo.RawMacroDefinition
				);

				WorkingDefinition += Formatted;
			}

			Output.codeBlock(DefinitionLanguage, WorkingDefinition);

			/* If we have macro expansion text, display it */
			if (EnhancedInfo.MacroExpansionText)
			{
				auto Formatted = c32::doccam::formatCode(
					EnhancedInfo.Style, *EnhancedInfo.MacroExpansionText
				);
				Output.heading(SectionHeadingLevel).text("Expansion");
				Output.codeBlock(DefinitionLanguage, Formatted);
			}

			return;
		}

		/* Append the symbol's definition (e.g., 'c32::doccam::markdown::Document
		 * HoverInfo::presentDoccam() const') */
		if (!Definition.empty())
		{
			if (index::SymbolKind::EnumConstant == ConcreteSymbolKind)
			{
				WorkingDefinition += Definition;

				if (const auto& Val = Value)
					WorkingDefinition += " = " + *Val;
			}
			else
			{
				if (!AccessSpecifier.empty())
					WorkingDefinition += AccessSpecifier + ": ";

				WorkingDefinition += Definition;
			}
		}

		/* Append the scope braces if ShowScope is enabled */
		if (Cfg.C32.Doccam.Hover.ShowScope)
		{
			if (NamespaceScope && !NamespaceScope->empty())
				WorkingDefinition += "\n}";

			if (!LocalScope.empty())
				WorkingDefinition += "\n}";
		}

		auto Formatted =
			c32::doccam::formatCode(EnhancedInfo.Style, WorkingDefinition);

		Output.codeBlock(DefinitionLanguage, Formatted);

		/* Thick divider between the 'KEY=n' definition and any doccam content
		 */
		Output.line(/* Thickness */ 4U);
	};

	auto MaybeAddSizeAndOffsetFunc = [&]()
	{
		if (!Offset && !Size)
			return;

		if (!Cfg.C32.Doccam.Hover.ShowSizeAndOffset)
			return;

		Output.line(4U);

		auto& Table = Output.table();

		if (Offset)
			Table.column("Offset");

		if (Size)
		{
			Table.column("Size");

			if (Padding && 0U != *Padding)
				Table.column("Padding");

			if (Align)
				Table.column("Alignment");
		}

		auto Row = Table.row();

		if (Offset)
		{
			Row["Offset"].code(formatOffset(*Offset));
		}

		if (Size)
		{
			Row["Size"].code(formatSize(*Size));

			if (Padding && 0U != *Padding)
			{
				Row["Padding"].code(formatSize(*Padding));
			}

			if (Align)
			{
				Row["Alignment"].code(formatSize(*Align));
			}
		}
	};

	auto MaybeAddCalleeArgInfoFunc = [&]()
	{
		if (!CalleeArgInfo || !CallPassType)
			return;

		if (!Cfg.C32.Doccam.Hover.ShowCalleeInfo)
			return;

		auto& Paragraph = Output.paragraph();

		if (auto ParentFuncOrMethodName =
				CalleeArgInfo->ParentFunctionOrMethodName)
		{

			std::string Working = *ParentFuncOrMethodName + "(";

			if (CalleeArgInfo->Type)
			{
				Working += CalleeArgInfo->Type->Type + " ";
			}

			if (CalleeArgInfo->Name)
			{
				Working += *(CalleeArgInfo->Name) + " = ";
			}

			if (CallPassType->PassBy != HoverInfo::PassType::Value)
			{
				Working += "(";

				if (CallPassType->PassBy == HoverInfo::PassType::ConstRef)
				{
					Working += "const ";
				}

				if (CallPassType->Converted && CalleeArgInfo->Type)
				{
					Working += "ref " + CalleeArgInfo->Type->Type;
				}
				else
				{
					Working += "ref";
				}

				Working += ") ";
			}

			Working += Name + ")";

			Output.heading(SectionHeadingLevel).text("Function Argument");

			Output.codeBlock(DefinitionLanguage, Working);
		}
		else
		{

			Paragraph.text("Passing").space().code(Name).space();

			if (CallPassType->PassBy != HoverInfo::PassType::Value)
			{
				Paragraph.text("by").space();

				if (CallPassType->PassBy == HoverInfo::PassType::ConstRef)
				{
					Paragraph.text("const").bold().italic().space();
				}

				Paragraph.text("reference").bold().italic().space();
			}

			if (CalleeArgInfo->Name)
			{
				Paragraph.text("as").space().code(*(CalleeArgInfo->Name)).space();
			}
			else if (CallPassType->PassBy == HoverInfo::PassType::Value)
			{
				Paragraph.text("by").space().text("value").italic().space();
			}

			if (CallPassType->Converted && CalleeArgInfo->Type)
			{
				Paragraph.text("(converted to")
					.space()
					.code(CalleeArgInfo->Type->Type)
					.text(")");
			}
		}
	};

	auto AddFullDoccamFunc = [&](c32::doccam::ParsedDoccam& Parsed)
	{
		bool KeepDivider = false;

		/* Append the brief/description */
		if (!Parsed.Brief.empty())
		{
			Output.paragraph().text(Parsed.Brief);
		}

		/* Append untagged user lines */
		if (!Parsed.UntaggedLines.empty())
		{
			auto& Paragraph = Output.paragraph();

			for (const auto& Line : Parsed.UntaggedLines)
			{
				Paragraph.text(Line).newline();
			}
		}

		/* Thick divider between the hover's heading with signature+brief and the
		 * rest */
		auto& Divider = Output.line(4U);

		/* Append all warning tags */
		if (!Parsed.Warnings.empty())
		{
			KeepDivider = true;

			Output.heading(SectionHeadingLevel).text("⚠️ Warning");

			auto& List = Output.list().compact();

			for (const auto& Warning : Parsed.Warnings)
			{
				List.item().text(Warning).italic();
			}
		}

		/* Append deprecated warning */
		if (!Parsed.Deprecated.empty())
		{
			KeepDivider = true;

			Output.heading(SectionHeadingLevel).text("Deprecated").strikethrough();

			Output.paragraph().text(Parsed.Deprecated);
		}

		/* Append a thick divider to separate warnings from the rest of the
		 * content
		 */
		if (!Parsed.Warnings.empty() || !Parsed.Deprecated.empty())
		{
			Output.line(/* Thickness */ 4U);
		}

		/* Append function parameters */
		if (!Parsed.Parameters.empty())
		{
			if (KeepDivider)
				Output.line();
			else
				KeepDivider = true;

			Output.heading(SectionHeadingLevel).text("Parameters");

			for (const auto& P : Parsed.Parameters)
			{
				auto& Paragraph = Output.paragraph();

				if (c32::doccam::ParameterTag::Specifier::None != P.Specifiers)
				{
					Paragraph.code(c32::doccam::to_string(P.Specifiers)).space();
				}

				Paragraph.code(P.Name).space().text("→").space().text(P.Description);
			}
		}

		/* Append the collected @throw/@throws tags */
		if (!Parsed.TParams.empty())
		{
			if (KeepDivider)
				Output.line();
			else
				KeepDivider = true;

			Output.heading(SectionHeadingLevel).text("Template Params");

			auto& List = Output.list();

			for (const auto& [key, val] : llvm::reverse(Parsed.TParams))
			{
				auto& Item = List.item();

				Item.code(key).bold();

				if (!val.empty())
				{
					Item.space().text("→").space().text(val);
				}
			}
		}

		/* Append the return type description */
		if (!Parsed.Returns.empty())
		{
			if (KeepDivider)
				Output.line();
			else
				KeepDivider = true;

			Output.heading(SectionHeadingLevel).text("Returns");
			Output.paragraph().text(Parsed.Returns);
		}

		/* Append the collected @retval/@result tags */
		if (!Parsed.Retvals.empty())
		{
			/* Add the 'Returns' heading if the user didn't specify a @returns tag
			 */
			if (Parsed.Returns.empty())
			{
				if (KeepDivider)
					Output.line();
				else
					KeepDivider = true;

				Output.heading(SectionHeadingLevel).text("Returns");
			}

			auto& List = Output.list();

			for (const auto& [key, val] : llvm::reverse(Parsed.Retvals))
			{
				auto& Item = List.item();

				Item.code(key).bold();

				if (!val.empty())
				{
					Item.space().text("→").space().text(val);
				}
			}
		}

		/* Append the collected @throw/@throws tags */
		if (!Parsed.Throws.empty())
		{
			if (KeepDivider)
				Output.line();
			else
				KeepDivider = true;

			Output.heading(SectionHeadingLevel).text("Throws");

			auto& List = Output.list();

			for (const auto& [key, val] : llvm::reverse(Parsed.Throws))
			{
				auto& Item = List.item();

				Item.code(key).bold();

				if (!val.empty())
				{
					Item.space().text("→").space().text(val);
				}
			}
		}

		/* Append the @version/@available/@availability tag */
		if (!Parsed.Version.first.empty())
		{
			if (KeepDivider)
				Output.line();
			else
				KeepDivider = true;

			Output.heading(SectionHeadingLevel).text("Availability");

			auto& Paragraph = Output.paragraph();

			Paragraph.code(Parsed.Version.first);

			if (!Parsed.Version.second.empty())
			{
				Paragraph.space().text("→").space().text(Parsed.Version.second);
			}
		}

		/* Append doxygen-like tags that aren't part of doccam */
		if (!Parsed.CustomTags.empty())
		{
			if (KeepDivider)
				Output.line();
			else
				KeepDivider = true;

			for (const auto& CustomTag : Parsed.CustomTags)
			{
				Output.heading(SectionHeadingLevel).text(CustomTag.Name);

				Output.paragraph().text(CustomTag.Body);
			}
		}

		/* Append any examples */
		if (!Parsed.CodeExamples.empty())
		{
			if (KeepDivider)
				Output.line();
			else
				KeepDivider = true;

			for (const auto& Exp : Parsed.CodeExamples)
			{
				Output.heading(SectionHeadingLevel)
					.text("Example")
					.space()
					.code(Exp.Lang);

				auto Formatted = c32::doccam::formatCode(EnhancedInfo.Style, Exp.Code);

				Output.codeBlock(Exp.Lang, Formatted);
			}
		}

		if (!KeepDivider)
			Divider.thickness(0U);
	};

	auto AddParameterDoccamFunc = [&](c32::doccam::ParsedDoccam& Parsed)
	{
		const c32::doccam::ParameterTag* Param = nullptr;

		/* Find our matching parameter */
		for (const auto& P : Parsed.Parameters)
		{
			if (P.Name == Name)
				Param = &P;
		}

		/* No parameter documentation? Bail early */
		if (nullptr == Param)
			return;

		auto& Paragraph = Output.paragraph();

		if (c32::doccam::ParameterTag::Specifier::None != Param->Specifiers)
		{
			Paragraph.code(c32::doccam::to_string(Param->Specifiers)).space();
		}

		Paragraph.code(Param->Name)
			.space()
			.text("→")
			.space()
			.text(Param->Description);
	};

	// MARK: - Build Output

	/* Maybe add the provider (typically an include directive) */
	MaybeAddIncludeHeaderFunc();

	/* Add the title for the hover info */
	AddTitleFunc();

	/* Add the symbol's definition/signature */
	AddSymbolDefinitionFunc();

	/* Maybe add the callee argument details */
	MaybeAddCalleeArgInfoFunc();

	/* Parse and build out Doccam content */
	if (!Documentation.empty())
	{
		auto Parsed = c32::doccam::parse(Documentation, EnhancedInfo.Style);

		switch (ConcreteSymbolKind)
		{
			case index::SymbolKind::Parameter:
				AddParameterDoccamFunc(Parsed);
				break;

			default:
				AddFullDoccamFunc(Parsed);
				break;
		}
	}

	/* Build out the enum values */
	if (index::SymbolKind::Enum == ConcreteSymbolKind)
	{
		if (const auto& Members = EnhancedInfo.EnumMembers)
		{
			Output.line(/* Thickness */ 4U);

			Output.heading(SectionHeadingLevel).text("Values");

			auto& List = Output.list();

			for (const auto& Member : *Members)
			{
				List.item()
					.code(Member.Name)
					.space()
					.text("=")
					.space()
					.code(Member.Value);
			}
		}
	}

	/* Maybe add the size, offset, and alignment details */
	MaybeAddSizeAndOffsetFunc();

	/* List symbols provided by the header when hovering over `#include`
	 * directives */
	if (!EnhancedInfo.ProvidedSymbols.empty())
	{
		Output.line();

		Output.heading(SectionHeadingLevel).text("Provides");

		auto& List = Output.list();

		auto Symbols = llvm::ArrayRef(EnhancedInfo.ProvidedSymbols).take_front(20U);

		for (const auto& Sym : Symbols)
		{
			std::string Kind = c32::doccam::to_string(Sym.Kind);

			if (const auto& Type = Sym.Type)
				Kind = (*Type).Type;

			List.item()
				.code(Sym.Name + (Sym.isFunction() ? "()" : ""))
				.bold()
				.space()
				.text("of type")
				.italic()
				.space()
				.code(Kind);
		}

		if (EnhancedInfo.ProvidedSymbols.size() > Symbols.size())
			List.item().text(
				"+" +
				std::to_string(EnhancedInfo.ProvidedSymbols.size() - Symbols.size()) +
				" more"
			);
	}

	return Output;
}

} // namespace clang::clangd
