#pragma once

#include "Enums.h"
#include <kls/temp/STL.h>

namespace count {
    class Reference {
    public:
        [[nodiscard]] int64_t load() const noexcept { return m_value; }

        [[nodiscard]] auto id() const noexcept { return m_id; }
    private:
        int32_t m_id;
        int64_t m_value;
    };

    class Operand {
    public:
        [[nodiscard]] int64_t load() const noexcept {
            if (m_is_imm) return m_data.immediate; else return m_data.reference->load();
        }
    private:
        union {
            int64_t immediate;
            Reference *reference;
        } m_data;
        bool m_is_imm;
    };

// instruction: short
// LSB                 MSB
// operation:4, comparison:4, ov:4, cv:4
    struct Instruction {
        Op operation;
        Cmp comparison;
        Reference left;
        Operand right;
    };

    struct ComputeRequest {
        kls::temp::vector<Reference> referred;
        kls::temp::vector<Instruction> program;
    };

    struct ComputeResult {
        struct Commit {
            int32_t id;
            int64_t value;
        };
        kls::temp::vector<int64_t> values;
        kls::temp::vector<Commit> commits;
    };

    ComputeResult compute(const ComputeRequest& request) noexcept;
}