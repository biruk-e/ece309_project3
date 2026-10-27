// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"

namespace {

using Kind = CalcToken::Kind;

int precedence(Kind op) {
    if (op == Kind::Times || op == Kind::Divide) {
        return 2;
    }

    if (op == Kind::Plus || op == Kind::Minus) {
        return 1;
    }

    return 0;
}

bool apply_operator(std::vector<double>& values,
                    std::vector<Kind>& ops,
                    std::string* error) {
    if (ops.empty() || values.size() < 2) {
        *error = "not enough operands or operators";
        return false;
    }

    Kind op = ops.back();
    ops.pop_back();

    double right = values.back();
    values.pop_back();

    double left = values.back();
    values.pop_back();

    double result = 0.0;

    switch (op) {
        case Kind::Plus:
            result = left + right;
            break;

        case Kind::Minus:
            result = left - right;
            break;

        case Kind::Times:
            result = left * right;
            break;

        case Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;

        default:
            *error = "invalid operator";
            return false;
    }

    values.push_back(result);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens,
                   double* out, std::string* error) {
    std::vector<double> values;
    std::vector<Kind> ops;

    for (const auto& token : tokens) {
        if (token.kind == Kind::Number) {
            values.push_back(token.value);

        } else if (token.kind == Kind::LParen) {
            ops.push_back(token.kind);

        } else if (token.kind == Kind::RParen) {
            while (!ops.empty() && ops.back() != Kind::LParen) {
                if (!apply_operator(values, ops, error)) {
                    return false;
                }
            }

            if (ops.empty()) {
                *error = "unmatched closing parenthesis";
                return false;
            }

            ops.pop_back();

        } else {
            while (!ops.empty() &&
                   ops.back() != Kind::LParen &&
                   precedence(ops.back()) >= precedence(token.kind)) {
                if (!apply_operator(values, ops, error)) {
                    return false;
                }
            }

            ops.push_back(token.kind);
        }
    }

    while (!ops.empty()) {
        if (!apply_operator(values, ops, error)) {
            return false;
        }
    }

    if (values.size() != 1) {
        *error = "invalid expression";
        return false;
    }

    *out = values.back();
    error->clear();
    return true;
}