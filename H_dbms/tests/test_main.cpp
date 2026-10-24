#include <iostream>
#include <cassert>
#include <string>
#include <array>
#include <memory>
#include <sstream>
#include <map>

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

int main() {
    std::cout << "=========================================================\n";
    std::cout << " Running Track H1: PostgreSQL & TPC-H SF 0.1 Unit Tests  \n";
    std::cout << "=========================================================\n";

    test_postgres_connection();
    test_tpch_database_exists();
    test_tpch_table_counts();

    std::cout << "\n>>> All Track H1 PostgreSQL & TPC-H Tests PASSED! <<<\n";
    return 0;
}
