#include "c32-doccam/DoccamContext.hpp"

#include "support/Context.h"

namespace clang::clangd::c32::doccam {

clangd::Key<DoccamContext> DoccamContext::ContextKey;

const DoccamContext&
DoccamContext::current()
{
	if (const DoccamContext* C = Context::current().get(ContextKey))
		return *C;

	static const DoccamContext Default;

	return Default;
}

} // namespace clang::clangd::c32::doccam
