#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_MARKDOWN_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_MARKDOWN_HPP
#include "c32-doccam/DoccamContext.hpp"
#include "c32-doccam/DoccamMarkdownStyle.hpp"
#include "c32-doccam/DoccamUtils.hpp"

#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::doccam::markdown {

// MARK: - Rendering

/**
 * Table column alignment specifier
 */
enum class Align
{
	/// Left column alignment (`:----`)
	Left,

	/// Right column alignment (`----:`)
	Right,

	/// Center column alignment (`:----:`)
	Center
};

/**
 * Base renderer interface for rendering the markup content.
 */
struct Renderer
{
	virtual void emitHeader(unsigned int Level) = 0;

	virtual void emitText(std::string_view S, const TextStyle& Style) = 0;

	virtual void emitCode(std::string_view S, const TextStyle& Style) = 0;

	virtual void emitLink(std::string_view Text, std::string_view Url, const TextStyle& Style) = 0;

	virtual void emitSymbol(MarkdownSymbol::ID ID, const TextStyle& Style) = 0;

	virtual void emitSpace() = 0;

	virtual void emitBlankLine() = 0;

	virtual void emitNewLine() = 0;
};

/**
 * Options for rendering the markup content.
 */
struct RenderOptions
{
	bool				UseHtml = false;
	MarkdownSymbolStyle PreferredSymbolStyle;

	RenderOptions() = default;

	RenderOptions(const DoccamContext& Ctx)
		: UseHtml(Ctx.UseHtml), PreferredSymbolStyle(Ctx.PreferredSymbolStyle) {}
};

class MarkdownRenderer : public Renderer
{
private:
	llvm::raw_ostream&	 Out;
	const RenderOptions& Options;

	bool HasInlineContent  = false;
	bool LastWasWhitespace = false;

private:
	void markInlineContent();
	void resetInlineState();
	void emitStyledInline(std::string_view S, const TextStyle& Style);

public:
	MarkdownRenderer(llvm::raw_ostream& Out, const RenderOptions& Options)
		: Out(Out), Options(Options) {}

	void emitHeader(unsigned int Level) override;
	void emitText(std::string_view S, const TextStyle& Style) override;
	void emitCode(std::string_view S, const TextStyle& Style) override;
	void emitLink(std::string_view Text, std::string_view Url, const TextStyle& Style) override;
	void emitSymbol(MarkdownSymbol::ID ID, const TextStyle& Style) override;
	void emitSpace() override;
	void emitBlankLine() override;
	void emitNewLine() override;
};

class PlaintextRenderer : public Renderer
{
private:
	llvm::raw_ostream&	 Out;
	const RenderOptions& Options;

	bool HasInlineContent  = false;
	bool LastWasWhitespace = false;

private:
	void markInlineContent();
	void resetInlineState();
	void emitStyledInline(std::string_view S, const TextStyle& Style);

public:
	PlaintextRenderer(llvm::raw_ostream& Out, const RenderOptions& Options)
		: Out(Out), Options(Options) {}

	void emitHeader(unsigned int Level) override;
	void emitText(std::string_view S, const TextStyle& Style) override;
	void emitCode(std::string_view S, const TextStyle& Style) override;
	void emitLink(std::string_view Text, std::string_view Url, const TextStyle& Style) override;
	void emitSymbol(MarkdownSymbol::ID ID, const TextStyle& Style) override;
	void emitSpace() override;
	void emitBlankLine() override;
	void emitNewLine() override;
};

// MARK: - Document

template <typename Base>
class ChunkContainer
{
private:
	enum class ChunkKind
	{
		Text,
		Code,
		Link,
		Symbol,
		Space,
		Newline,
		Blankline,
	};

	struct Chunk
	{
		ChunkKind Kind;

		std::string		   Text;
		std::string		   Url;
		MarkdownSymbol::ID SymbolID = MarkdownSymbol::ID::Warning;
		TextStyle		   Style;

		bool CanonicalizeInlineWhitespace = true;

		explicit Chunk(ChunkKind Kind) : Kind(Kind) {}

		Chunk(ChunkKind Kind, std::string Text, bool CanonicalizeInlineWhitespace = true)
			: Kind(Kind), Text(std::move(Text)),
			  CanonicalizeInlineWhitespace(CanonicalizeInlineWhitespace) {}

		Chunk(ChunkKind Kind, std::string Text, std::string Url, bool CanonicalizeInlineWhitespace = true)
			: Kind(Kind), Text(std::move(Text)), Url(std::move(Url)),
			  CanonicalizeInlineWhitespace(CanonicalizeInlineWhitespace) {}

