#include <iostream>
#include <string>
#include <cstdlib>
#include <array>
#include <memory>
#include <sstream>
#include <map>

std::string exec_cmd(const char* cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << " Track H1: PostgreSQL TPC-H (Scale Factor 0.1) Setup & Verification     \n";
    std::cout << "========================================================================\n";

    // 1. Verify PostgreSQL daemon and version
    std::cout << "[Step 1] Querying PostgreSQL Daemon Version...\n";
    std::string version_out = exec_cmd("psql -d tpch_sf01 -t -A -c 'SELECT version();' 2>/dev/null");
    if (version_out.empty()) {
        version_out = exec_cmd("psql -U postgres -d postgres -t -A -c 'SELECT version();' 2>/dev/null");
    }
    if (version_out.empty()) {
        std::cout << "  - Status: FAILED (Cannot connect to PostgreSQL via psql)\n";
        return 1;
    }
    std::cout << "  - Status: CONNECTED\n";
    std::cout << "  - Server: " << version_out.substr(0, version_out.find('\n')) << "\n\n";

    // 2. Query Row Counts from Database tpch_sf01
    std::cout << "[Step 2] Querying Table Row Counts in 'tpch_sf01'...\n";
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
        std::string fallback_query =
            "psql -U postgres -d tpch_sf01 -t -A -F'|' -c \""
            "SELECT 'region', count(*) FROM region UNION ALL "
            "SELECT 'nation', count(*) FROM nation UNION ALL "
            "SELECT 'supplier', count(*) FROM supplier UNION ALL "
            "SELECT 'customer', count(*) FROM customer UNION ALL "
            "SELECT 'part', count(*) FROM part UNION ALL "
            "SELECT 'partsupp', count(*) FROM partsupp UNION ALL "
            "SELECT 'orders', count(*) FROM orders UNION ALL "
            "SELECT 'lineitem', count(*) FROM lineitem;\" 2>/dev/null";
        counts_out = exec_cmd(fallback_query.c_str());
    }
    if (counts_out.empty()) {
        std::cout << "  - Status: FAILED (Database 'tpch_sf01' not found or empty)\n";
        return 1;
    }

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

    std::cout << "------------------------------------------------------------------------\n";
    std::cout << " Table Name      Expected (SF 0.1)    Actual Count    Status\n";
    std::cout << "------------------------------------------------------------------------\n";

    bool all_ok = true;
    for (const auto& [tbl, exp_cnt] : expected) {
        long long act_cnt = actual[tbl];
        bool match = (act_cnt == exp_cnt);
        if (!match) all_ok = false;

        std::cout << " " << tbl;
        for (size_t i = tbl.length(); i < 16; i++) std::cout << " ";
        std::cout << exp_cnt;
        std::string exp_str = std::to_string(exp_cnt);
        for (size_t i = exp_str.length(); i < 21; i++) std::cout << " ";
        std::cout << act_cnt;
        std::string act_str = std::to_string(act_cnt);
        for (size_t i = act_str.length(); i < 16; i++) std::cout << " ";
        std::cout << (match ? "[PASS]" : "[FAIL]") << "\n";
    }

    std::cout << "------------------------------------------------------------------------\n";
    if (all_ok) {
        std::cout << ">>> All 8 TPC-H tables loaded and verified with 100% precision! <<<\n";
    } else {
        std::cout << ">>> Verification failed for one or more tables! <<<\n";
        return 1;
    }
    std::cout << "========================================================================\n";

    return 0;
}
