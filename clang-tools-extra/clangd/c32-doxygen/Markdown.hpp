#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_GFM_MARKDOWN_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_GFM_MARKDOWN_HPP

#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::markdown {

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
	virtual void emitHeader(unsigned int level) = 0;

	virtual void emitText(std::string_view s) = 0;

	virtual void emitCode(std::string_view s) = 0;

	virtual void emitLink(std::string_view text, std::string_view url) = 0;

	virtual void emitBlankLine() = 0;

	virtual void emitNewLine() = 0;

	virtual void emitBold() = 0;

	virtual void emitItalic() = 0;

	virtual void emitStrikethrough() = 0;
};

/**
 * Markdown renderer implementation of `Renderer`
 */
struct MarkdownRenderer : Renderer
{
	llvm::raw_ostream& out;

	MarkdownRenderer(llvm::raw_ostream& out) : out(out) {}

	void
	emitHeader(unsigned int level) override
	{
		constexpr const char* ATX[] = { "#", "##", "###", "####", "#####", "######" };

		/* Level has been checked before render */
		level -= 1U;

		out << ATX[level] << " ";
	}

	void
	emitText(std::string_view s) override
	{
		out << s;
	}

	void
	emitCode(std::string_view s) override
	{
		out << "`" << s << "`";
	}

	void
	emitLink(std::string_view text, std::string_view url) override
	{
		out << "[" << text << "]" << "(" << url << ")";
	}

	void
	emitBlankLine() override
	{
		out << "\n\n";
	}

	void
	emitNewLine() override
	{
		out << "\n";
	}

	void
	emitBold() override
	{
		out << "**";
	}

	void
	emitItalic() override
	{
		out << "*";
	}

	void
	emitStrikethrough() override
	{
		out << "~~";
	}
};

/**
 * Plaintext renderer implementation of `Renderer`
 */
struct PlaintextRenderer : Renderer
{
	llvm::raw_ostream& out;

	PlaintextRenderer(llvm::raw_ostream& out) : out(out) {}

	void
	emitHeader(unsigned int level) override
	{
		(void)(level);
	}

	void
	emitText(std::string_view s) override
	{
		out << s;
	}

	void
	emitCode(std::string_view s) override
	{
		out << "`" << s << "`";
	}

	void
	emitLink(std::string_view text, std::string_view url) override
	{
		out << "[" << text << "]" << "(" << url << ")";
	}

	void
	emitBlankLine() override
	{
		out << "\n\n";
	}

	void
	emitNewLine() override
	{
		out << "\n";
	}

	void
	emitBold() override
	{
	}

	void
	emitItalic() override
	{
	}

	void
	emitStrikethrough() override
	{
	}
};

/**
 * @brief Basic `Chunk` container that is used to build up markup content and
 * optionally customize the font with functions like `bold()` and `italic()`.
 *
 * The example below will produce:
 * 		@md - `Markdown` **Example Text**
 * 		@md - `Plaintext` Example Text
 *
 * @example[c] {
 * 		Document doc;
 *
 * 		doc.paragraph()
 * 				.text("Example Text")
 * 				.bold();
 * }
 */
struct ChunkContainer
{
private:
	enum class Kind
	{
		Text,
		Code,
		Link
	};

	struct Chunk
	{
		Kind kind;

		std::string text;
		std::string url;

		bool isBold			 = false;
		bool isItalic		 = false;
		bool isStrikethrough = false;

		Chunk(Kind kind, std::string text, std::string url = "") : kind(kind), text(text), url(url) {}
	};

	std::vector<Chunk> chunks;

public:
	ChunkContainer&
	text(std::string s)
	{
		chunks.push_back({ Kind::Text, s });

		return *this;
	}

	ChunkContainer&
	code(std::string s)
	{
		chunks.push_back({ Kind::Code, s });

		return *this;
	}

	ChunkContainer&
	link(std::string text, std::string url)
	{
		chunks.push_back({ Kind::Link, text, url });

		return *this;
	}

	ChunkContainer&
	bold()
	{
		assert(!chunks.empty());

		chunks.back().isBold = true;

		return *this;
	}

	ChunkContainer&
	italic()
	{
		assert(!chunks.empty());

		chunks.back().isItalic = true;

		return *this;
	}

	ChunkContainer&
	strikethrough()
	{
		assert(!chunks.empty());

		chunks.back().isStrikethrough = true;

		return *this;
	}

