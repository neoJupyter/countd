#include "Service.h"
#include <variant>

using namespace njp;
using namespace kls;
using namespace njp::count;
using namespace kls::coroutine;

namespace {
    enum Op { // 3bit
        OP_GET = 0u,
        OP_SET = 1u,
        OP_INC = 3u,
        OP_DEC = 4u,
        OP_ADD = 5u,
        OP_SUB = 6u
    };

    enum Cmp { // 3bit
        CMP_NONE = 0u,
        CMP_EQ = 1u,
        CMP_NEQ = 2u,
        CMP_L = 3U,
        CMP_G = 4U,
        CMP_LE = 5U,
        CMP_GE = 6U
    };

    enum Val { // 3bit
        VAL_ZERO = 0u,
        VAL_NAME = 1u,
        VAL_IMMSS = 2u,
        VAL_IMMUS = 3u,
        VAL_IMMSI = 4u,
        VAL_IMMUI = 5u,
        VAL_IMMSL = 6u,
    };
}

struct Immediate {
    int64_t value;
    void set(int64_t) { throw (void(this), std::runtime_error("Invalid Operation")); }
    [[nodiscard]] int64_t get() const { return value; }
};

struct StorageItem {
    int32_t key;
    ICache &cache;
    void set(int64_t value) { cache.store(key, value); }
    [[nodiscard]] int64_t get() const { return cache.load(key); }
};

class Value {
public:
    explicit Value(int64_t value) : m_variant{Immediate{.value = value}} {}
    Value(int32_t key, ICache &cache) : m_variant{StorageItem{.key = key, .cache=cache}} {}

    void set(int64_t value) {
        switch (m_variant.index()) {
            case 0:return std::get_if<0>(&m_variant)->set(value);
            case 1:return std::get_if<1>(&m_variant)->set(value);
        }
    }

    [[nodiscard]] int64_t get() const {
        switch (m_variant.index()) {
            case 0:return std::get_if<0>(&m_variant)->get();
            case 1:return std::get_if<1>(&m_variant)->get();
        }
        return 0;
    }
private:
    std::variant<Immediate, StorageItem> m_variant;
};

struct Instruction {
    Op op;
    Cmp cmp;
    Value target, operand;

    [[nodiscard]] int64_t run() {
        auto result = compute(op, cmp, target.get(), operand.get());
        return target.set(result), result;
    }
private:
    static int64_t compute(Op op, Cmp cmp, int64_t a, int64_t b) noexcept {
        switch (cmp) {
            case CMP_EQ:if (a != b) return a; else break;
            case CMP_NEQ:if (a == b) return a; else break;
            case CMP_L:if (a >= b) return a; else break;
            case CMP_G:if (a <= b) return a; else break;
            case CMP_LE:if (a > b) return a; else break;
            case CMP_GE:if (a < b) return a; else break;
            default:break;
        }
        switch (op) {
            case OP_SET:return b;
            case OP_INC:return a + 1;
            case OP_DEC:return a - 1;
            case OP_ADD:return a + b;
            case OP_SUB:return a - b;
            default:break;
        }
        return a;
    }
};

class Program {
public:
    Program(temp::vector<int32_t> id, temp::vector<Instruction> ins) noexcept:
            m_id(std::move(id)), m_ins(std::move(ins)) {}

    void run() {
        m_result.reserve(m_ins.size());
        for (auto &ins: m_ins) m_result.push_back(ins.run());
    }

    ValueAsync<> fetch(ICache &cache) { co_await cache.ensure(m_id); }
    ValueAsync<> store(ICache &cache) { co_await cache.commit(m_id); }
    temp::vector<int64_t> get() &&{ return {std::move(m_result)}; }
private:
    temp::vector<int32_t> m_id;
    temp::vector<int64_t> m_result;
    temp::vector<Instruction> m_ins;
};

static Program compile(const temp::vector<uint16_t> &code, const temp::vector<int32_t> &id, ICache &cache) {
    class CodeAccess {
    public:
        explicit CodeAccess(const temp::vector<uint16_t> &mData) noexcept: m_data(mData) {}
        void check(int count = 1) const { if (m_offset + count > m_data.size()) invalid(); }
        explicit operator bool() const noexcept { return m_offset < m_data.size(); }
        uint16_t get() { return m_data[m_offset++]; }
    private:
        size_t m_offset = 0;
        const temp::vector<uint16_t> &m_data;
        [[noreturn]] static void invalid() { throw std::runtime_error("Invalid Encoding: code incomplete"); }
    };
    CodeAccess access{code};
    auto decodeVal = [&](Val v) {
        switch (Val(v)) {
            case VAL_ZERO:return Value(0);
            case VAL_NAME:return Value(id[(access.check(1), access.get())], cache);
            case VAL_IMMSS:return Value(static_cast<int16_t>((access.check(1), access.get())));
            case VAL_IMMUS:return Value((access.check(1), access.get()));
            case VAL_IMMSI: {
                const uint32_t a = (access.check(1), access.get());
                const uint32_t b = (access.check(1), access.get());
                return Value(static_cast<int32_t>(a << 16 | b));
            }
            case VAL_IMMUI: {
                const uint32_t a = (access.check(1), access.get());
                const uint32_t b = (access.check(1), access.get());
                return Value(a << 16 | b);
            }
            case VAL_IMMSL: {
                const uint64_t a = (access.check(1), access.get());
                const uint64_t b = (access.check(1), access.get());
                const uint64_t c = (access.check(1), access.get());
                const uint64_t d = (access.check(1), access.get());
                return Value(static_cast<int64_t>((a << 48) | (b << 32) | (c << 16) | d));
            }
        }
        throw std::runtime_error("Invalid Encoding: bad value type");
    };
    auto decodeTgt = [&](Val v) {
        if (v != Val::VAL_NAME) throw std::runtime_error("Invalid Encoding: target is not named");
        return decodeVal(v);
    };
    temp::vector<Instruction> res{};
    while (bool(access)) {
        auto in0 = (access.check(1), access.get());
        auto tgt = decodeTgt(Val((in0 >> 4) & 0xF));
        auto val = decodeVal(Val(in0 & 0xF));
        res.push_back(Instruction{
                .op = Op((in0 >> 12) & 0xF), .cmp = Cmp((in0 >> 8) & 0xF), .target = tgt, .operand = val
        });
    }
    return {id, std::move(res)};
}

// this is the shell function for processing
static ValueAsync<temp_json> process(const temp_json &request, ICache &cache) {
    auto id_vector = cache.acquires(request["s"]);
    auto program = compile(request["c"], id_vector, cache);
    co_await program.fetch(cache), program.run(), co_await program.store(cache);
    co_return std::move(program).get();
}

ValueAsync<> count::run_service(IServer &server, ICache &cache) {
    server.handles("POST", "/", [&](const temp_json &request) { return process(request, cache); });
    co_await server.run();
}
