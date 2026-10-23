#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <iomanip>
#include <cassert>
#include <cstdint>

namespace toy {

// ============================================================================
// 1. LEXER (TOKENIZER)
// ============================================================================

enum class TokenType {
    TOK_EOF,
    TOK_INT_LITERAL,
    TOK_IDENT,
    
    // Keywords
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_PRINT,
    
    // Operators
    TOK_PLUS,         // +
    TOK_MINUS,        // -
    TOK_STAR,         // *
    TOK_SLASH,        // /
    TOK_PERCENT,      // %
    TOK_ASSIGN,       // =
    TOK_EQ,           // ==
    TOK_NE,           // !=
    TOK_LT,           // <
    TOK_LE,           // <=
    TOK_GT,           // >
    TOK_GE,           // >=
    TOK_AND,          // &&
    TOK_OR,           // ||
    TOK_NOT,          // !
    
    // Delimiters
    TOK_LPAREN,       // (
    TOK_RPAREN,       // )
    TOK_LBRACE,       // {
    TOK_RBRACE,       // }
    TOK_SEMICOLON     // ;
};

struct Token {
    TokenType type;
    std::string text;
    int64_t int_val = 0;
    int line = 1;
    int col = 1;
};

class Lexer {
public:
    explicit Lexer(std::string source) : src_(std::move(source)) {}

    Token next_token() {
        skip_whitespace_and_comments();
        if (pos_ >= src_.size()) {
            return Token{TokenType::TOK_EOF, "", 0, line_, col_};
        }

        int start_col = col_;
        char c = src_[pos_];

        // Number literal
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = pos_;
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                advance();
            }
            std::string text = src_.substr(start, pos_ - start);
            int64_t val = std::stoll(text);
            return Token{TokenType::TOK_INT_LITERAL, text, val, line_, start_col};
        }

        // Identifiers and Keywords
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = pos_;
            while (pos_ < src_.size() && (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_')) {
                advance();
            }
            std::string text = src_.substr(start, pos_ - start);
            TokenType type = TokenType::TOK_IDENT;
            if (text == "if") type = TokenType::TOK_IF;
            else if (text == "else") type = TokenType::TOK_ELSE;
            else if (text == "while") type = TokenType::TOK_WHILE;
            else if (text == "print") type = TokenType::TOK_PRINT;

            return Token{type, text, 0, line_, start_col};
        }

        // Two-character operators
        if (pos_ + 1 < src_.size()) {
            std::string two = src_.substr(pos_, 2);
            if (two == "==") { advance(); advance(); return Token{TokenType::TOK_EQ, "==", 0, line_, start_col}; }
            if (two == "!=") { advance(); advance(); return Token{TokenType::TOK_NE, "!=", 0, line_, start_col}; }
            if (two == "<=") { advance(); advance(); return Token{TokenType::TOK_LE, "<=", 0, line_, start_col}; }
            if (two == ">=") { advance(); advance(); return Token{TokenType::TOK_GE, ">=", 0, line_, start_col}; }
            if (two == "&&") { advance(); advance(); return Token{TokenType::TOK_AND, "&&", 0, line_, start_col}; }
            if (two == "||") { advance(); advance(); return Token{TokenType::TOK_OR, "||", 0, line_, start_col}; }
        }

        // Single-character operators and delimiters
        advance();
        switch (c) {
            case '+': return Token{TokenType::TOK_PLUS, "+", 0, line_, start_col};
            case '-': return Token{TokenType::TOK_MINUS, "-", 0, line_, start_col};
            case '*': return Token{TokenType::TOK_STAR, "*", 0, line_, start_col};
            case '/': return Token{TokenType::TOK_SLASH, "/", 0, line_, start_col};
            case '%': return Token{TokenType::TOK_PERCENT, "%", 0, line_, start_col};
            case '=': return Token{TokenType::TOK_ASSIGN, "=", 0, line_, start_col};
            case '<': return Token{TokenType::TOK_LT, "<", 0, line_, start_col};
            case '>': return Token{TokenType::TOK_GT, ">", 0, line_, start_col};
            case '!': return Token{TokenType::TOK_NOT, "!", 0, line_, start_col};
            case '(': return Token{TokenType::TOK_LPAREN, "(", 0, line_, start_col};
            case ')': return Token{TokenType::TOK_RPAREN, ")", 0, line_, start_col};
            case '{': return Token{TokenType::TOK_LBRACE, "{", 0, line_, start_col};
            case '}': return Token{TokenType::TOK_RBRACE, "}", 0, line_, start_col};
            case ';': return Token{TokenType::TOK_SEMICOLON, ";", 0, line_, start_col};
            default:
                throw std::runtime_error("Lexer error at line " + std::to_string(line_) +
                                         ", col " + std::to_string(start_col) +
                                         ": Unexpected character '" + std::string(1, c) + "'");
        }
    }

    std::vector<Token> tokenize_all() {
        std::vector<Token> tokens;
        while (true) {
            Token t = next_token();
            tokens.push_back(t);
            if (t.type == TokenType::TOK_EOF) break;
        }
        return tokens;
    }