		bool
		stylable() const
		{
			return ChunkKind::Text == Kind || ChunkKind::Code == Kind ||
				ChunkKind::Link == Kind || ChunkKind::Symbol == Kind;
		}
	};

	std::vector<Chunk> Chunks;

private:
	Chunk&
	lastStylableChunk()
	{
		assert(!Chunks.empty());
		assert(Chunks.back().stylable());
		return Chunks.back();
	}

	static void
	applyTint(TextStyle& Style, TintStyle Tint)
	{ Style.Tint = Tint; }

	static void
	applyFont(TextStyle& Style, FontStyle Font)
	{
		Style.Font |= Font;
	}

	static void
	applySemantic(TextStyle& Style, SemanticStyle Semantic)
	{
		Style.Semantic = Semantic;
	}

	void
	applyToAllStylable(void (*Fn)(TextStyle&, TintStyle), TintStyle Value)
	{
		for (auto& C : Chunks)
		{
			if (C.stylable())
				Fn(C.Style, Value);
		}
	}

	void
	applyToAllStylable(void (*Fn)(TextStyle&, FontStyle), FontStyle Value)
	{
		for (auto& C : Chunks)
		{
			if (C.stylable())
				Fn(C.Style, Value);
		}
	}

	void
	applyToAllStylable(void (*Fn)(TextStyle&, SemanticStyle), SemanticStyle Value)
	{
		for (auto& C : Chunks)
		{
			if (C.stylable())
				Fn(C.Style, Value);
		}
	}

protected:
	void
	renderChunks(Renderer& R) const
	{
		for (const auto& C : Chunks)
		{
			switch (C.Kind)
			{
				case ChunkKind::Text:
					R.emitText(C.CanonicalizeInlineWhitespace ? canonicalizeWhitespace(C.Text, false) : std::string_view(C.Text), C.Style);
					break;

				case ChunkKind::Code:
					R.emitCode(C.CanonicalizeInlineWhitespace ? canonicalizeWhitespace(C.Text, false) : std::string_view(C.Text), C.Style);
					break;

				case ChunkKind::Link:
					R.emitLink(C.CanonicalizeInlineWhitespace ? canonicalizeWhitespace(C.Text, false) : std::string_view(C.Text), C.Url, C.Style);
					break;

				case ChunkKind::Symbol:
					R.emitSymbol(C.SymbolID, C.Style);
					break;

				case ChunkKind::Space:
					R.emitSpace();
					break;

				case ChunkKind::Newline:
					R.emitNewLine();
					break;

				case ChunkKind::Blankline:
					R.emitBlankLine();
					break;
			}
		}
	}

public:
	Base&
	text(std::string_view S)
	{
		Chunks.emplace_back(ChunkKind::Text, std::string(S), true);
		return static_cast<Base&>(*this);
	}

	Base&
	rawText(std::string_view S)
	{
		Chunks.emplace_back(ChunkKind::Text, std::string(S), false);
		return static_cast<Base&>(*this);
	}

	Base&
	code(std::string_view S)
	{
		Chunks.emplace_back(ChunkKind::Code, std::string(S), true);
		return static_cast<Base&>(*this);
	}

	Base&
	rawCode(std::string_view S)
	{
		Chunks.emplace_back(ChunkKind::Code, std::string(S), false);
		return static_cast<Base&>(*this);
	}

	Base&
	link(std::string_view Text, std::string_view Url)
	{
		Chunks.emplace_back(ChunkKind::Link, std::string(Text), std::string(Url), true);
		return static_cast<Base&>(*this);
	}

	Base&
	rawLink(std::string_view Text, std::string_view Url)
	{
		Chunks.emplace_back(ChunkKind::Link, std::string(Text), std::string(Url), false);
		return static_cast<Base&>(*this);
	}

	Base&
	symbol(MarkdownSymbol::ID ID)
	{
		Chunks.emplace_back(ChunkKind::Symbol);
		Chunks.back().SymbolID = ID;
		return static_cast<Base&>(*this);
	}

	Base&
	space()
	{
		Chunks.emplace_back(ChunkKind::Space);
		return static_cast<Base&>(*this);
	}

	Base&
	newline()
	{
		Chunks.emplace_back(ChunkKind::Newline);
		return static_cast<Base&>(*this);
	}

	Base&
	blankline()
	{
		Chunks.emplace_back(ChunkKind::Blankline);
		return static_cast<Base&>(*this);
	}

	// Apply to most recent stylable chunk
	Base&
	tint(TintStyle Tint)
	{
		lastStylableChunk().Style.Tint = Tint;
		return static_cast<Base&>(*this);
	}

