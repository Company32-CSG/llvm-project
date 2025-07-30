#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_GFM_MARKDOWN_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_GFM_MARKDOWN_HPP
#include "Utils.hpp"

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

	virtual void emitSpace() = 0;

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
	emitSpace() override
	{
		out << " ";
	}

	void
	emitBlankLine() override
	{
		out << "  \n  \n";
	}

	void
	emitNewLine() override
	{
		out << "  \n";
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
	emitSpace() override
	{
		out << " ";
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
 * 		- `Markdown` **Example Text**
 * 		- `Plaintext` Example Text
 *
 * @example[c] {
 * 		Document doc;
 *
 * 		doc.paragraph()
 * 				.text("Example Text")
 * 				.bold();
 * }
 */
template <typename Base>
struct ChunkContainer
{
private:
	enum class Kind
	{
		Text,
		Code,
		Link,
		Space,
		Newline,
		Blankline
	};

	struct Chunk
	{
		Kind kind;

		std::string text;
		std::string url;

		bool isBold			 = false;
		bool isItalic		 = false;
		bool isStrikethrough = false;

		Chunk(Kind kind, std::string text = "", std::string url = "") : kind(kind), text(text), url(url) {}

		inline bool
		stylable() const
		{
			return Kind::Space != kind && Kind::Newline != kind && Kind::Blankline != kind;
		}
	};

	std::vector<Chunk> chunks;

public:
	Base&
	text(std::string s)
	{
		chunks.push_back({ Kind::Text, s });

		return static_cast<Base&>(*this);
	}

	Base&
	code(std::string s)
	{
		chunks.push_back({ Kind::Code, s });

		return static_cast<Base&>(*this);
	}

	Base&
	link(std::string text, std::string url)
	{
		chunks.push_back({ Kind::Link, text, url });

		return static_cast<Base&>(*this);
	}

	Base&
	space()
	{
		chunks.push_back({ Kind::Space });

		return static_cast<Base&>(*this);
	}

	Base&
	newline()
	{
		chunks.push_back({ Kind::Newline });

		return static_cast<Base&>(*this);
	}

	Base&
	blankline()
	{
		chunks.push_back({ Kind::Blankline });

		return static_cast<Base&>(*this);
	}

	Base&
	bold()
	{
		assert(!chunks.empty());

		chunks.back().isBold = true;

		return static_cast<Base&>(*this);
	}

	Base&
	italic()
	{
		assert(!chunks.empty());

		chunks.back().isItalic = true;

		return static_cast<Base&>(*this);
	}

	Base&
	strikethrough()
	{
		assert(!chunks.empty());

		chunks.back().isStrikethrough = true;

		return static_cast<Base&>(*this);
	}

	void
	renderChunks(Renderer& r) const
	{
		for (const auto& chunk : chunks)
		{
			auto text = trim(chunk.text);

			if (chunk.stylable())
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
					r.emitText(text);
					break;

				case Kind::Code:
					r.emitCode(text);
					break;

				case Kind::Link:
					r.emitLink(text, chunk.url);
					break;

				case Kind::Space:
					r.emitSpace();
					break;

				case Kind::Newline:
					r.emitNewLine();
					break;

				case Kind::Blankline:
					r.emitBlankLine();
					break;
			}

			if (chunk.stylable())
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
 * - **`Document`**
 * - `Heading`
 * - `Paragraph`
 * - `List`
 * - `Table`
 * - `CodeBlock`
 * - `BlockQuote`
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
class Heading : public Block, public ChunkContainer<Heading>
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
		r.emitBlankLine();

		/// Emit our ATX heading (#)
		r.emitHeader(mLevel);

		/// Render the chunks for this heading
		renderChunks(r);
	}
};

/**
 * @brief Paragraph for a `Document`.
 *
 * Use `Document::paragraph()` to create paragraphs.
 */
class Paragraph : public Block, public ChunkContainer<Paragraph>
{
	void
	render(Renderer& r) const override
	{
		r.emitNewLine();

		/// Render the chunks for this heading
		renderChunks(r);

		/// Finish the heading with blank line to separate from other content
		r.emitNewLine();
	}
};

/**
 * @brief Ordered (numerical) or unordered (bullet) lists for a `Document`.
 *
 * Use `Document::list()` and `Document::list().item()` to add lists and items.
 */
class List : public Block, public ChunkContainer<List>
{
private:
	std::vector<ChunkContainer> mItems;
	bool						mOrdered;
	bool						mCompact;

public:
	List(bool ordered = false) : mOrdered(ordered), mCompact(false) {}

	void
	render(Renderer& r) const override
	{
		size_t index = 1U;

		/// Render any title content
		renderChunks(r);

		/// Start our list
		r.emitNewLine();

		if (mCompact && 1U == mItems.size())
		{
			mItems.front().renderChunks(r);
		}
		else
		{
			for (const auto& item : mItems)
			{
				if (mOrdered)
					r.emitText(std::to_string(index++) + ". ");
				else
					r.emitText("- ");

				item.renderChunks(r);

				r.emitNewLine();
			}
		}

		/// End our list
		r.emitNewLine();
	}

	List&
	item()
	{
		mItems.emplace_back();

		return static_cast<List&>(mItems.back());
	}

	List&
	compact()
	{
		mCompact = true;

		return *this;
	}
};

/**
 * @brief Structured table for a `Document`.
 *
 * Use `Document::table()` and `Document::row()[0U].text("...")`, etc. to add tables and rows to tables.
 */
class Table : public Block
{
public:
	class Cell : public Block, public ChunkContainer<Cell>
	{
		void
		render(Renderer& r) const override
		{
			renderChunks(r);
		}
	};

	class RowBuilder
	{
	private:
		const std::vector<std::pair<std::string, Align>>& columns;

		std::vector<Cell>& cells;

	public:
		RowBuilder(const std::vector<std::pair<std::string, Align>>& columns, std::vector<Cell>& cells)
			: columns(columns), cells(cells)
		{
			assert(cells.size() == columns.size());
		}

		Cell&
		operator[](std::string_view name)
		{
			for (size_t i = 0U; i < columns.size(); i++)
			{
				const auto& [column, _] = columns[i];

				if (name == column)
					return cells[i];
			}

			llvm::errs() << "Table::RowBuilder::operator[] Unknown column name '" << name << "'\n";
			assert(false && "Table::RowBuilder::operator[] Unknown column name");

			return cells[0U];
		}
	};

private:
	std::vector<std::pair<std::string, Align>> columns;
	std::vector<std::vector<Cell>>			   rows;

public:
	Table() = default;

	Table&
	column(std::string name, Align alignment = Align::Left)
	{
		columns.emplace_back(std::move(name), alignment);

		return *this;
	}

	RowBuilder
	row()
	{
		assert(!columns.empty() && "Table::row() No columns!");

		rows.emplace_back(columns.size());

		return RowBuilder(columns, rows.back());
	}

	void
	render(Renderer& r) const override
	{
		r.emitNewLine();

		/// Emit the leading 'wall' of the column headers
		r.emitText("| ");

		/// Emit headers: ' | First | Second | '
		for (const auto& [column, _] : columns)
		{
			r.emitText(column);
			r.emitText(" | ");
		}

		r.emitNewLine();

		/// Emit the leading 'wall' of the column-row divider
		r.emitText("|");

		/// Emit divider with proper alignment for each column
		for (const auto& [_, alignment] : columns)
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
		for (const auto& rows : rows)
		{
			r.emitNewLine();

			/// Emit the leading 'wall' of each row
			r.emitText("| ");

			for (const auto& rowColumn : rows)
			{
				rowColumn.renderChunks(r);

				r.emitText(" | ");
			}
		}

		r.emitNewLine();
	}
};

/**
 * @brief Thematic break (line) for a `Document`.
 *
 * Use `Document::line()` to add a thematic break to the document.
 */
class Line : public Block
{
private:
	size_t mThickness;

public:
	Line(size_t thickness = 1U) : mThickness(thickness) {}

	/**
	 * Update a line's thickness prior to rendering. The line can be omitted
	 * by setting \p value to `0`.
	 *
	 * @param[in] value
	 * 		The line's thickness. Setting to `0` will omit the line.
	 */
	void
	thickness(size_t value)
	{
		mThickness = value;
	}

	void
	render(Renderer& r) const override
	{
		if (0U == mThickness)
			return;

		r.emitBlankLine();

		for (size_t i = 1U; i <= mThickness; i++)
		{
			r.emitText("---");

			if (i + 1U < mThickness)
				r.emitNewLine();
		}
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
		r.emitNewLine();
		r.emitText("```" + std::string(mCodeLang));
		r.emitNewLine();
		r.emitText(mCodeBlock);
		r.emitNewLine();
		r.emitText("```");
		r.emitNewLine();
	}
};

/**
 * @brief Block quotes `Document`.
 *
 * Use `Document::blockQuote()` to add block quotes to the document.
 */
class BlockQuote : public Block, public ChunkContainer<BlockQuote>
{
public:
	void
	render(Renderer& r) const override
	{
		r.emitNewLine();

		r.emitText("> ");

		renderChunks(r);

		r.emitNewLine();
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
 * 		doc.codeBlock("c", " int example_function() { printf("I will have syntax highlighting!"); } ");
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
	table()
	{
		auto block = std::make_unique<Table>();

		Table& bRef = *block;

		mBlocks.push_back(std::move(block));

		return bRef;
	}

	Line&
	line(size_t thickness = 1U)
	{
		auto block = std::make_unique<Line>(thickness);

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