private:
    std::string src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;

    void advance() {
        if (pos_ < src_.size()) {
            if (src_[pos_] == '\n') {
                line_++;
                col_ = 1;
            } else {
                col_++;
            }
            pos_++;
        }
    }

    void skip_whitespace_and_comments() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (std::isspace(static_cast<unsigned char>(c))) {
                advance();
            } else if (c == '/' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '/') {
                // Line comment
                while (pos_ < src_.size() && src_[pos_] != '\n') {
                    advance();
                }
            } else {
                break;
            }
        }
    }
};

// ============================================================================
// 2. ABSTRACT SYNTAX TREE (AST)
// ============================================================================

enum class BinaryOp {
    ADD, SUB, MUL, DIV, MOD,
    EQ, NE, LT, LE, GT, GE,
    AND, OR
};

enum class UnaryOp {
    NEG, NOT
};

struct ASTNode {
    virtual ~ASTNode() = default;
};

struct ExprAST : public ASTNode {};

struct LiteralExprAST : public ExprAST {
    int64_t val;
    explicit LiteralExprAST(int64_t v) : val(v) {}
};

struct VarExprAST : public ExprAST {
    std::string name;
    explicit VarExprAST(std::string n) : name(std::move(n)) {}
};

struct BinaryExprAST : public ExprAST {
    BinaryOp op;
    std::unique_ptr<ExprAST> lhs;
    std::unique_ptr<ExprAST> rhs;
    BinaryExprAST(BinaryOp o, std::unique_ptr<ExprAST> l, std::unique_ptr<ExprAST> r)
        : op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct UnaryExprAST : public ExprAST {
    UnaryOp op;
    std::unique_ptr<ExprAST> operand;
    UnaryExprAST(UnaryOp o, std::unique_ptr<ExprAST> opnd)
        : op(o), operand(std::move(opnd)) {}
};

struct StmtAST : public ASTNode {};

struct AssignStmtAST : public StmtAST {
    std::string var_name;
    std::unique_ptr<ExprAST> expr;
    AssignStmtAST(std::string n, std::unique_ptr<ExprAST> e)
        : var_name(std::move(n)), expr(std::move(e)) {}
};

struct PrintStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> expr;
    explicit PrintStmtAST(std::unique_ptr<ExprAST> e) : expr(std::move(e)) {}
};

struct BlockStmtAST : public StmtAST {
    std::vector<std::unique_ptr<StmtAST>> stmts;
};

struct IfStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> cond;
    std::unique_ptr<StmtAST> then_branch;
    std::unique_ptr<StmtAST> else_branch; // optional (nullptr if no else)
    IfStmtAST(std::unique_ptr<ExprAST> c, std::unique_ptr<StmtAST> t, std::unique_ptr<StmtAST> e = nullptr)
        : cond(std::move(c)), then_branch(std::move(t)), else_branch(std::move(e)) {}
};

struct WhileStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> cond;
    std::unique_ptr<StmtAST> body;
    WhileStmtAST(std::unique_ptr<ExprAST> c, std::unique_ptr<StmtAST> b)
        : cond(std::move(c)), body(std::move(b)) {}
};

struct ProgramAST {
    std::vector<std::unique_ptr<StmtAST>> statements;
};

// ============================================================================
// 3. RECURSIVE-DESCENT PARSER
// ============================================================================

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    ProgramAST parse_program() {
        ProgramAST prog;
        while (!is_at_end()) {
            prog.statements.push_back(parse_statement());
        }
        return prog;
    }

