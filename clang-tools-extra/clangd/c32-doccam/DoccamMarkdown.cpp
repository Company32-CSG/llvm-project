#include "c32-doccam/DoccamMarkdown.hpp"
#include "c32-doccam/DoccamMarkdownHelpers.hpp"
#include "c32-doccam/DoccamMarkdownStyle.hpp"
#include "c32-doccam/DoccamUtils.hpp"

#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <string_view>

namespace clang::clangd::c32::doccam::markdown {

// MARK: - Markdown

void
MarkdownRenderer::markInlineContent()
{
	HasInlineContent  = true;
	LastWasWhitespace = false;
}

void
MarkdownRenderer::resetInlineState()
{
	HasInlineContent  = false;
	LastWasWhitespace = false;
}

void
MarkdownRenderer::emitHeader(unsigned int Level)
{
	static constexpr const char* ATX[] = { "#", "##", "###", "####", "#####", "######" };

	Level = std::clamp(Level, 1U, 6U);
	Out << ATX[Level - 1U] << " ";
}

void
MarkdownRenderer::emitText(std::string_view S, const TextStyle& Style)
{
	emitStyledInline(S, Style);
}

void
MarkdownRenderer::emitCode(std::string_view S, const TextStyle& Style)
{
	std::string Wrapped;
	Wrapped += "`";
	Wrapped += S;
	Wrapped += "`";

	emitStyledInline(Wrapped, Style);
}

void
MarkdownRenderer::emitLink(std::string_view Text, std::string_view Url, const TextStyle& Style)
{
	std::string Wrapped;
	Wrapped += "[";
	Wrapped += Text;
	Wrapped += "](";
	Wrapped += Url;
	Wrapped += ")";

	emitStyledInline(Wrapped, Style);
}

void
MarkdownRenderer::emitSymbol(MarkdownSymbol::ID ID, const TextStyle& Style)
{
	emitStyledInline(markdownSymbolResolve(ID, Options.PreferredSymbolStyle), Style);
}

void
MarkdownRenderer::emitSpace()
{
	if (!HasInlineContent || LastWasWhitespace)
		return;

	Out << " ";
	LastWasWhitespace = true;
}

void
MarkdownRenderer::emitBlankLine()
{
	Out << "\n\n";
	resetInlineState();
}

void
MarkdownRenderer::emitNewLine()
{
	Out << "\n";
	resetInlineState();
}

void
MarkdownRenderer::emitStyledInline(std::string_view S, const TextStyle& Style)
{
	if (S.empty())
		return;

	const std::string_view TintCSS = tintStyleToVSCodeCSS(Style.Tint);
	const std::string_view SemanticCSS =
		semanticStyleToVSCodeCSS(Style.Semantic);

	if (Options.UseHtml && (!TintCSS.empty() || !SemanticCSS.empty()))
	{
		const std::string Escaped = escapeHtml(S);
		const std::string Wrapped = fontStyleApply(Escaped, Style.Font);

		if (!TintCSS.empty())
		{
			Out << "<span style='color:" << TintCSS << ";'>" << Wrapped
				<< "</span>";
		}
		else
		{
			Out << "<span style='color:" << SemanticCSS << ";'>" << Wrapped
				<< "</span>";
		}
	}
	else
	{
		Out << fontStyleApply(S, Style.Font);
	}

	markInlineContent();
}

// MARK: - Plaintext

void
PlaintextRenderer::markInlineContent()
{
	HasInlineContent  = true;
	LastWasWhitespace = false;
}

void
PlaintextRenderer::resetInlineState()
{
	HasInlineContent  = false;
	LastWasWhitespace = false;
}

void
PlaintextRenderer::emitStyledInline(std::string_view S, const TextStyle& Style)
{
	if (S.empty())
		return;

	Out << S;
	markInlineContent();
}

void
PlaintextRenderer::emitHeader(unsigned int Level)
{
	(void)Level;
}

void
PlaintextRenderer::emitText(std::string_view S, const TextStyle& Style)
{
	emitStyledInline(S, Style);
}

void
PlaintextRenderer::emitCode(std::string_view S, const TextStyle& Style)
{
	std::string Wrapped;
	Wrapped += "`";
	Wrapped += S;
	Wrapped += "`";
	emitStyledInline(Wrapped, Style);
}

void
PlaintextRenderer::emitLink(std::string_view Text, std::string_view Url, const TextStyle& Style)
{
	std::string Wrapped(Text);
	Wrapped += " <";
	Wrapped += Url;
	Wrapped += ">";
	emitStyledInline(Wrapped, Style);
}

void
PlaintextRenderer::emitSymbol(MarkdownSymbol::ID ID, const TextStyle& Style)
{
	emitStyledInline(markdownSymbolResolve(ID, Options.PreferredSymbolStyle), Style);
}

void
PlaintextRenderer::emitSpace()
{
	if (!HasInlineContent || LastWasWhitespace)
		return;

	Out << " ";
	LastWasWhitespace = true;
}

void
PlaintextRenderer::emitBlankLine()
{
	Out << "\n\n";
	resetInlineState();
}

void
PlaintextRenderer::emitNewLine()
{
	Out << "\n";
	resetInlineState();
}

} // namespace clang::clangd::c32::doccam::markdown
