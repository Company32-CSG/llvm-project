#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_CONTEXT_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_CONTEXT_HPP
#include "c32-doccam/DoccamMarkdownStyle.hpp"

#include "Protocol.h"
#include "support/Context.h"

namespace clang::clangd::c32::doccam {

struct DoccamContext
{
	static clangd::Key<DoccamContext> ContextKey;

	static const DoccamContext& current();

	bool									   UseHtml				= false;
	bool									   UseCodicons			= false;
	c32::doccam::markdown::MarkdownSymbolStyle PreferredSymbolStyle = c32::doccam::markdown::MarkdownSymbolStyle::Glyph;

	DoccamContext() = default;

	DoccamContext(DoccamContextParameters Params)
	{
		DoccamContextSymbolStyle EffSymStyle =
			(Params.SupportedSymbolStyles & Params.PreferredSymbolStyle);

		UseHtml = Params.UseHtml;

		if (isDoccamContextSymbolStyleSet(EffSymStyle, DoccamContextSymbolStyle::Codicon))
			PreferredSymbolStyle = c32::doccam::markdown::MarkdownSymbolStyle::Codicon;
		else if (isDoccamContextSymbolStyleSet(EffSymStyle, DoccamContextSymbolStyle::Emoji))
			PreferredSymbolStyle = c32::doccam::markdown::MarkdownSymbolStyle::Emoji;
		else if (isDoccamContextSymbolStyleSet(EffSymStyle, DoccamContextSymbolStyle::Glyph))
			PreferredSymbolStyle = c32::doccam::markdown::MarkdownSymbolStyle::Glyph;
	}
};

} // namespace clang::clangd::c32::doccam

#endif