	Base&
	style(FontStyle Font)
	{
		lastStylableChunk().Style.Font |= Font;
		return static_cast<Base&>(*this);
	}

	Base&
	semantic(SemanticStyle Semantic)
	{
		lastStylableChunk().Style.Semantic = Semantic;
		return static_cast<Base&>(*this);
	}

	Base&
	bold()
	{ return style(FontStyle::Bold); }

	Base&
	italic()
	{ return style(FontStyle::Italic); }

	Base&
	strikethrough()
	{ return style(FontStyle::Strikethrough); }

	// Apply to all current stylable chunks in this container
	Base&
	tintAll(TintStyle Tint)
	{
		applyToAllStylable(&applyTint, Tint);
		return static_cast<Base&>(*this);
	}

	Base&
	styleAll(FontStyle Font)
	{
		applyToAllStylable(&applyFont, Font);
		return static_cast<Base&>(*this);
	}

	Base&
	semanticAll(SemanticStyle Semantic)
	{
		applyToAllStylable(&applySemantic, Semantic);
		return static_cast<Base&>(*this);
	}
};

struct Block
{
	virtual ~Block()					 = default;
	virtual void render(Renderer&) const = 0;
};

class Heading : public Block, public ChunkContainer<Heading>
{
private:
	unsigned int Level;

public:
	explicit Heading(unsigned int L) : Level(std::clamp(L, 1U, 6U)) {}

	void
	render(Renderer& R) const override
	{
		R.emitBlankLine();
		R.emitHeader(Level);
		renderChunks(R);
		R.emitNewLine();
	}
};

class Paragraph : public Block, public ChunkContainer<Paragraph>
{
public:
	void
	render(Renderer& R) const override
	{
		R.emitNewLine();
		renderChunks(R);
		R.emitNewLine();
	}
};

class List : public Block
{
public:
	class Item : public ChunkContainer<Item>
	{
	public:
		void
		render(Renderer& R) const
		{ renderChunks(R); }
	};

private:
	std::vector<Item> Items;
	bool			  Ordered = false;
	bool			  Compact = false;

public:
	explicit List(bool Ordered = false) : Ordered(Ordered) {}

	Item&
	item()
	{
		Items.emplace_back();
		return Items.back();
	}

	List&
	compact()
	{
		Compact = true;
		return *this;
	}

	void
	render(Renderer& R) const override
	{
		std::size_t Index = 1U;

		R.emitNewLine();

		if (Compact && 1U == Items.size())
		{
			Items.front().render(R);
			R.emitNewLine();
			return;
		}

		for (const auto& Item : Items)
		{
			if (Ordered)
				R.emitText(std::to_string(Index++) + ". ", {});
			else
				R.emitText("- ", {});

			Item.render(R);
			R.emitNewLine();
		}
	}
};

class Table : public Block
{
public:
	class Cell : public ChunkContainer<Cell>
	{
	public:
		void
		render(Renderer& R) const
		{ renderChunks(R); }
	};

	class RowBuilder
	{
	private:
		const std::vector<std::pair<std::string, Align>>& Columns;
		std::vector<Cell>&								  Cells;

	public:
		RowBuilder(const std::vector<std::pair<std::string, Align>>& Columns, std::vector<Cell>& Cells)
			: Columns(Columns), Cells(Cells)
		{
			assert(Cells.size() == Columns.size());
		}

		Cell&
		operator[](std::string_view Name)
		{
			for (std::size_t I = 0U; I < Columns.size(); I++)
			{
				if (Columns[I].first == Name)
					return Cells[I];
			}

			llvm::errs() << "Table::RowBuilder::operator[] Unknown column name '"
						 << Name << "'\n";
			assert(false && "Table::RowBuilder::operator[] Unknown column name");
			return Cells[0U];
		}
	};

private:
	std::vector<std::pair<std::string, Align>> Columns;
	std::vector<std::vector<Cell>>			   Rows;

public:
	Table&
	column(std::string Name, Align Alignment = Align::Left)
	{
		Columns.emplace_back(std::move(Name), Alignment);
		return *this;
	}

	RowBuilder
	row()
	{
		assert(!Columns.empty() && "Table::row() No columns!");
		Rows.emplace_back(Columns.size());
		return RowBuilder(Columns, Rows.back());
	}

