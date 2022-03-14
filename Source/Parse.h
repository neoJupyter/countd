#pragma once

#include "Compute.h"
#include <optional>
#include <kls/essential/Memory.h>

namespace count {
    struct ParseResult {
        kls::temp::vector<Reference> referred;
        kls::temp::vector<Instruction> program;
        kls::temp::vector<std::string_view> name_table;
    };

    std::optional<ParseResult> parse(kls::essential::Span<> buffer) noexcept;
}
