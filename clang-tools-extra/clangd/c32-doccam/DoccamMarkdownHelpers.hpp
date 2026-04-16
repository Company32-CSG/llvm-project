#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_MARKDOWN_HELPERS_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_MARKDOWN_HELPERS_HPP
#include "c32-doccam/DoccamMarkdownStyle.hpp"

#include "Hover.h"

#include <string_view>

namespace clang::clangd::c32::doccam::markdown {

///////////////////////////////////////////////////////////
// Tint Styles ////////////////////////////////////////////

inline std::string_view
tintStyleToVSCodeCSS(TintStyle Style)
{
	switch (Style)
	{
		case TintStyle::Muted:
			return "var(--vscode-doccam-muted-foreground)";
		case TintStyle::Accent:
			return "var(--vscode-doccam-accent-foreground)";
		case TintStyle::Info:
			return "var(--vscode-doccam-info-foreground)";
		case TintStyle::Warning:
			return "var(--vscode-doccam-warning-foreground)";
		case TintStyle::Error:
			return "var(--vscode-doccam-error-foreground)";
		case TintStyle::Success:
			return "var(--vscode-doccam-success-foreground)";
		case TintStyle::None:
		default:
			return {};
	}
}

///////////////////////////////////////////////////////////
// Semantic Styling ///////////////////////////////////////

inline std::string_view
semanticStyleToVSCodeCSS(SemanticStyle Style)
{
	switch (Style)
	{
		case SemanticStyle::SymbolArray:
			return "var(--vscode-symbolIcon-arrayForeground)";
		case SemanticStyle::SymbolBoolean:
			return "var(--vscode-symbolIcon-booleanForeground)";
		case SemanticStyle::SymbolClass:
			return "var(--vscode-symbolIcon-classForeground)";
		case SemanticStyle::SymbolColor:
			return "var(--vscode-symbolIcon-colorForeground)";
		case SemanticStyle::SymbolConstant:
			return "var(--vscode-symbolIcon-constantForeground)";
		case SemanticStyle::SymbolConstructor:
			return "var(--vscode-symbolIcon-constructorForeground)";
		case SemanticStyle::SymbolEnum:
			return "var(--vscode-symbolIcon-enumForeground)";
		case SemanticStyle::SymbolEnumMember:
			return "var(--vscode-symbolIcon-enumMemberForeground)";
		case SemanticStyle::SymbolEvent:
			return "var(--vscode-symbolIcon-eventForeground)";
		case SemanticStyle::SymbolField:
			return "var(--vscode-symbolIcon-fieldForeground)";
		case SemanticStyle::SymbolFile:
			return "var(--vscode-symbolIcon-fileForeground)";
		case SemanticStyle::SymbolFolder:
			return "var(--vscode-symbolIcon-folderForeground)";
		case SemanticStyle::SymbolInterface:
			return "var(--vscode-symbolIcon-interfaceForeground)";
		case SemanticStyle::SymbolKey:
			return "var(--vscode-symbolIcon-keyForeground)";
		case SemanticStyle::SymbolKeyword:
			return "var(--vscode-symbolIcon-keywordForeground)";
		case SemanticStyle::SymbolMethod:
			return "var(--vscode-symbolIcon-methodForeground)";
		case SemanticStyle::SymbolModule:
			return "var(--vscode-symbolIcon-moduleForeground)";
		case SemanticStyle::SymbolNamespace:
			return "var(--vscode-symbolIcon-namespaceForeground)";
		case SemanticStyle::SymbolNull:
			return "var(--vscode-symbolIcon-nullForeground)";
		case SemanticStyle::SymbolNumber:
			return "var(--vscode-symbolIcon-numberForeground)";
		case SemanticStyle::SymbolObject:
			return "var(--vscode-symbolIcon-objectForeground)";
		case SemanticStyle::SymbolOperator:
			return "var(--vscode-symbolIcon-operatorForeground)";
		case SemanticStyle::SymbolPackage:
			return "var(--vscode-symbolIcon-packageForeground)";
		case SemanticStyle::SymbolProperty:
			return "var(--vscode-symbolIcon-propertyForeground)";
		case SemanticStyle::SymbolReference:
			return "var(--vscode-symbolIcon-referenceForeground)";
		case SemanticStyle::SymbolSnippet:
			return "var(--vscode-symbolIcon-snippetForeground)";
		case SemanticStyle::SymbolString:
			return "var(--vscode-symbolIcon-stringForeground)";
		case SemanticStyle::SymbolStruct:
			return "var(--vscode-symbolIcon-structForeground)";
		case SemanticStyle::SymbolText:
			return "var(--vscode-symbolIcon-textForeground)";
		case SemanticStyle::SymbolTypeParameter:
			return "var(--vscode-symbolIcon-typeParameterForeground)";
		case SemanticStyle::SymbolUnit:
			return "var(--vscode-symbolIcon-unitForeground)";
		case SemanticStyle::SymbolVariable:
			return "var(--vscode-symbolIcon-variableForeground)";
		case SemanticStyle::None:
		default:
			return {};
	}
}

inline SemanticStyle
semanticStyleFromSymbolKind(const clang::index::SymbolKind& Kind)
{
	switch (Kind)
	{
		case clang::index::SymbolKind::Module:
			return SemanticStyle::SymbolModule;

		case clang::index::SymbolKind::Namespace:
		case clang::index::SymbolKind::NamespaceAlias:
			return SemanticStyle::SymbolNamespace;

		case clang::index::SymbolKind::Macro:
			return SemanticStyle::SymbolConstant;

		case clang::index::SymbolKind::Enum:
			return SemanticStyle::SymbolEnum;

		case clang::index::SymbolKind::EnumConstant:
			return SemanticStyle::SymbolEnumMember;

		case clang::index::SymbolKind::Struct:
			return SemanticStyle::SymbolStruct;

		case clang::index::SymbolKind::Class:
			return SemanticStyle::SymbolClass;

		case clang::index::SymbolKind::Protocol:
			return SemanticStyle::SymbolInterface;

		case clang::index::SymbolKind::Extension:
			return SemanticStyle::SymbolClass;

		case clang::index::SymbolKind::Union:
			return SemanticStyle::SymbolStruct;

		case clang::index::SymbolKind::TypeAlias:
			return SemanticStyle::SymbolInterface;

		case clang::index::SymbolKind::Function:
		case clang::index::SymbolKind::InstanceMethod:
		case clang::index::SymbolKind::ClassMethod:
		case clang::index::SymbolKind::StaticMethod:
			return SemanticStyle::SymbolMethod;

		case clang::index::SymbolKind::InstanceProperty:
		case clang::index::SymbolKind::ClassProperty:
		case clang::index::SymbolKind::StaticProperty:
			return SemanticStyle::SymbolProperty;

		case clang::index::SymbolKind::Variable:
			return SemanticStyle::SymbolVariable;

		case clang::index::SymbolKind::Field:
			return SemanticStyle::SymbolField;

		default:
			return SemanticStyle::None;
	}
}

inline SemanticStyle
semanticStyleFromLiteralKind(const HoverInfo::EnhancedHoverInfo::LiteralKind& Kind)
{
	switch (Kind)
	{
		case HoverInfo::EnhancedHoverInfo::LiteralKind::String:
			return SemanticStyle::SymbolString;

		case HoverInfo::EnhancedHoverInfo::LiteralKind::Numeric:
			return SemanticStyle::SymbolNumber;

		default:
			return SemanticStyle::None;
	}
}

///////////////////////////////////////////////////////////
// Markdown Symbols ///////////////////////////////////////

inline const MarkdownSymbol*
markdownSymbolFromClangSymbolKind(const clang::index::SymbolKind& Kind)
{
	switch (Kind)
	{
		case clang::index::SymbolKind::Module:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolModule);

		case clang::index::SymbolKind::Namespace:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolNamespace);