	void
	render(Renderer& R) const override
	{
		R.emitNewLine();

		R.emitText("| ", {});
		for (const auto& [Column, _] : Columns)
		{
			R.emitText(Column, {});
			R.emitText(" | ", {});
		}
		R.emitNewLine();

		R.emitText("|", {});
		for (const auto& [_, Alignment] : Columns)
		{
			switch (Alignment)
			{
				case Align::Left:
					R.emitText(":---|", {});
					break;
				case Align::Center:
					R.emitText(":---:|", {});
					break;
				case Align::Right:
					R.emitText("---:|", {});
					break;
			}
		}
		R.emitNewLine();

		for (const auto& Row : Rows)
		{
			R.emitText("| ", {});

			for (const auto& Cell : Row)
			{
				Cell.render(R);
				R.emitText(" | ", {});
			}

			R.emitNewLine();
		}
	}
};

class Line : public Block
{
private:
	std::size_t Thickness = 1U;

public:
	explicit Line(std::size_t Thickness = 1U) : Thickness(Thickness) {}

	void
	thickness(std::size_t Value)
	{ Thickness = Value; }

	void
	render(Renderer& R) const override
	{
		if (0U == Thickness)
			return;

		R.emitBlankLine();

		for (std::size_t I = 0U; I < Thickness; I++)
		{
			R.emitText("---", {});
			R.emitNewLine();
		}
	}
};

class CodeBlock : public Block
{
private:
	std::string Lang;
	std::string Block;

public:
	CodeBlock(std::string Lang, std::string Code)
		: Lang(std::move(Lang)), Block(std::move(Code)) {}

	void
	render(Renderer& R) const override
	{
		R.emitNewLine();
		R.emitText("```" + Lang, {});
		R.emitNewLine();
		R.emitText(Block, {});
		R.emitNewLine();
		R.emitText("```", {});
		R.emitNewLine();
	}
};

class BlockQuote : public Block, public ChunkContainer<BlockQuote>
{
public:
	void
	render(Renderer& R) const override
	{
		R.emitNewLine();
		R.emitText("> ", {});
		renderChunks(R);
		R.emitNewLine();
	}
};

class Document : public Block
{
private:
	std::vector<std::unique_ptr<Block>> Blocks;

	void
	render(Renderer& R) const override
	{
		for (const auto& B : Blocks)
			B->render(R);
	}

public:
	Document() = default;

	explicit Document(std::string_view Contents) { paragraph().text(Contents); }

	void
	append(Document&& Other)
	{
		for (auto& B : Other.Blocks)
			Blocks.push_back(std::move(B));

		Other.Blocks.clear();
	}

	Heading&
	heading(unsigned int Level)
	{
		auto  RenderBlock = std::make_unique<Heading>(Level);
		auto& Ref		  = *RenderBlock;
		Blocks.push_back(std::move(RenderBlock));
		return Ref;
	}

	Paragraph&
	paragraph()
	{
		auto  RenderBlock = std::make_unique<Paragraph>();
		auto& Ref		  = *RenderBlock;
		Blocks.push_back(std::move(RenderBlock));
		return Ref;
	}

	List&
	list(bool Ordered = false)
	{
		auto  RenderBlock = std::make_unique<List>(Ordered);
		auto& Ref		  = *RenderBlock;
		Blocks.push_back(std::move(RenderBlock));
		return Ref;
	}

	Table&
	table()
	{
		auto  RenderBlock = std::make_unique<Table>();
		auto& Ref		  = *RenderBlock;
		Blocks.push_back(std::move(RenderBlock));
		return Ref;
	}

	Line&
	line(std::size_t Thickness = 1U)
	{
		auto  RenderBlock = std::make_unique<Line>(Thickness);
		auto& Ref		  = *RenderBlock;
		Blocks.push_back(std::move(RenderBlock));
		return Ref;
	}

	CodeBlock&
	codeBlock(std::string Lang, std::string Code)
	{
		auto RenderBlock =
			std::make_unique<CodeBlock>(std::move(Lang), std::move(Code));
		auto& Ref = *RenderBlock;
		Blocks.push_back(std::move(RenderBlock));
		return Ref;
	}

	BlockQuote&
	blockQuote()
	{
		auto  RenderBlock = std::make_unique<BlockQuote>();
		auto& Ref		  = *RenderBlock;
		Blocks.push_back(std::move(RenderBlock));
		return Ref;
	}

	std::string
	markdown(const RenderOptions& Options) const
	{
		std::string				 Buffer;
		llvm::raw_string_ostream Out(Buffer);
		MarkdownRenderer		 R(Out, Options);

		render(R);
		Out.flush();
		return Buffer;
	}

	std::string
	plaintext(const RenderOptions& Options) const
	{
		std::string				 Buffer;
		llvm::raw_string_ostream Out(Buffer);
		PlaintextRenderer		 R(Out, Options);

		render(R);
		Out.flush();
		return Buffer;
	}
};

} // namespace clang::clangd::c32::doccam::markdown

#endif