	void
	renderChunks(Renderer& r) const
	{
		for (const auto& chunk : chunks)
		{
			if (Kind::Code != chunk.kind)
			{
				if (chunk.isBold)
					r.emitBold();

				if (chunk.isItalic)
					r.emitItalic();

				if (chunk.isStrikethrough)
					r.emitStrikethrough();
			}

			switch (chunk.kind)
			{
				case Kind::Text:
					r.emitText(chunk.text);
					break;

				case Kind::Code:
					r.emitCode(chunk.text);
					break;

				case Kind::Link:
					r.emitLink(chunk.text, chunk.url);
					break;
			}

			if (Kind::Code != chunk.kind)
			{
				if (chunk.isStrikethrough)
					r.emitStrikethrough();

				if (chunk.isItalic)
					r.emitItalic();

				if (chunk.isBold)
					r.emitBold();
			}
		}
	}
};

/**
 * @brief Base container for all markup content. Each document consists
 * of multiple `Block` based classes.
 *
 * The current types are:
 * @md - **`Document`**
 * @md - `Heading`
 * @md - `Paragraph`
 * @md - `List`
 * @md - `Table`
 * @md - `CodeBlock`
 * @md - `BlockQuote`
 */
struct Block
{
	virtual ~Block()					 = default;
	virtual void render(Renderer&) const = 0;
};

/**
 * @brief Heading for a `Document`.
 *
 * Use `Document::heading()` to create headings.
 */
class Heading : public Block, public ChunkContainer
{
private:
	unsigned int mLevel;

public:
	Heading(unsigned int level)
	{
		if (level < 1U)
			level = 0U;

		mLevel = std::min(level, 6U);
	}

	void
	render(Renderer& r) const override
	{
		/// Emit our ATX heading (#)
		r.emitHeader(mLevel);

		/// Render the chunks for this heading
		renderChunks(r);

		/// Finish the heading with blank line to separate from other content
		r.emitBlankLine();
	}
};

/**
 * @brief Paragraph for a `Document`.
 *
 * Use `Document::paragraph()` to create paragraphs.
 */
class Paragraph : public Block, public ChunkContainer
{
	void
	render(Renderer& r) const override
	{
		/// Render the chunks for this heading
		renderChunks(r);

		/// Finish the heading with blank line to separate from other content
		r.emitBlankLine();
	}
};

/**
 * @brief Ordered (numerical) or unordered (bullet) lists for a `Document`.
 *
 * Use `Document::list()` and `Document::list().item()` to add lists and items.
 */
class List : public Block
{
private:
	std::vector<ChunkContainer> mItems;
	bool						mOrdered;

public:
	List(bool ordered = false) : mOrdered(ordered) {}

	void
	render(Renderer& r) const override
	{
		size_t index = 1U;

		for (const auto& item : mItems)
		{
			if (mOrdered)
				r.emitText(std::to_string(index++) + ". ");
			else
				r.emitText("- ");

			item.renderChunks(r);

			r.emitNewLine();
		}

		r.emitNewLine();
	}

	ChunkContainer&
	item()
	{
		mItems.emplace_back();

		return mItems.back();
	}
};

/**
 * @brief Structured table for a `Document`.
 *
 * Use `Document::table()` and `Document::row()[0U].text("...")`, etc. to add tables and rows to tables.
 */
class Table : public Block
{
private:
	std::vector<std::pair<std::string, Align>> mColumns;
	std::vector<std::vector<ChunkContainer>>   mRows;

public:
	Table(std::initializer_list<std::string> columns, Align alignment)
	{
		mColumns.reserve(columns.size());

		for (const auto& name : columns)
			mColumns.emplace_back(name, alignment);
	}

	Table(std::initializer_list<std::pair<std::string, Align>> columns) : mColumns(columns) {}

	std::vector<ChunkContainer>&
	row()
	{
		std::vector<ChunkContainer> newRow(mColumns.size());

		mRows.push_back(std::move(newRow));

		return mRows.back();
	}

	void
	render(Renderer& r) const override
	{
		r.emitBlankLine();

		/// Emit the leading 'wall' of the column headers
		r.emitText("| ");

		/// Emit headers: ' | First | Second | '
		for (const auto& [column, _] : mColumns)
		{
			r.emitText(column);
			r.emitText(" |");
		}

		r.emitNewLine();

		/// Emit the leading 'wall' of the column-row divider
		r.emitText("|");

		/// Emit divider with proper alignment for each column
		for (const auto& [_, alignment] : mColumns)
		{
			switch (alignment)
			{
				case Align::Left:
					r.emitText(":---|");
					break;

				case Align::Center:
					r.emitText(":---:|");
					break;

				case Align::Right:
					r.emitText("---:|");
					break;
			}
		}

		/// Emit the content for each row
		for (const auto& rows : mRows)
		{
			r.emitNewLine();

			/// Emit the leading 'wall' of each row
			r.emitText("| ");

			for (const auto& rowColumn : rows)
			{
				rowColumn.renderChunks(r);

				r.emitText(" |");
			}
		}

		r.emitBlankLine();
	}
};

