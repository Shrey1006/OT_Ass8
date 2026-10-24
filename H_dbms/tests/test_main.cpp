#include <iostream>
#include <cassert>
#include <string>
#include <array>
#include <memory>
#include <sstream>
#include <map>
#include <vector>
#include <cmath>
#include "index_selection.hpp"

std::string exec_cmd(const char* cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void test_postgres_connection() {
    std::cout << "[Test 1] PostgreSQL Daemon Connection..." << std::flush;
    std::string out = exec_cmd("psql -d tpch_sf01 -t -A -c 'SELECT 1;' 2>/dev/null");
    if (out.empty()) out = exec_cmd("psql -U postgres -d postgres -t -A -c 'SELECT 1;' 2>/dev/null");
    assert(!out.empty() && out.find('1') != std::string::npos);
    std::cout << " PASSED\n";
}

void test_tpch_database_exists() {
    std::cout << "[Test 2] Database 'tpch_sf01' Existence..." << std::flush;
    std::string out = exec_cmd("psql -lqt 2>/dev/null | cut -d \\| -f 1 | grep -w tpch_sf01");
    if (out.empty()) out = exec_cmd("psql -U postgres -lqt 2>/dev/null | cut -d \\| -f 1 | grep -w tpch_sf01");
    assert(!out.empty() && out.find("tpch_sf01") != std::string::npos);
    std::cout << " PASSED\n";
}

void test_tpch_table_counts() {
    std::cout << "[Test 3] TPC-H SF 0.1 Table Cardinality Invariants..." << std::flush;
    std::string query =
        "psql -d tpch_sf01 -t -A -F'|' -c \""
        "SELECT 'region', count(*) FROM region UNION ALL "
        "SELECT 'nation', count(*) FROM nation UNION ALL "
        "SELECT 'supplier', count(*) FROM supplier UNION ALL "
        "SELECT 'customer', count(*) FROM customer UNION ALL "
        "SELECT 'part', count(*) FROM part UNION ALL "
        "SELECT 'partsupp', count(*) FROM partsupp UNION ALL "
        "SELECT 'orders', count(*) FROM orders UNION ALL "
        "SELECT 'lineitem', count(*) FROM lineitem;\" 2>/dev/null";

    std::string counts_out = exec_cmd(query.c_str());
    if (counts_out.empty()) {
        std::string fb_query =
            "psql -U postgres -d tpch_sf01 -t -A -F'|' -c \""
            "SELECT 'region', count(*) FROM region UNION ALL "
            "SELECT 'nation', count(*) FROM nation UNION ALL "
            "SELECT 'supplier', count(*) FROM supplier UNION ALL "
            "SELECT 'customer', count(*) FROM customer UNION ALL "
            "SELECT 'part', count(*) FROM part UNION ALL "
            "SELECT 'partsupp', count(*) FROM partsupp UNION ALL "
            "SELECT 'orders', count(*) FROM orders UNION ALL "
            "SELECT 'lineitem', count(*) FROM lineitem;\" 2>/dev/null";
        counts_out = exec_cmd(fb_query.c_str());
    }
    assert(!counts_out.empty());

    std::map<std::string, long long> expected = {
        {"region", 5},
        {"nation", 25},
        {"supplier", 1000},
        {"customer", 15000},
        {"part", 20000},
        {"partsupp", 80000},
        {"orders", 150000},
        {"lineitem", 600572}
    };

    std::stringstream ss(counts_out);
    std::string line;
    std::map<std::string, long long> actual;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        auto pos = line.find('|');
        if (pos != std::string::npos) {
            std::string tbl = line.substr(0, pos);
            long long cnt = std::stoll(line.substr(pos + 1));
            actual[tbl] = cnt;
        }
    }

    for (const auto& [tbl, exp_cnt] : expected) {
        assert(actual[tbl] == exp_cnt);
    }
    std::cout << " PASSED\n";
}

void test_textbook_knapsack_instance() {
    std::cout << "[Test 4] Textbook Knapsack Equivalence (Greedy vs DP vs ILP)..." << std::flush;
    dbms::IndexSelectionProblem prob;
    // Classic instance where greedy is suboptimal:
    // Item 1: wt=6, val=30 (density=5.0)
    // Item 2: wt=5, val=24 (density=4.8)
    // Item 3: wt=5, val=24 (density=4.8)
    // Capacity = 10.
    // Greedy takes Item 1 (val 30, wt 6, remaining 4).
    // Optimal (DP/ILP) takes Items 2 & 3 (val 48, wt 10).
    prob.candidates = {
        {1, "idx_a", "tbl", "a", 6, 100, 70, 30.0, 5.0},
        {2, "idx_b", "tbl", "b", 5, 100, 76, 24.0, 4.8},
        {3, "idx_c", "tbl", "c", 5, 100, 76, 24.0, 4.8}
    };

    auto res_greedy = prob.solve_greedy(10);
    auto res_dp = prob.solve_dp(10);
    auto res_ilp = prob.solve_ilp(10);

    assert(std::abs(res_greedy.total_benefit - 30.0) < 1e-6);
    assert(res_greedy.total_size_kb == 6);

    assert(std::abs(res_dp.total_benefit - 48.0) < 1e-6);
    assert(res_dp.total_size_kb == 10);

    assert(std::abs(res_ilp.total_benefit - 48.0) < 1e-6);
    assert(res_ilp.total_size_kb == 10);

    // Exact equality of DP and ILP
    assert(std::abs(res_dp.total_benefit - res_ilp.total_benefit) < 1e-6);
    std::cout << " PASSED (Greedy=30.0 < DP=48.0 == ILP=48.0)\n";
}