		case clang::index::SymbolKind::NamespaceAlias:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolNamespace);

		case clang::index::SymbolKind::Macro:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolMacro);

		case clang::index::SymbolKind::Enum:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolEnum);

		case clang::index::SymbolKind::EnumConstant:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolEnumMember);

		case clang::index::SymbolKind::Struct:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolStruct);

		case clang::index::SymbolKind::Class:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolClass);

		case clang::index::SymbolKind::Protocol:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolInterface);

		case clang::index::SymbolKind::Extension:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolMisc);

		case clang::index::SymbolKind::Union:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolUnion);

		case clang::index::SymbolKind::TypeAlias:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolMisc);

		case clang::index::SymbolKind::Function:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolMethod);

		case clang::index::SymbolKind::InstanceMethod:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolMethod);

		case clang::index::SymbolKind::ClassMethod:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolMethod);

		case clang::index::SymbolKind::StaticMethod:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolMethod);

		case clang::index::SymbolKind::InstanceProperty:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolProperty);

		case clang::index::SymbolKind::ClassProperty:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolProperty);

		case clang::index::SymbolKind::StaticProperty:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolProperty);

		case clang::index::SymbolKind::Variable:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolVariable);

		case clang::index::SymbolKind::Field:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolField);

		case clang::index::SymbolKind::Constructor:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolConstructor);

		case clang::index::SymbolKind::Destructor:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolDestructor);

		case clang::index::SymbolKind::ConversionFunction:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolMethod);

		case clang::index::SymbolKind::Parameter:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolParameter);

		case clang::index::SymbolKind::Using:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolMisc);

		case clang::index::SymbolKind::TemplateTypeParm:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolTypeParameter);

		case clang::index::SymbolKind::TemplateTemplateParm:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolTypeParameter);

		case clang::index::SymbolKind::NonTypeTemplateParm:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolParameter);

		case clang::index::SymbolKind::Concept:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolMisc);

		case clang::index::SymbolKind::IncludeDirective:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolIncludeDirective);

		default:
			return markdownSymbolLookup(MarkdownSymbol::ID::Unknown);
	}
}

inline const MarkdownSymbol*
markdownSymbolFromLiteralKind(const HoverInfo::EnhancedHoverInfo::LiteralKind& Kind)
{
	switch (Kind)
	{
		case HoverInfo::EnhancedHoverInfo::LiteralKind::String:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolString);

		case HoverInfo::EnhancedHoverInfo::LiteralKind::Numeric:
			return markdownSymbolLookup(MarkdownSymbol::ID::SymbolNumber);

		default:
			return markdownSymbolLookup(MarkdownSymbol::ID::Unknown);
	}
}

///////////////////////////////////////////////////////////

} // namespace clang::clangd::c32::doccam::markdown

#endif
