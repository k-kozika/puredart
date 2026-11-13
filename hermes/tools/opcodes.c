// Preprocessor adapter for the pinned official BytecodeList.def.
#define OPERAND_STRING_ID(name, n) PH_STRING(name, n)
#define DEFINE_OPCODE_0(name) PH_OP(name)
#define DEFINE_OPCODE_1(name, a) PH_OP(name, a)
#define DEFINE_OPCODE_2(name, a, b) PH_OP(name, a, b)
#define DEFINE_OPCODE_3(name, a, b, c) PH_OP(name, a, b, c)
#define DEFINE_OPCODE_4(name, a, b, c, d) PH_OP(name, a, b, c, d)
#define DEFINE_OPCODE_5(name, a, b, c, d, e) PH_OP(name, a, b, c, d, e)
#define DEFINE_OPCODE_6(name, a, b, c, d, e, f) PH_OP(name, a, b, c, d, e, f)
#include "BytecodeList.def"