/**
 * @brief Thematic break (line) for a `Document`.
 *
 * Use `Document::line()` to add a thematic break to the document.
 */
class Line : public Block
{
	void
	render(Renderer& r) const override
	{
		r.emitBlankLine();
		r.emitText("---");
		r.emitBlankLine();
	}
};

/**
 * @brief Syntax highlighted code block for `Document`.
 *
 * Use `Document::codeBlock()` to add code with syntax highlighting to the document.
 */
class CodeBlock : public Block
{
private:
	std::string mCodeLang;
	std::string mCodeBlock;

public:
	CodeBlock(std::string lang, std::string code) : mCodeLang(lang), mCodeBlock(code) {}

	void
	render(Renderer& r) const override
	{
		r.emitText("```" + std::string(mCodeLang));
		r.emitBlankLine();
		r.emitText(mCodeBlock);
		r.emitBlankLine();
		r.emitText("```");
	}
};

/**
 * @brief Block quotes `Document`.
 *
 * Use `Document::blockQuote()` to add block quotes to the document.
 */
class BlockQuote : public Block, public ChunkContainer
{
public:
	void
	render(Renderer& r) const override
	{
		r.emitText("> ");

		renderChunks(r);
	}
};

/**
 * @brief Base document to create markup content.
 *
 * @example[c] {
 * 		Document doc;
 *
 * 		doc.heading(3)
 * 				.text("My Header");
 *
 * 		doc.line();
 *
 * 		doc.paragraph()
 * 				.text("Example Text")
 * 				.bold()
 * 				.text("Event more text in this paragraph");
 *
 *		doc.paragraph()
 * 				.text("This text will be bold, italic, and have a strikethrough!")
 * 				.bold()
 * 				.italic()
 * 				.strikethrough();
 *
 *		doc.line();
 *
 * 		doc.codeBlock("c", " int example_function() { printf(\"I will have syntax highlighting!\"); } ");
 * }
 */
class Document : public Block
{
private:
	std::vector<std::unique_ptr<Block>> mBlocks;

	void
	render(Renderer& r) const override
	{
		for (const auto& block : mBlocks)
		{
			block->render(r);
		}
	}

public:
	Heading&
	heading(unsigned int level)
	{
		auto block = std::make_unique<Heading>(level);

		Heading& bRef = *block;

		mBlocks.push_back(std::move(block));

		return bRef;
	}

	Paragraph&
	paragraph()
	{
		auto block = std::make_unique<Paragraph>();

		Paragraph& bRef = *block;

		mBlocks.push_back(std::move(block));

		return bRef;
	}

	List&
	list(bool ordered = false)
	{
		auto block = std::make_unique<List>(ordered);

		List& bRef = *block;

		mBlocks.push_back(std::move(block));

		return bRef;
	}

	Table&
	table(std::initializer_list<std::string> columns, Align alignment = Align::Left)
	{
		auto block = std::make_unique<Table>(columns, alignment);

		Table& bRef = *block;

		mBlocks.push_back(std::move(block));

		return bRef;
	}

	Table&
	table(std::initializer_list<std::pair<std::string, Align>> columns)
	{
		auto block = std::make_unique<Table>(columns);

		Table& bRef = *block;

		mBlocks.push_back(std::move(block));

		return bRef;
	}

	Line&
	line()
	{
		auto block = std::make_unique<Line>();

		Line& bRef = *block;

		mBlocks.push_back(std::move(block));

		return bRef;
	}

	CodeBlock&
	codeBlock(std::string lang, std::string code)
	{
		auto block = std::make_unique<CodeBlock>(lang, code);

		CodeBlock& bRef = *block;

		mBlocks.push_back(std::move(block));

		return bRef;
	}

	BlockQuote&
	blockQuote()
	{
		auto block = std::make_unique<BlockQuote>();

		BlockQuote& bRef = *block;

		mBlocks.push_back(std::move(block));

		return bRef;
	}

	std::string
	markdown()
	{
		std::string				 buffer;
		llvm::raw_string_ostream out(buffer);
		MarkdownRenderer		 r(out);

		render(r);

		return buffer;
	}

	std::string
	plaintext()
	{
		std::string				 buffer;
		llvm::raw_string_ostream out(buffer);
		PlaintextRenderer		 r(out);

		render(r);

		return buffer;
	}
};

} // namespace clang::clangd::c32::markdown

#endif
