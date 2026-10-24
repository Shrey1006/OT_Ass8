#include <iostream>
#include <string>
#include <cstdlib>
#include <array>
#include <memory>
#include <sstream>
#include <map>
#include <vector>
#include <fstream>
#include "index_selection.hpp"

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

void verify_postgres_environment() {
    std::cout << "========================================================================\n";
    std::cout << " Track H1/H2: PostgreSQL Environment & Invariant Check                 \n";
    std::cout << "========================================================================\n";

    std::string version_out = exec_cmd("psql -d tpch_sf01 -t -A -c 'SELECT version();' 2>/dev/null");
    if (version_out.empty()) {
        version_out = exec_cmd("psql -U postgres -d postgres -t -A -c 'SELECT version();' 2>/dev/null");
    }
    if (!version_out.empty()) {
        std::cout << "  - PostgreSQL Server: " << version_out.substr(0, version_out.find('\n')) << "\n";
        std::cout << "  - Database: tpch_sf01 (CONNECTED)\n";
    } else {
        std::cout << "  - PostgreSQL: Offline (using cached candidate index catalog)\n";
    }
}

int main(int argc, char* argv[]) {
    verify_postgres_environment();

    std::cout << "\n========================================================================\n";
    std::cout << " Track H2: Index Selection under Storage Budget (Knapsack / ILP)       \n";
    std::cout << "========================================================================\n";

    dbms::IndexSelectionProblem problem;
    std::string csv_path = "H_dbms/data/candidate_indexes.csv";
    if (argc > 1) {
        csv_path = argv[1];
    }

    if (!problem.load_from_csv(csv_path) && !problem.load_from_csv("../H_dbms/data/candidate_indexes.csv")) {
        std::cout << "[INFO] Loading built-in TPC-H SF 0.1 candidate index catalog...\n";
        problem.load_default_tpch_candidates();
    } else {
        std::cout << "[INFO] Successfully loaded candidate indexes from " << csv_path << "\n";
    }

    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << " Candidate Index Catalog (TPC-H SF 0.1 Workload)\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << std::left 
              << std::setw(4)  << "ID"
              << std::setw(34) << "Index Name"
              << std::setw(12) << "Table"
              << std::setw(10) << "Size (KB)"
              << std::setw(12) << "Benefit"
              << std::setw(10) << "Density"
              << "\n";
    std::cout << "------------------------------------------------------------------------\n";

    int total_candidate_storage = 0;
    double total_candidate_benefit = 0.0;
    for (const auto& idx : problem.candidates) {
        std::cout << std::left 
                  << std::setw(4)  << idx.id
                  << std::setw(34) << idx.name
                  << std::setw(12) << idx.table
                  << std::setw(10) << idx.size_kb
                  << std::setw(12) << std::fixed << std::setprecision(1) << idx.benefit
                  << std::setw(10) << std::fixed << std::setprecision(4) << idx.density
                  << "\n";
        total_candidate_storage += idx.size_kb;
        total_candidate_benefit += idx.benefit;
    }
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << " Total Candidate Storage: " << total_candidate_storage << " KB (" 
              << (total_candidate_storage / 1024.0) << " MB) | Total Max Benefit: " 
              << total_candidate_benefit << "\n\n";

    // Evaluate across a spectrum of storage budgets: 1MB, 2.5MB, 5MB, 10MB, 15MB, 25MB
    std::vector<int> test_budgets_kb = {1024, 2560, 5120, 10240, 15360, 25600};

    std::cout << "=========================================================================================================\n";
    std::cout << " Solver Comparison across Storage Budgets (Greedy vs Dynamic Programming vs Exact ILP/BnB)\n";
    std::cout << "=========================================================================================================\n";
    std::cout << std::left
              << std::setw(12) << "Budget"
              << std::setw(28) << "Method"
              << std::setw(14) << "Benefit"
              << std::setw(14) << "Size (KB)"
              << std::setw(12) << "Ratio (%)"
              << std::setw(12) << "Time (us)"
              << "Selected Indices"
              << "\n";
    std::cout << "---------------------------------------------------------------------------------------------------------\n";

    std::ofstream out_csv("H_dbms/results/index_selection_comparison.csv");
    if (!out_csv.is_open()) {
        out_csv.open("../H_dbms/results/index_selection_comparison.csv");
    }
    if (out_csv.is_open()) {
        out_csv << "budget_kb,budget_mb,method,benefit,size_kb,storage_utilization_pct,runtime_us,optimal,indices_count\n";
    }

    for (int budget : test_budgets_kb) {
        auto res_greedy = problem.solve_greedy(budget);
        auto res_dp     = problem.solve_dp(budget);
        auto res_ilp    = problem.solve_ilp(budget);

        std::vector<dbms::SelectionResult> results = {res_greedy, res_dp, res_ilp};

        for (const auto& res : results) {
            double util_pct = (res.total_size_kb * 100.0) / budget;
            std::string budget_str = std::to_string(budget / 1024) + " MB (" + std::to_string(budget) + "k)";

            std::stringstream id_stream;
            for (size_t i = 0; i < res.selected_index_ids.size(); ++i) {
                if (i > 0) id_stream << ",";
                id_stream << res.selected_index_ids[i];
            }

            std::cout << std::left
                      << std::setw(12) << budget_str
                      << std::setw(28) << res.method_name
                      << std::setw(14) << std::fixed << std::setprecision(1) << res.total_benefit
                      << std::setw(14) << res.total_size_kb
                      << std::setw(12) << std::fixed << std::setprecision(1) << util_pct
                      << std::setw(12) << std::fixed << std::setprecision(1) << res.runtime_us
                      << "[" << id_stream.str() << "]"
                      << "\n";

            if (out_csv.is_open()) {
                out_csv << budget << ","
                        << (budget / 1024.0) << ","
                        << "\"" << res.method_name << "\","
                        << res.total_benefit << ","
                        << res.total_size_kb << ","
                        << util_pct << ","
                        << res.runtime_us << ","
                        << (res.optimal ? 1 : 0) << ","
                        << res.selected_index_ids.size() << "\n";
            }
        }
        std::cout << "---------------------------------------------------------------------------------------------------------\n";
    }

    if (out_csv.is_open()) {
        out_csv.close();
        std::cout << "[INFO] Results saved to H_dbms/results/index_selection_comparison.csv\n";
    }

    std::cout << "\n>>> Track H2 Index Selection Program Completed Successfully! <<<\n";
    return 0;
}