private:
    std::vector<Token> tokens_;
    size_t cursor_ = 0;

    const Token& peek() const {
        return tokens_[cursor_];
    }

    const Token& previous() const {
        return tokens_[cursor_ - 1];
    }

    bool check(TokenType type) const {
        if (cursor_ >= tokens_.size()) return false;
        return peek().type == type;
    }

    bool match(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    bool is_at_end() const {
        return cursor_ >= tokens_.size() || peek().type == TokenType::TOK_EOF;
    }

    const Token& advance() {
        if (!is_at_end()) cursor_++;
        return previous();
    }

    const Token& consume(TokenType type, const std::string& message) {
        if (check(type)) return advance();
        const auto& tok = peek();
        throw std::runtime_error("Parser error at line " + std::to_string(tok.line) +
                                 ", col " + std::to_string(tok.col) + ": " + message +
                                 " (found '" + tok.text + "')");
    }

    // Statements
    std::unique_ptr<StmtAST> parse_statement() {
        if (match(TokenType::TOK_PRINT)) {
            return parse_print_statement();
        }
        if (match(TokenType::TOK_IF)) {
            return parse_if_statement();
        }
        if (match(TokenType::TOK_WHILE)) {
            return parse_while_statement();
        }
        if (match(TokenType::TOK_LBRACE)) {
            return parse_block_statement();
        }
        return parse_assignment_statement();
    }

    std::unique_ptr<StmtAST> parse_print_statement() {
        bool has_paren = match(TokenType::TOK_LPAREN);
        auto expr = parse_expression();
        if (has_paren) {
            consume(TokenType::TOK_RPAREN, "Expected ')' after print expression");
        }
        consume(TokenType::TOK_SEMICOLON, "Expected ';' after print statement");
        return std::make_unique<PrintStmtAST>(std::move(expr));
    }

    std::unique_ptr<StmtAST> parse_assignment_statement() {
        const Token& var_tok = consume(TokenType::TOK_IDENT, "Expected variable identifier");
        consume(TokenType::TOK_ASSIGN, "Expected '=' in assignment");
        auto expr = parse_expression();
        consume(TokenType::TOK_SEMICOLON, "Expected ';' after assignment");
        return std::make_unique<AssignStmtAST>(var_tok.text, std::move(expr));
    }

    std::unique_ptr<StmtAST> parse_block_statement() {
        auto block = std::make_unique<BlockStmtAST>();
        while (!check(TokenType::TOK_RBRACE) && !is_at_end()) {
            block->stmts.push_back(parse_statement());
        }
        consume(TokenType::TOK_RBRACE, "Expected '}' at end of block");
        return block;
    }

    std::unique_ptr<StmtAST> parse_if_statement() {
        consume(TokenType::TOK_LPAREN, "Expected '(' after 'if'");
        auto cond = parse_expression();
        consume(TokenType::TOK_RPAREN, "Expected ')' after if condition");

        auto then_branch = parse_statement();
        std::unique_ptr<StmtAST> else_branch = nullptr;
        if (match(TokenType::TOK_ELSE)) {
            else_branch = parse_statement();
        }
        return std::make_unique<IfStmtAST>(std::move(cond), std::move(then_branch), std::move(else_branch));
    }

    std::unique_ptr<StmtAST> parse_while_statement() {
        consume(TokenType::TOK_LPAREN, "Expected '(' after 'while'");
        auto cond = parse_expression();
        consume(TokenType::TOK_RPAREN, "Expected ')' after while condition");

        auto body = parse_statement();
        return std::make_unique<WhileStmtAST>(std::move(cond), std::move(body));
    }

    // Expressions (Operator Precedence Climbing)
    std::unique_ptr<ExprAST> parse_expression() {
        return parse_logical_or();
    }

    std::unique_ptr<ExprAST> parse_logical_or() {
        auto expr = parse_logical_and();
        while (match(TokenType::TOK_OR)) {
            auto rhs = parse_logical_and();
            expr = std::make_unique<BinaryExprAST>(BinaryOp::OR, std::move(expr), std::move(rhs));
        }
        return expr;
    }

    std::unique_ptr<ExprAST> parse_logical_and() {
        auto expr = parse_equality();
        while (match(TokenType::TOK_AND)) {
            auto rhs = parse_equality();
            expr = std::make_unique<BinaryExprAST>(BinaryOp::AND, std::move(expr), std::move(rhs));
        }
        return expr;
    }

    std::unique_ptr<ExprAST> parse_equality() {
        auto expr = parse_relational();
        while (true) {
            if (match(TokenType::TOK_EQ)) {
                auto rhs = parse_relational();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::EQ, std::move(expr), std::move(rhs));
            } else if (match(TokenType::TOK_NE)) {
                auto rhs = parse_relational();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::NE, std::move(expr), std::move(rhs));
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<ExprAST> parse_relational() {
        auto expr = parse_additive();
        while (true) {
            if (match(TokenType::TOK_LT)) {
                auto rhs = parse_additive();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::LT, std::move(expr), std::move(rhs));
            } else if (match(TokenType::TOK_LE)) {
                auto rhs = parse_additive();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::LE, std::move(expr), std::move(rhs));
            } else if (match(TokenType::TOK_GT)) {
                auto rhs = parse_additive();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::GT, std::move(expr), std::move(rhs));
            } else if (match(TokenType::TOK_GE)) {
                auto rhs = parse_additive();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::GE, std::move(expr), std::move(rhs));
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<ExprAST> parse_additive() {
        auto expr = parse_multiplicative();
        while (true) {
            if (match(TokenType::TOK_PLUS)) {
                auto rhs = parse_multiplicative();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::ADD, std::move(expr), std::move(rhs));
            } else if (match(TokenType::TOK_MINUS)) {
                auto rhs = parse_multiplicative();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::SUB, std::move(expr), std::move(rhs));
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<ExprAST> parse_multiplicative() {
        auto expr = parse_unary();
        while (true) {
            if (match(TokenType::TOK_STAR)) {
                auto rhs = parse_unary();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::MUL, std::move(expr), std::move(rhs));
            } else if (match(TokenType::TOK_SLASH)) {
                auto rhs = parse_unary();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::DIV, std::move(expr), std::move(rhs));
            } else if (match(TokenType::TOK_PERCENT)) {
                auto rhs = parse_unary();
                expr = std::make_unique<BinaryExprAST>(BinaryOp::MOD, std::move(expr), std::move(rhs));
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<ExprAST> parse_unary() {
        if (match(TokenType::TOK_MINUS)) {
            auto operand = parse_unary();
            return std::make_unique<UnaryExprAST>(UnaryOp::NEG, std::move(operand));
        }
        if (match(TokenType::TOK_NOT)) {
            auto operand = parse_unary();
            return std::make_unique<UnaryExprAST>(UnaryOp::NOT, std::move(operand));
        }
        return parse_primary();
    }

    std::unique_ptr<ExprAST> parse_primary() {
        if (match(TokenType::TOK_INT_LITERAL)) {
            return std::make_unique<LiteralExprAST>(previous().int_val);
        }
        if (match(TokenType::TOK_IDENT)) {
            return std::make_unique<VarExprAST>(previous().text);
        }
        if (match(TokenType::TOK_LPAREN)) {
            auto expr = parse_expression();
            consume(TokenType::TOK_RPAREN, "Expected ')' after grouped expression");
            return expr;
        }
        const auto& tok = peek();
        throw std::runtime_error("Parser error at line " + std::to_string(tok.line) +
                                 ", col " + std::to_string(tok.col) + ": Expected expression, found '" +
                                 tok.text + "'");
    }
};

// ============================================================================
// 4. THREE-ADDRESS-CODE (TAC) IR
// ============================================================================

enum class TACOp {
    ADD, SUB, MUL, DIV, MOD,
    EQ, NE, LT, LE, GT, GE,
    AND, OR, NOT, NEG,
    COPY,           // dst = src1
    CONST,          // dst = int_val
    LABEL,          // label:
    JUMP,           // jump target
    JUMP_IF_ZERO,   // jump target if src1 == 0
    PRINT           // print src1
};

inline std::string tac_op_to_string(TACOp op) {
    switch (op) {
        case TACOp::ADD: return "ADD";
        case TACOp::SUB: return "SUB";
        case TACOp::MUL: return "MUL";
        case TACOp::DIV: return "DIV";
        case TACOp::MOD: return "MOD";
        case TACOp::EQ: return "EQ";
        case TACOp::NE: return "NE";
        case TACOp::LT: return "LT";
        case TACOp::LE: return "LE";
        case TACOp::GT: return "GT";
        case TACOp::GE: return "GE";
        case TACOp::AND: return "AND";
        case TACOp::OR: return "OR";
        case TACOp::NOT: return "NOT";
        case TACOp::NEG: return "NEG";
        case TACOp::COPY: return "COPY";
        case TACOp::CONST: return "CONST";
        case TACOp::LABEL: return "LABEL";
        case TACOp::JUMP: return "JUMP";
        case TACOp::JUMP_IF_ZERO: return "JZ";
        case TACOp::PRINT: return "PRINT";
    }
    return "UNKNOWN";
}

struct TACInstruction {
    TACOp op;
    std::string dst;
    std::string src1;
    std::string src2;
    int64_t imm = 0;

    std::string to_string() const {
        std::ostringstream oss;
        if (op == TACOp::LABEL) {
            oss << dst << ":";
        } else if (op == TACOp::CONST) {
            oss << "  " << dst << " = " << imm;
        } else if (op == TACOp::COPY) {
            oss << "  " << dst << " = " << src1;
        } else if (op == TACOp::PRINT) {
            oss << "  PRINT " << src1;
        } else if (op == TACOp::JUMP) {
            oss << "  JUMP " << dst;
        } else if (op == TACOp::JUMP_IF_ZERO) {
            oss << "  JZ " << src1 << ", " << dst;
        } else if (op == TACOp::NOT || op == TACOp::NEG) {
            oss << "  " << dst << " = " << tac_op_to_string(op) << " " << src1;
        } else {
            oss << "  " << dst << " = " << src1 << " " << tac_op_to_string(op) << " " << src2;
        }
        return oss.str();
    }
};

class TACGenerator {
public:
    std::vector<TACInstruction> generate(const ProgramAST& prog) {
        instructions_.clear();
        temp_counter_ = 0;
        label_counter_ = 0;

        for (const auto& stmt : prog.statements) {
            generate_stmt(stmt.get());
        }
        return instructions_;
    }

private:
    std::vector<TACInstruction> instructions_;
    int temp_counter_ = 0;
    int label_counter_ = 0;

    std::string new_temp() {
        return "t" + std::to_string(temp_counter_++);
    }

    std::string new_label() {
        return "L" + std::to_string(label_counter_++);
    }

    void emit(TACInstruction instr) {
        instructions_.push_back(std::move(instr));
    }

    void generate_stmt(const StmtAST* stmt) {
        if (auto assign = dynamic_cast<const AssignStmtAST*>(stmt)) {
            std::string src = generate_expr(assign->expr.get());
            emit(TACInstruction{TACOp::COPY, assign->var_name, src, "", 0});
        } else if (auto pr = dynamic_cast<const PrintStmtAST*>(stmt)) {
            std::string src = generate_expr(pr->expr.get());
            emit(TACInstruction{TACOp::PRINT, "", src, "", 0});
        } else if (auto blk = dynamic_cast<const BlockStmtAST*>(stmt)) {
            for (const auto& s : blk->stmts) {
                generate_stmt(s.get());
            }
        } else if (auto ifs = dynamic_cast<const IfStmtAST*>(stmt)) {
            std::string cond_var = generate_expr(ifs->cond.get());
            std::string label_else = new_label();
            std::string label_end = new_label();

            emit(TACInstruction{TACOp::JUMP_IF_ZERO, label_else, cond_var, "", 0});
            generate_stmt(ifs->then_branch.get());

            if (ifs->else_branch) {
                emit(TACInstruction{TACOp::JUMP, label_end, "", "", 0});
                emit(TACInstruction{TACOp::LABEL, label_else, "", "", 0});
                generate_stmt(ifs->else_branch.get());
                emit(TACInstruction{TACOp::LABEL, label_end, "", "", 0});
            } else {
                emit(TACInstruction{TACOp::LABEL, label_else, "", "", 0});
            }
        } else if (auto wh = dynamic_cast<const WhileStmtAST*>(stmt)) {
            std::string label_start = new_label();
            std::string label_end = new_label();

            emit(TACInstruction{TACOp::LABEL, label_start, "", "", 0});
            std::string cond_var = generate_expr(wh->cond.get());
            emit(TACInstruction{TACOp::JUMP_IF_ZERO, label_end, cond_var, "", 0});

            generate_stmt(wh->body.get());
            emit(TACInstruction{TACOp::JUMP, label_start, "", "", 0});
            emit(TACInstruction{TACOp::LABEL, label_end, "", "", 0});
        }
    }

    std::string generate_expr(const ExprAST* expr) {
        if (auto lit = dynamic_cast<const LiteralExprAST*>(expr)) {
            std::string t = new_temp();
            emit(TACInstruction{TACOp::CONST, t, "", "", lit->val});
            return t;
        }
        if (auto var = dynamic_cast<const VarExprAST*>(expr)) {
            return var->name;
        }
        if (auto un = dynamic_cast<const UnaryExprAST*>(expr)) {
            std::string src = generate_expr(un->operand.get());
            std::string t = new_temp();
            TACOp op = (un->op == UnaryOp::NEG) ? TACOp::NEG : TACOp::NOT;
            emit(TACInstruction{op, t, src, "", 0});
            return t;
        }
        if (auto bin = dynamic_cast<const BinaryExprAST*>(expr)) {
            std::string left = generate_expr(bin->lhs.get());
            std::string right = generate_expr(bin->rhs.get());
            std::string t = new_temp();

            TACOp op;
            switch (bin->op) {
                case BinaryOp::ADD: op = TACOp::ADD; break;
                case BinaryOp::SUB: op = TACOp::SUB; break;
                case BinaryOp::MUL: op = TACOp::MUL; break;
                case BinaryOp::DIV: op = TACOp::DIV; break;
                case BinaryOp::MOD: op = TACOp::MOD; break;
                case BinaryOp::EQ:  op = TACOp::EQ; break;
                case BinaryOp::NE:  op = TACOp::NE; break;
                case BinaryOp::LT:  op = TACOp::LT; break;
                case BinaryOp::LE:  op = TACOp::LE; break;
                case BinaryOp::GT:  op = TACOp::GT; break;
                case BinaryOp::GE:  op = TACOp::GE; break;
                case BinaryOp::AND: op = TACOp::AND; break;
                case BinaryOp::OR:  op = TACOp::OR; break;
            }
            emit(TACInstruction{op, t, left, right, 0});
            return t;
        }
        throw std::runtime_error("TACGenerator: Unknown expression type.");
    }
};

// ============================================================================
// 5. STACK VIRTUAL MACHINE (BYTECODE & INTERPRETER)
// ============================================================================

enum class OpCode : uint8_t {
    PUSH_CONST,   // Push imm onto stack
    LOAD,         // Push value of var[operand]
    STORE,        // Pop top and store into var[operand]
    ADD, SUB, MUL, DIV, MOD,
    EQ, NE, LT, LE, GT, GE,
    AND, OR, NOT, NEG,
    JMP,          // Unconditional jump to target PC
    JZ,           // Pop top; jump to target PC if 0
    PRINT,        // Pop and print top of stack
    HALT          // Stop execution
};

inline std::string opcode_to_string(OpCode op) {
    switch (op) {
        case OpCode::PUSH_CONST: return "PUSH_CONST";
        case OpCode::LOAD: return "LOAD";
        case OpCode::STORE: return "STORE";
        case OpCode::ADD: return "ADD";
        case OpCode::SUB: return "SUB";
        case OpCode::MUL: return "MUL";
        case OpCode::DIV: return "DIV";
        case OpCode::MOD: return "MOD";
        case OpCode::EQ: return "EQ";
        case OpCode::NE: return "NE";
        case OpCode::LT: return "LT";
        case OpCode::LE: return "LE";
        case OpCode::GT: return "GT";
        case OpCode::GE: return "GE";
        case OpCode::AND: return "AND";
        case OpCode::OR: return "OR";
        case OpCode::NOT: return "NOT";
        case OpCode::NEG: return "NEG";
        case OpCode::JMP: return "JMP";
        case OpCode::JZ: return "JZ";
        case OpCode::PRINT: return "PRINT";
        case OpCode::HALT: return "HALT";
    }
    return "UNKNOWN";
}

struct VMInstruction {
    OpCode opcode;
    int64_t operand = 0; // immediate value, variable index, or jump target PC
};

class BytecodeEmitter {
public:
    struct CompiledProgram {
        std::vector<VMInstruction> code;
        std::vector<std::string> var_names;
    };

    CompiledProgram compile(const std::vector<TACInstruction>& tac_list) {
        CompiledProgram prog;
        std::unordered_map<std::string, int> var_indices;
        std::unordered_map<std::string, int> label_to_pc;
        std::vector<std::pair<size_t, std::string>> pending_jumps; // (instr_index, label_name)

        auto get_var_index = [&](const std::string& name) -> int {
            auto it = var_indices.find(name);
            if (it != var_indices.end()) return it->second;
            int idx = static_cast<int>(prog.var_names.size());
            var_indices[name] = idx;
            prog.var_names.push_back(name);
            return idx;
        };

        for (const auto& tac : tac_list) {
            if (tac.op == TACOp::LABEL) {
                label_to_pc[tac.dst] = static_cast<int>(prog.code.size());
                continue;
            }

            if (tac.op == TACOp::CONST) {
                prog.code.push_back({OpCode::PUSH_CONST, tac.imm});
                prog.code.push_back({OpCode::STORE, get_var_index(tac.dst)});
            } else if (tac.op == TACOp::COPY) {
                prog.code.push_back({OpCode::LOAD, get_var_index(tac.src1)});
                prog.code.push_back({OpCode::STORE, get_var_index(tac.dst)});
            } else if (tac.op == TACOp::PRINT) {
                prog.code.push_back({OpCode::LOAD, get_var_index(tac.src1)});
                prog.code.push_back({OpCode::PRINT, 0});
            } else if (tac.op == TACOp::JUMP) {
                pending_jumps.push_back({prog.code.size(), tac.dst});
                prog.code.push_back({OpCode::JMP, 0}); // target patched later
            } else if (tac.op == TACOp::JUMP_IF_ZERO) {
                prog.code.push_back({OpCode::LOAD, get_var_index(tac.src1)});
                pending_jumps.push_back({prog.code.size(), tac.dst});
                prog.code.push_back({OpCode::JZ, 0}); // target patched later
            } else if (tac.op == TACOp::NOT || tac.op == TACOp::NEG) {
                prog.code.push_back({OpCode::LOAD, get_var_index(tac.src1)});
                prog.code.push_back({(tac.op == TACOp::NOT) ? OpCode::NOT : OpCode::NEG, 0});
                prog.code.push_back({OpCode::STORE, get_var_index(tac.dst)});
            } else {
                // Binary operations: load src1, load src2, binary op, store dst
                prog.code.push_back({OpCode::LOAD, get_var_index(tac.src1)});
                prog.code.push_back({OpCode::LOAD, get_var_index(tac.src2)});

                OpCode opc;
                switch (tac.op) {
                    case TACOp::ADD: opc = OpCode::ADD; break;
                    case TACOp::SUB: opc = OpCode::SUB; break;
                    case TACOp::MUL: opc = OpCode::MUL; break;
                    case TACOp::DIV: opc = OpCode::DIV; break;
                    case TACOp::MOD: opc = OpCode::MOD; break;
                    case TACOp::EQ:  opc = OpCode::EQ; break;
                    case TACOp::NE:  opc = OpCode::NE; break;
                    case TACOp::LT:  opc = OpCode::LT; break;
                    case TACOp::LE:  opc = OpCode::LE; break;
                    case TACOp::GT:  opc = OpCode::GT; break;
                    case TACOp::GE:  opc = OpCode::GE; break;
                    case TACOp::AND: opc = OpCode::AND; break;
                    case TACOp::OR:  opc = OpCode::OR; break;
                    default: throw std::runtime_error("BytecodeEmitter: Unknown binary TAC op");
                }
                prog.code.push_back({opc, 0});
                prog.code.push_back({OpCode::STORE, get_var_index(tac.dst)});
            }
        }

        // Emit HALT at the end of the program
        prog.code.push_back({OpCode::HALT, 0});

        // Patch jump targets
        for (const auto& pj : pending_jumps) {
            auto it = label_to_pc.find(pj.second);
            if (it == label_to_pc.end()) {
                // Target could be end of program
                prog.code[pj.first].operand = static_cast<int64_t>(prog.code.size() - 1);
            } else {
                prog.code[pj.first].operand = it->second;
            }
        }

        return prog;
    }
};

class StackVM {
public:
    struct ExecutionResult {
        std::vector<int64_t> output;
        std::map<std::string, int64_t> final_variables;
        int64_t cycles_executed = 0;
    };

    ExecutionResult execute(const BytecodeEmitter::CompiledProgram& prog, int64_t max_cycles = 10000000) {
        ExecutionResult res;
        std::vector<int64_t> memory(prog.var_names.size(), 0);
        std::vector<int64_t> stack;
        stack.reserve(256);

        size_t pc = 0;
        int64_t cycles = 0;

        while (pc < prog.code.size()) {
            if (++cycles > max_cycles) {
                throw std::runtime_error("VM Execution error: Exceeded max execution cycles (" +
                                         std::to_string(max_cycles) + ") - possible infinite loop.");
            }

            const auto& instr = prog.code[pc++];
            switch (instr.opcode) {
                case OpCode::PUSH_CONST:
                    stack.push_back(instr.operand);
                    break;
                case OpCode::LOAD:
                    stack.push_back(memory[instr.operand]);
                    break;
                case OpCode::STORE: {
                    if (stack.empty()) throw std::runtime_error("VM Stack underflow on STORE");
                    memory[instr.operand] = stack.back();
                    stack.pop_back();
                    break;
                }
                case OpCode::ADD: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back(a + b);
                    break;
                }
                case OpCode::SUB: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back(a - b);
                    break;
                }
                case OpCode::MUL: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back(a * b);
                    break;
                }
                case OpCode::DIV: {
                    auto [a, b] = pop_two(stack);
                    if (b == 0) throw std::runtime_error("VM Runtime Error: Division by zero");
                    stack.push_back(a / b);
                    break;
                }
                case OpCode::MOD: {
                    auto [a, b] = pop_two(stack);
                    if (b == 0) throw std::runtime_error("VM Runtime Error: Modulo by zero");
                    stack.push_back(a % b);
                    break;
                }
                case OpCode::EQ: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back(a == b ? 1 : 0);
                    break;
                }
                case OpCode::NE: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back(a != b ? 1 : 0);
                    break;
                }
                case OpCode::LT: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back(a < b ? 1 : 0);
                    break;
                }
                case OpCode::LE: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back(a <= b ? 1 : 0);
                    break;
                }
                case OpCode::GT: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back(a > b ? 1 : 0);
                    break;
                }
                case OpCode::GE: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back(a >= b ? 1 : 0);
                    break;
                }
                case OpCode::AND: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back((a != 0 && b != 0) ? 1 : 0);
                    break;
                }
                case OpCode::OR: {
                    auto [a, b] = pop_two(stack);
                    stack.push_back((a != 0 || b != 0) ? 1 : 0);
                    break;
                }
                case OpCode::NOT: {
                    if (stack.empty()) throw std::runtime_error("VM Stack underflow on NOT");
                    int64_t v = stack.back();
                    stack.pop_back();
                    stack.push_back(v == 0 ? 1 : 0);
                    break;
                }
                case OpCode::NEG: {
                    if (stack.empty()) throw std::runtime_error("VM Stack underflow on NEG");
                    int64_t v = stack.back();
                    stack.pop_back();
                    stack.push_back(-v);
                    break;
                }
                case OpCode::JMP:
                    pc = static_cast<size_t>(instr.operand);
                    break;
                case OpCode::JZ: {
                    if (stack.empty()) throw std::runtime_error("VM Stack underflow on JZ");
                    int64_t cond = stack.back();
                    stack.pop_back();
                    if (cond == 0) {
                        pc = static_cast<size_t>(instr.operand);
                    }
                    break;
                }
                case OpCode::PRINT: {
                    if (stack.empty()) throw std::runtime_error("VM Stack underflow on PRINT");
                    int64_t val = stack.back();
                    stack.pop_back();
                    res.output.push_back(val);
                    break;
                }
                case OpCode::HALT:
                    pc = prog.code.size();
                    break;
            }
        }

        res.cycles_executed = cycles;
        for (size_t i = 0; i < prog.var_names.size(); ++i) {
            // Keep user-defined variables (ignore compiler generated t0, t1...)
            if (!prog.var_names[i].empty() && prog.var_names[i][0] != 't') {
                res.final_variables[prog.var_names[i]] = memory[i];
            }
        }
        return res;
    }

