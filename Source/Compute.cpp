#include "Compute.h"

namespace count {
    static int64_t compute_one(Op op, Cmp cmp, int64_t a, int64_t b) noexcept {
        switch (cmp) {
            case CMP_EQ:
                if (a != b) return a;
                else break;
            case CMP_NEQ:
                if (a == b) return a;
                else break;
            case CMP_L:
                if (a >= b) return a;
                else break;
            case CMP_G:
                if (a <= b) return a;
                else break;
            case CMP_LE:
                if (a > b) return a;
                else break;
            case CMP_GE:
                if (a < b) return a;
                else break;
            default:
                break;
        }
        switch (op) {
            case OP_SET:
                return b;
            case OP_INC:
                return a + 1;
            case OP_DEC:
                return a - 1;
            case OP_ADD:
                return a + b;
            case OP_SUB:
                return a - b;
            default:
                break;
        }
        return a;
    }

    ComputeResult compute(const ComputeRequest& request) noexcept {
        ComputeResult res {};
        res.values.reserve(request.program.size());
        res.commits.reserve(request.program.size());
        for (auto&& [op, cmp, a, b] : request.program) {
            const auto result = compute_one(op, cmp, a.load(), b.load());
            res.values.push_back(result);
            if (result != a.load()) res.commits.push_back({a.id()});
        }
        return res;
    }
}