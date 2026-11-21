/**
 * \file tests/vm/command_pipeline_tests.cpp
 * one script taken through every stage, with what each stage gives written out in full.
 * the other command tests check one thing each; this one fails when anything at all changes
 **/
#include <cstring>

#include "vm/builtin_command_devices.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/ocmd_headers.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"
#include "vm/command_language/command_lexer.hpp"
#include "vm/command_language/command_lowering.hpp"
#include "vm/command_language/command_parser.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/other_device.hpp"
#include "vm/vm.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    /// a comment, a file-level variable, a function with a branch, a function that passes a
    ///  text to a driver, and a statement that calls the first function. report is never
    ///  called, so the program runs without a syscall and its result can be read back
    constexpr std::string_view kSource = R"(# picks the larger of two numbers
let limit = 100000

fn larger(a, b) {
  if (a > b) { return a }
  return b
}

fn report(text) {
  core log text 2
}

let best = larger(300, limit)
)";

    struct expected_token {
      command_token_type type;
      std::string_view text;
    };

    /// the comment and the blank lines leave nothing behind; each line that has something ends in one newline
    // clang-format off
    constexpr expected_token kTokens[] = {
      { CMD_TOKEN_KW_LET, "let" },      { CMD_TOKEN_NAME, "limit" },      { CMD_TOKEN_ASSIGN, "=" },       { CMD_TOKEN_INTEGER, "100000" },
      { CMD_TOKEN_NEWLINE, "\n" },

      { CMD_TOKEN_KW_FN, "fn" },        { CMD_TOKEN_NAME, "larger" },     { CMD_TOKEN_LEFT_PAREN, "(" },   { CMD_TOKEN_NAME, "a" },
      { CMD_TOKEN_COMMA, "," },         { CMD_TOKEN_NAME, "b" },          { CMD_TOKEN_RIGHT_PAREN, ")" },  { CMD_TOKEN_LEFT_BRACE, "{" },
      { CMD_TOKEN_NEWLINE, "\n" },

      { CMD_TOKEN_KW_IF, "if" },        { CMD_TOKEN_LEFT_PAREN, "(" },    { CMD_TOKEN_NAME, "a" },         { CMD_TOKEN_GREATER, ">" },
      { CMD_TOKEN_NAME, "b" },          { CMD_TOKEN_RIGHT_PAREN, ")" },   { CMD_TOKEN_LEFT_BRACE, "{" },   { CMD_TOKEN_KW_RETURN, "return" },
      { CMD_TOKEN_NAME, "a" },          { CMD_TOKEN_RIGHT_BRACE, "}" },   { CMD_TOKEN_NEWLINE, "\n" },

      { CMD_TOKEN_KW_RETURN, "return" }, { CMD_TOKEN_NAME, "b" },         { CMD_TOKEN_NEWLINE, "\n" },

      { CMD_TOKEN_RIGHT_BRACE, "}" },   { CMD_TOKEN_NEWLINE, "\n" },

      { CMD_TOKEN_KW_FN, "fn" },        { CMD_TOKEN_NAME, "report" },     { CMD_TOKEN_LEFT_PAREN, "(" },   { CMD_TOKEN_NAME, "text" },
      { CMD_TOKEN_RIGHT_PAREN, ")" },   { CMD_TOKEN_LEFT_BRACE, "{" },    { CMD_TOKEN_NEWLINE, "\n" },

      { CMD_TOKEN_NAME, "core" },       { CMD_TOKEN_NAME, "log" },        { CMD_TOKEN_NAME, "text" },      { CMD_TOKEN_INTEGER, "2" },
      { CMD_TOKEN_NEWLINE, "\n" },

      { CMD_TOKEN_RIGHT_BRACE, "}" },   { CMD_TOKEN_NEWLINE, "\n" },

      { CMD_TOKEN_KW_LET, "let" },      { CMD_TOKEN_NAME, "best" },       { CMD_TOKEN_ASSIGN, "=" },       { CMD_TOKEN_NAME, "larger" },
      { CMD_TOKEN_LEFT_PAREN, "(" },    { CMD_TOKEN_INTEGER, "300" },     { CMD_TOKEN_COMMA, "," },        { CMD_TOKEN_NAME, "limit" },
      { CMD_TOKEN_RIGHT_PAREN, ")" },   { CMD_TOKEN_NEWLINE, "\n" },

      { CMD_TOKEN_EOF, "" },
    };
    // clang-format on

    /// the tree, written the way to_text below writes it: functions first, then the statements
    ///  outside them, in the order they are in the file
    constexpr std::string_view kTree =
      "(fn larger (a b) (if (> a b) (then (return a))) (return b))\n"
      "(fn report (text) (command core log text 2))\n"
      "(let limit 100000)\n"
      "(let best (call larger 300 limit))\n";

    constexpr std::string_view kAssembly = R"(; generated from pipeline.ocmd
