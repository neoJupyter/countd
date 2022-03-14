#pragma once

namespace count {
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

    enum ECode {
        E_SUCCESS = 0,
        E_BAD_INS = 1,
        E_BAD_ARGS = 2,
    };
}