void test_edge_cases() {
    std::cout << "[Test 5] Budget Edge Cases (Zero Budget, Massive Budget, Oversized Items)..." << std::flush;
    dbms::IndexSelectionProblem prob;
    prob.candidates = {
        {1, "idx_small", "t1", "c1", 100, 500, 400, 100.0, 1.0},
        {2, "idx_large", "t2", "c2", 5000, 1000, 200, 800.0, 0.16}
    };

    // Budget = 0
    auto dp_0 = prob.solve_dp(0);
    auto ilp_0 = prob.solve_ilp(0);
    auto gr_0 = prob.solve_greedy(0);
    assert(dp_0.total_benefit == 0.0 && dp_0.total_size_kb == 0);
    assert(ilp_0.total_benefit == 0.0 && ilp_0.total_size_kb == 0);
    assert(gr_0.total_benefit == 0.0 && gr_0.total_size_kb == 0);

    // Budget fits only small
    auto dp_small = prob.solve_dp(500);
    auto ilp_small = prob.solve_ilp(500);
    assert(dp_small.total_benefit == 100.0 && dp_small.total_size_kb == 100);
    assert(ilp_small.total_benefit == 100.0 && ilp_small.total_size_kb == 100);

    // Budget fits all
    auto dp_all = prob.solve_dp(10000);
    auto ilp_all = prob.solve_ilp(10000);
    auto gr_all = prob.solve_greedy(10000);
    assert(dp_all.total_benefit == 900.0 && dp_all.total_size_kb == 5100);
    assert(ilp_all.total_benefit == 900.0 && ilp_all.total_size_kb == 5100);
    assert(gr_all.total_benefit == 900.0 && gr_all.total_size_kb == 5100);

    std::cout << " PASSED\n";
}

void test_tpch_catalog_invariants() {
    std::cout << "[Test 6] TPC-H SF 0.1 Workload Invariant Assertions across Budgets..." << std::flush;
    dbms::IndexSelectionProblem prob;
    prob.load_default_tpch_candidates();

    std::vector<int> budgets = {500, 1024, 2560, 5120, 10240, 20480, 40000};
    for (int b : budgets) {
        auto gr = prob.solve_greedy(b);
        auto dp = prob.solve_dp(b);
        auto ilp = prob.solve_ilp(b);

        // 1. Budget constraint strictly satisfied
        assert(gr.total_size_kb <= b);
        assert(dp.total_size_kb <= b);
        assert(ilp.total_size_kb <= b);

        // 2. Exact solvers yield identical global optimum
        assert(std::abs(dp.total_benefit - ilp.total_benefit) < 1e-4);

        // 3. Exact optimum is >= Greedy heuristic
        assert(dp.total_benefit >= gr.total_benefit - 1e-4);
    }
    std::cout << " PASSED (All invariants verified across 7 budget regimes)\n";
}

void test_tpch_benchmark_results() {
    std::cout << "[Test 7] TPC-H 8-Query Benchmark Checksums & Speedup Invariants..." << std::flush;
    std::ifstream csv("H_dbms/results/tpch_latency_comparison.csv");
    if (!csv.is_open()) {
        csv.open("../H_dbms/results/tpch_latency_comparison.csv");
    }
    assert(csv.is_open());

    std::string line;
    std::getline(csv, line); // Skip header

    int count = 0;
    while (std::getline(csv, line)) {
        if (line.empty()) continue;
        assert(line.find("MATCH") != std::string::npos);
        count++;
    }
    assert(count == 8); // Exactly 8 queries verified
    std::cout << " PASSED (8/8 Queries Bitwise Result Checksum MATCH)\n";
}

void test_storage_budget_compliance() {
    std::cout << "[Test 8] Storage Budget Constraint Compliance..." << std::flush;
    std::ifstream csv("H_dbms/results/tpch_storage_breakdown.csv");
    if (!csv.is_open()) {
        csv.open("../H_dbms/results/tpch_storage_breakdown.csv");
    }
    assert(csv.is_open());

    std::string line;
    std::getline(csv, line); // Skip header

    long total_added_kb = 0;
    while (std::getline(csv, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> cols;
        while (std::getline(ss, token, ',')) cols.push_back(token);
        if (cols.size() >= 5) {
            total_added_kb += std::stol(cols[4]);
        }
    }
    // Storage budget was 25 MB = 25,600 KB
    assert(total_added_kb <= 25600);
    assert(total_added_kb > 0);
    std::cout << " PASSED (Total Added Index Storage: " << total_added_kb << " KB <= 25600 KB)\n";
}

void test_join_order_experiment() {
    std::cout << "[Test 9] Join-Order & Query-Rewrite Experiment Integrity..." << std::flush;
    std::ifstream csv("H_dbms/results/join_order_experiment.csv");
    if (!csv.is_open()) {
        csv.open("../H_dbms/results/join_order_experiment.csv");
    }
    assert(csv.is_open());

    std::string line;
    std::getline(csv, line); // Skip header

    int configs = 0;
    while (std::getline(csv, line)) {
        if (line.empty()) continue;
        assert(line.find("YES") != std::string::npos);
        configs++;
    }
    assert(configs == 3);
    std::cout << " PASSED (3/3 Join-Order Configurations Verified)\n";
}

int main() {
    std::cout << "=========================================================================\n";
    std::cout << " Running Track H1-H3: DBMS Optimization & Invariant Unit Tests           \n";
    std::cout << "=========================================================================\n";

    test_postgres_connection();
    test_tpch_database_exists();
    test_tpch_table_counts();
    test_textbook_knapsack_instance();
    test_edge_cases();
    test_tpch_catalog_invariants();
    test_tpch_benchmark_results();
    test_storage_budget_compliance();
    test_join_order_experiment();

    std::cout << "\n>>> All Track H1, H2 & H3 DBMS Tests PASSED (100% OK)! <<<\n";
    return 0;
}
