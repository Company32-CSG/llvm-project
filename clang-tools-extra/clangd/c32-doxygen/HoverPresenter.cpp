#include "HoverPresenter.hpp"
#include "DoxygenParser.hpp"
#include "Hover.h"
#include "Markdown.hpp"
#include "to_string.hpp"

#include "clang/Index/IndexSymbol.h"
#include "llvm/ADT/StringRef.h"

namespace clang::clangd::c32::hover {

/* ------------------------------------------------------------ */

namespace {

void
addHoveringOver(const HoverInfo& info, markdown::Document& output)
{
	auto symbolKind = info.concreteKind();

	if (index::SymbolKind::Unknown == symbolKind)
		return;

	output.paragraph()
		.text("Hovering Over:")
		.italic()
		.space()
		.code(std::to_string(symbolKind))
		.bold();
}

void
addScope(const HoverInfo& info, markdown::Document& output)
{
	if (info.LocalScope.empty() && (!info.NamespaceScope || info.NamespaceScope->empty()))
		return;

	auto& paragraph = output.paragraph();

	if (!info.LocalScope.empty())
	{
		paragraph
			.text("Scope:")
			.italic()
			.space()
			.code(llvm::StringRef(info.LocalScope).rtrim(':').str())
			.bold();
	}

	if (info.NamespaceScope && !info.NamespaceScope->empty())
	{
		paragraph
			.text("Namespace:")
			.italic()
			.space()
			.code(llvm::StringRef(*(info.NamespaceScope)).rtrim(':').str())
			.bold();
	}
}

} // namespace

void
presentForFunction(doxygen::ParsedDoxygen& doxygen, markdown::Document& output)
{
}

} // namespace clang::clangd::c32::hover