/entry: m_pipeline

#d_pipeline {
  .v_limit : = 100000
  .v_best : = 0
  .l_0_a : = 0
  .l_1_b : = 0
  .l_2_text : = 0
}

$f_larger:
  write r0, d_pipeline.l_0_a
  write r1, d_pipeline.l_1_b
  set r8, [d_pipeline.l_0_a]
  set r9, [d_pipeline.l_1_b]
  cmpgt r8, r9, r8
  mov r8, rflag
  je l_0
  set r8, [d_pipeline.l_0_a]
  mov r8, rf
  set rflag, 0
  ret
@l_0:
  set r8, [d_pipeline.l_1_b]
  mov r8, rf
  set rflag, 0
  ret
  set rf, 0
  set rflag, 0
  ret
end

$f_report:
  write r0, d_pipeline.l_2_text
  set r0, [d_pipeline.l_2_text]
  mov r0, r1
  set re, 32
  rshift r1, re
  lshift r0, re
  rshift r0, re
  set r2, 2
  set rflag, 0
  syscall core.log
  set rf, 0
  set rflag, 0
  ret
end

$m_pipeline:
  set r0, 0
  add r0, 300
  set r1, [d_pipeline.v_limit]
  call f_larger
  mov rf, r8
  write r8, d_pipeline.v_best
  stopdev