private:
    std::pair<int64_t, int64_t> pop_two(std::vector<int64_t>& stack) {
        if (stack.size() < 2) {
            throw std::runtime_error("VM Stack underflow on binary operator");
        }
        int64_t b = stack.back(); stack.pop_back();
        int64_t a = stack.back(); stack.pop_back();
        return {a, b};
    }
};

// ============================================================================
// 6. END-TO-END PIPELINE HELPER
// ============================================================================

struct CompilationArtifacts {
    std::vector<Token> tokens;
    ProgramAST ast;
    std::vector<TACInstruction> tac;
    BytecodeEmitter::CompiledProgram bytecode;
    StackVM::ExecutionResult execution;
};

inline CompilationArtifacts compile_and_run(const std::string& source) {
    CompilationArtifacts artifacts;
    
    // Lex
    Lexer lexer(source);
    artifacts.tokens = lexer.tokenize_all();

    // Parse
    Parser parser(artifacts.tokens);
    artifacts.ast = parser.parse_program();

    // TAC IR
    TACGenerator tac_gen;
    artifacts.tac = tac_gen.generate(artifacts.ast);

    // Bytecode
    BytecodeEmitter emitter;
    artifacts.bytecode = emitter.compile(artifacts.tac);

    // Interpret
    StackVM vm;
    artifacts.execution = vm.execute(artifacts.bytecode);

    return artifacts;
}

} // namespace toy
