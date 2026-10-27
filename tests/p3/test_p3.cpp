// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    Tool* first = nullptr;

    for (int i = 0; i < 10000; ++i) {
        std::string name = "tool_" + std::to_string(i);

        REQUIRE(r.add(std::make_unique<NamedTool>(name)));

        if (i == 0) {
            first = r.find(name);
        }

        CHECK(r.load_factor() <= 1.0);
    }

    REQUIRE(first != nullptr);
    CHECK(r.size() == 10000);
    CHECK(r.find("tool_0") == first);

    for (int i = 0; i < 10000; ++i) {
        std::string name = "tool_" + std::to_string(i);
        Tool* found = r.find(name);

        REQUIRE(found != nullptr);
        CHECK(found->name() == name);
    }

    CHECK(r.max_probe_length() <= 64);

    std::printf(
        "Registry: buckets=%zu, load=%.6f, longest chain=%zu\n",
        r.bucket_count(), r.load_factor(), r.max_probe_length());

    std::vector<int> weak_counts(r.bucket_count(), 0);
    int weak_longest = 0;

    for (int i = 0; i < 10000; ++i) {
        std::string name = "tool_" + std::to_string(i);
        std::size_t index =
            (name[0] + name.size()) % r.bucket_count();

        ++weak_counts[index];

        if (weak_counts[index] > weak_longest) {
            weak_longest = weak_counts[index];
        }
    }

    std::printf("Weak hash: longest chain=%d\n", weak_longest);
}

TEST(ws_accepts_safe_paths) {
    TempDir d;
    Workspace ws(d.path());

    struct Case {
        const char* input;
        fs::path expected;
    };

    const Case cases[] = {
        {"notes/todo.txt", ws.root() / "notes/todo.txt"},
        {"./notes//todo.txt", ws.root() / "notes/todo.txt"},
        {".", ws.root()},
        {"a/../b", ws.root() / "b"},
        {"a/..", ws.root()},
        {"new/dir/file.txt", ws.root() / "new/dir/file.txt"},
        {"my..notes.txt", ws.root() / "my..notes.txt"}
    };

    CHECK(!fs::exists(ws.root() / "new"));

    for (const auto& test : cases) {
        fs::path out;
        std::string reason;

        REQUIRE(ws.resolve(test.input, &out, &reason));
        CHECK(out == test.expected);
        CHECK(reason.empty());
    }

    CHECK(!fs::exists(ws.root() / "new"));
}

TEST(ws_rejects_unsafe_paths) {
    TempDir d;
    Workspace ws(d.path());

    const std::vector<std::string> paths = {
        "",
        std::string("notes") + '\0' + "/x",
        "/etc/passwd",
        "..",
        "../etc/passwd",
        "a/../../b",
        "notes/../../ws/notes/todo.txt",
        ws.root().string()
    };

    for (const auto& path : paths) {
        fs::path out;
        std::string reason;

        CHECK(!ws.resolve(path, &out, &reason));
        CHECK(!reason.empty());
    }
}

TEST(reg_empty) {
    ToolRegistry r;

    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1);
    CHECK(r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    CHECK(r.find("missing") == nullptr);
    CHECK(r.size() == 0);
}

TEST(reg_null_rejected) {
    ToolRegistry r;

    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);

    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    Tool* original = r.find("alpha");

    CHECK(!r.add(nullptr));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == original);

    CHECK(r.find("missing") == nullptr);
    CHECK(r.size() == 1);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;

    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    REQUIRE(r.add(std::make_unique<NamedTool>("beta")));
    REQUIRE(r.add(std::make_unique<NamedTool>("gamma")));

    std::vector<std::string> visited;

    r.for_each([&](const Tool& tool) {
        visited.push_back(tool.name());
    });

    CHECK(visited.size() == 3);

    for (const char* name : {"alpha", "beta", "gamma"}) {
        int count = 0;

        for (const auto& entry : visited) {
            if (entry == name) {
                ++count;
            }
        }

        CHECK(count == 1);
    }
}

TEST(calc_left_to_right) {
    double value = 0;
    std::string error;

    REQUIRE(eval("10 - 4 - 3", &value, &error));
    CHECK(value == 3);

    REQUIRE(eval("100 / 10 / 5", &value, &error));
    CHECK(value == 2);
}

TEST(calc_parentheses) {
    double value = 0;
    std::string error;

    REQUIRE(eval("(2 + 3) * 4", &value, &error));
    CHECK(value == 20);

    REQUIRE(eval("2 * (3 + 4) - 5", &value, &error));
    CHECK(value == 9);
}

TEST(calc_negatives_and_decimals) {
    double value = 0;
    std::string error;

    REQUIRE(eval("-3 * -3", &value, &error));
    CHECK(value == 9);

    REQUIRE(eval("7 / 2", &value, &error));
    CHECK(value == 3.5);
}

TEST(calc_computed_zero_divisor) {
    double value = 0;
    std::string error;

    CHECK(!eval("1 / (2 - 2)", &value, &error));
    CHECK(error == "division by zero");
}

TEST(calc_custom_expressions) {
    double value = 0;
    std::string error;

    REQUIRE(eval("2 * -3 + 1", &value, &error));
    CHECK(value == -5);

    REQUIRE(eval("(8 - 2) / (1 + 2)", &value, &error));
    CHECK(value == 2);
}

TEST(calc_long_expression) {
    std::string expression = "1";

    for (int i = 1; i < 20000; ++i) {
        expression += "+1";
    }

    double value = 0;
    std::string error;

    REQUIRE(eval(expression, &value, &error));
    CHECK(value == 20000);
}

TEST(calc_deep_parentheses) {
    std::string expression(5000, '(');
    expression += "7";
    expression += std::string(5000, ')');

    double value = 0;
    std::string error;

    REQUIRE(eval(expression, &value, &error));
    CHECK(value == 7);
}

// ------------------------------------------------------------- your tests
//
// Still to write (spec §6 has the full list). Suggested names:
//   reg_null_rejected, reg_for_each_visits_each_once
//   reg_10000_tools               REQUIRED: print bucket_count / load_factor /
//                                 max_probe_length, check the bound
//   reg_alternative_hash          REQUIRED: longest chain of a weak hash
//   ws_<case>                     REQUIRED: every row of the table in spec §4.4
//   calc_<case>                   left-to-right, parentheses, negatives

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