end
)";

    /// where the linker puts things. the code is 41 instructions, then five 8-byte fields:
    ///  v_limit, v_best, l_0_a, l_1_b, l_2_text
    constexpr uint16_t kEntry = 0x007c;
    constexpr uint16_t kDataStart = 0x00a4;
    constexpr uint16_t kProgramSize = 0x00cc;
    constexpr uint16_t kBestField = 0x00ac;

    constexpr uint32_t kCode[] = {
      /// f_larger: store a and b, compare, and leave by one of two returns
      0x110000b4,  // 0000  write r0, l_0_a
      0x110100bc,  // 0004  write r1, l_1_b
      0x140800b4,  // 0008  set r8, [l_0_a]
      0x140900bc,  // 000c  set r9, [l_1_b]
      0x41080908,  // 0010  cmpgt r8, r9, r8
      0x10081000,  // 0014  mov r8, rflag
      0x2100002c,  // 0018  je 0x002c
      0x140800b4,  // 001c  set r8, [l_0_a]
      0x10080f00,  // 0020  mov r8, rf
      0x15100000,  // 0024  set rflag, 0
      0x24000000,  // 0028  ret
      0x140800bc,  // 002c  set r8, [l_1_b]
      0x10080f00,  // 0030  mov r8, rf
      0x15100000,  // 0034  set rflag, 0
      0x24000000,  // 0038  ret
      0x150f0000,  // 003c  set rf, 0
      0x15100000,  // 0040  set rflag, 0
      0x24000000,  // 0044  ret

      /// f_report: split the text into its address and length, then the syscall
      0x110000c4,  // 0048  write r0, l_2_text
      0x140000c4,  // 004c  set r0, [l_2_text]
      0x10000100,  // 0050  mov r0, r1
      0x150e0020,  // 0054  set re, 32
      0x48010e01,  // 0058  rshift r1, re
      0x47000e00,  // 005c  lshift r0, re
      0x48000e00,  // 0060  rshift r0, re
      0x15020002,  // 0064  set r2, 2
      0x15100000,  // 0068  set rflag, 0
      0x26000000,  // 006c  syscall core.log
      0x150f0000,  // 0070  set rf, 0
      0x15100000,  // 0074  set rflag, 0
      0x24000000,  // 0078  ret

      /// m_pipeline, the entry: 300 is too large for 'set', so it is 0 plus 300
      0x15000000,  // 007c  set r0, 0
      0x3500012c,  // 0080  add r0, 300
      0x140100a4,  // 0084  set r1, [v_limit]
      0x23000000,  // 0088  call 0x0000
      0x100f0800,  // 008c  mov rf, r8
      0x110800ac,  // 0090  write r8, v_best
      0x00000000,  // 0094  stopdev, as the script's last line
      0x00000000,  // 0098  stopdev, the block's closing ret turned into one
      0x00000000,  // 009c  stopdev, added to every entry block
      0x00000000,  // 00a0  stopdev, the guard the linker puts after the code
    };

    /// v_limit holds 100000 from the start; the other four fields start at 0
    constexpr uint8_t kData[kProgramSize - kDataStart] = { 0xa0, 0x86, 0x01 };

    std::string to_text(const command_expr& expr) {
      std::string operands;
      for (const command_expr& operand : expr.operands) {
        operands += std::format(" {}", to_text(operand));
      }

      switch (expr.kind) {
        case CMD_EXPR_INTEGER: return std::format("{}", expr.integer);
        case CMD_EXPR_STRING: return std::format("\"{}\"", expr.text);
        case CMD_EXPR_BOOL: return expr.integer != 0 ? "true" : "false";
        case CMD_EXPR_NULL: return "null";
        case CMD_EXPR_NAME: return expr.text;
        case CMD_EXPR_UNARY: return std::format("({}{})", expr.op == CMD_TOKEN_BANG ? "!" : "-", operands);
        case CMD_EXPR_BINARY: {
          std::string_view op = "?";
          switch (expr.op) {
            case CMD_TOKEN_PLUS: op = "+"; break;
            case CMD_TOKEN_MINUS: op = "-"; break;
            case CMD_TOKEN_STAR: op = "*"; break;
            case CMD_TOKEN_SLASH: op = "/"; break;
            case CMD_TOKEN_PERCENT: op = "%"; break;
            case CMD_TOKEN_EQUAL_EQUAL: op = "=="; break;
            case CMD_TOKEN_BANG_EQUAL: op = "!="; break;
            case CMD_TOKEN_LESS: op = "<"; break;
            case CMD_TOKEN_LESS_EQUAL: op = "<="; break;
            case CMD_TOKEN_GREATER: op = ">"; break;
            case CMD_TOKEN_GREATER_EQUAL: op = ">="; break;
            case CMD_TOKEN_AND_AND: op = "&&"; break;
            case CMD_TOKEN_OR_OR: op = "||"; break;
            default: break;
          }
          return std::format("({}{})", op, operands);
        }
        case CMD_EXPR_CALL: {
          /// 'command' is the form written as words, 'call' the form with parentheses
          const std::string head = expr.member.empty() ? expr.text : std::format("{}.{}", expr.text, expr.member);
          return std::format("({} {}{})", expr.word_form ? "command" : "call", head, operands);
        }
        default: return "?";
      }
    }

    std::string to_text(const command_stmt& stmt);

    std::string to_text(const std::span<const command_stmt> statements) {
      std::string text;
      for (const command_stmt& stmt : statements) {
        text += std::format(" {}", to_text(stmt));
      }
      return text;
    }

    std::string to_text(const command_stmt& stmt) {
      switch (stmt.kind) {
        case CMD_STMT_LET: return std::format("(let {} {})", stmt.name, to_text(stmt.exprs.front()));
        case CMD_STMT_ASSIGN: return std::format("(assign {} {})", stmt.name, to_text(stmt.exprs.front()));
        case CMD_STMT_COMMANDS: {
          std::string text = to_text(stmt.exprs.front());
          for (size_t i = 1; i < stmt.exprs.size(); ++i) {
            text += std::format(" {} {}", stmt.joins[i - 1] == CMD_TOKEN_AND_AND ? "&&" : "||", to_text(stmt.exprs[i]));
          }
          return text;
        }
        case CMD_STMT_IF:
          return stmt.else_body.empty() ? std::format("(if {} (then{}))", to_text(stmt.exprs.front()), to_text(stmt.body)) : std::format("(if {} (then{}) (else{}))", to_text(stmt.exprs.front()), to_text(stmt.body), to_text(stmt.else_body));
        case CMD_STMT_WHILE: return std::format("(while {}{})", to_text(stmt.exprs.front()), to_text(stmt.body));
        case CMD_STMT_RETURN: return stmt.exprs.empty() ? "(return)" : std::format("(return {})", to_text(stmt.exprs.front()));
        case CMD_STMT_BREAK: return "(break)";
        case CMD_STMT_CONTINUE: return "(continue)";
        default: return "?";
      }
    }

    std::string to_text(const command_file& file) {
      std::string text;
      for (const command_use& use : file.uses) {
        text += std::format("(use \"{}\")\n", use.path);
      }
      for (const command_function& function : file.functions) {
        std::string params;
        for (const std::string& param : function.params) {
          params += std::format("{}{}", params.empty() ? "" : " ", param);
        }
        text += std::format("(fn {} ({}){})\n", function.name, params, to_text(function.body));
      }
      for (const command_hook& hook : file.hooks) {
        text += std::format("(on {}{}{})\n", hook.engine_event ? "event " : "", hook.name, to_text(hook.body));
      }
      for (const command_stmt& stmt : file.statements) {
        text += std::format("{}\n", to_text(stmt));
      }
      return text;
    }

  }  // namespace

  class command_pipeline_tests : public other_test {};

  TEST_F(command_pipeline_tests, a_script_is_exactly_this_at_every_stage) {
    diagnostic_engine diag;

    /// stage 1: text to tokens
    command_lexer lexer{ kSource };
    const ostd::vector<command_token> tokens = lexer.tokenize(&diag);
    ASSERT_FALSE(lexer.failed());
    ASSERT_EQ(tokens.size(), std::size(kTokens));
    for (size_t i = 0; i < tokens.size(); ++i) {
      EXPECT_EQ(tokens[i].type, kTokens[i].type) << "token " << i << " is '" << tokens[i].text << "'";
      EXPECT_EQ(tokens[i].text, kTokens[i].text) << "token " << i;
    }
    /// the numbers are read while lexing; the three in the script are tokens 3, 40 and 49
    EXPECT_EQ(tokens[3].value, 100000);
    EXPECT_EQ(tokens[40].value, 2);
    EXPECT_EQ(tokens[49].value, 300);
    /// a token knows where it was: 'best' is the second word of line 13
    EXPECT_EQ(tokens[45].span.start.line, 13);
    EXPECT_EQ(tokens[45].span.start.column, 5);

    /// stage 2: tokens to a tree
    const command_file file = command_parser{ tokens }.parse(&diag);
    ASSERT_TRUE(file.valid);
    EXPECT_EQ(to_text(file), kTree);

    /// stage 3: the tree to assembly, against the devices a driver registers
    command_bus bus;
    register_builtin_command_devices(bus);
    const command_lowering_result lowered = command_lowering{ bus, "pipeline.ocmd", {} }.lower(file, &diag);
    ASSERT_TRUE(lowered.success);
    EXPECT_EQ(lowered.oasm, kAssembly);
    EXPECT_TRUE(lowered.hooks.empty());

    /// stage 4: assembly to a linked program. without the debug section the file is the header,
    ///  the code and the data, so every byte of it is accounted for below
    const oasm_source source = { .name = "pipeline.ocmd", .text = lowered.oasm, .origins = lowered.origins };
    const obin_build_result built = ocmd_toolchain{}.build_obin(bus, std::span{ &source, 1 }, &diag, { .debug_info = false });
    ASSERT_TRUE(built.success);
    ASSERT_EQ(built.bytes.size(), sizeof(ocmd_file_header) + kProgramSize);

    ocmd_file_header header;
    std::memcpy(&header, built.bytes.data(), sizeof(header));
    EXPECT_EQ(header.prog_header.code_section_offset, 0);
    EXPECT_EQ(header.prog_header.data_section_offset, kDataStart);
    EXPECT_EQ(header.prog_header.entry_point_address, kEntry);
    EXPECT_EQ(header.prog_header.program_size, kProgramSize);
    EXPECT_EQ(header.prog_header.has_code_flag, 1);
    EXPECT_EQ(header.prog_header.has_debug_flag, 0);

    const std::span<const uint8_t> program = std::span<const uint8_t>{ built.bytes }.subspan(sizeof(ocmd_file_header));
    ASSERT_EQ(std::size(kCode) * sizeof(uint32_t), kDataStart);
    for (size_t i = 0; i < std::size(kCode); ++i) {
      uint32_t word = 0;
      std::memcpy(&word, program.data() + i * sizeof(word), sizeof(word));
      EXPECT_EQ(word, kCode[i]) << std::format("instruction at {:#06x} is {:#010x}, expected {:#010x}", i * sizeof(word), word, kCode[i]);
    }
    EXPECT_TRUE(std::ranges::equal(program.subspan(kDataStart), kData));

    /// stage 5: the program run to its end. larger(300, 100000) is 100000: it is what the call
    ///  gave back, and what the last statement stored in best
    other_command_device device;
    vm::initialize_device(&device);
    const size_t load_address = device.program_load_cursor;
    vm::load_program_from_bytes(&device, built.bytes);
    for (size_t step = 0; step < 4096 && !vm::has_flag(&device, other_command_device::STOPPED); ++step) {
      vm::step(&device);
    }
    ASSERT_TRUE(vm::has_flag(&device, other_command_device::STOPPED));
    EXPECT_EQ(device.read_register_as_u64(vm_register_idx::VM_RF), 100000);

    uint64_t best = 0;
    for (size_t i = 0; i < sizeof(best); ++i) {
      best |= static_cast<uint64_t>(device.memory->at(load_address + kBestField + i)) << (8 * i);
    }
    EXPECT_EQ(best, 100000);
    vm::shutdown_device(&device);
  }

}  // namespace other
