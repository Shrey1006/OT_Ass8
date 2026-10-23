#pragma once

#include "ev_vrptw.hpp"
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <limits>
#include <chrono>

namespace vrptw {

/**
 * Helper to strip recharge stations and extract pure customer sequences for each route.
 */
inline std::vector<std::vector<int>> extract_customer_routes(const Solution& sol) {
    std::vector<std::vector<int>> cust_routes;
    for (const auto& r : sol.routes) {
        std::vector<int> custs;
        for (const auto& st : r.stops) {
            if (st.node_id != 0 && st.node_id < 1000) { // Customer nodes
                // In our instances, customer IDs are in [1, num_customers]
                // Depot is 0, recharge stations are > num_customers
                custs.push_back(st.node_id);
            }
        }
        if (!custs.empty()) {
            cust_routes.push_back(custs);
        }
    }
    return cust_routes;
}

/**
 * Reconstructs a full VehicleRoute from a sequence of customer IDs by inserting
 * depot at start/finish and recharging stations whenever battery levels require it.
 */
inline bool reconstruct_route_with_recharge(
    const VRPTWInstance& instance,
    const std::vector<int>& customer_seq,
    int vehicle_id,
    VehicleRoute& out_route
) {
    const auto& nodes = instance.nodes;
    const auto& params = instance.params;
    auto station_ids = instance.station_ids();

    out_route = VehicleRoute();
    out_route.vehicle_id = vehicle_id;

    if (customer_seq.empty()) return true;

    // Check capacity first
    double total_load = 0.0;
    for (int cid : customer_seq) {
        total_load += nodes[cid].demand;
    }
    if (total_load > params.vehicle_capacity) {
        return false; // Infeasible load
    }

    // Start at depot
    RouteStop d_start;
    d_start.node_id = 0;
    out_route.stops.push_back(d_start);

    double curr_batt = params.battery_capacity;
    double curr_load = total_load;
    int curr_node = 0;

    auto find_best_station = [&](int from_node, int to_node, double load) -> int {
        int best_st = -1;
        double min_detour = std::numeric_limits<double>::infinity();
        for (int st_id : station_ids) {
            double e1 = instance.energy_cost(from_node, st_id, load);
            double e2 = instance.energy_cost(st_id, to_node, load);
            if (curr_batt >= e1 && params.battery_capacity >= e2) {
                double detour = instance.distance(from_node, st_id) + instance.distance(st_id, to_node);
                if (detour < min_detour) {
                    min_detour = detour;
                    best_st = st_id;
                }
            }
        }
        return best_st;
    };

    for (int target : customer_seq) {
        double direct_e = instance.energy_cost(curr_node, target, curr_load);
        double e_to_depot = instance.energy_cost(target, 0, curr_load - nodes[target].demand);
        
        bool can_direct = (curr_batt >= direct_e);
        bool can_escape = can_direct && (curr_batt - direct_e >= e_to_depot);

        if (!can_escape && can_direct) {
            // Check if can escape to any station from target
            for (int st_id : station_ids) {
                double e_to_st = instance.energy_cost(target, st_id, curr_load - nodes[target].demand);
                if (curr_batt - direct_e >= e_to_st) {
                    can_escape = true;
                    break;
                }
            }
        }

        if (can_escape) {
            // Direct move
            RouteStop s;
            s.node_id = target;
            out_route.stops.push_back(s);
            curr_batt -= direct_e;
            curr_load -= nodes[target].demand;
            curr_node = target;
        } else {
            // Need a recharge station before reaching target
            int st = find_best_station(curr_node, target, curr_load);
            if (st == -1) {
                return false; // Battery infeasible
            }
            RouteStop st_stop;
            st_stop.node_id = st;
            out_route.stops.push_back(st_stop);
            curr_batt = params.battery_capacity; // Recharged
            curr_node = st;

            double leg2_e = instance.energy_cost(curr_node, target, curr_load);
            if (curr_batt < leg2_e) return false;

            RouteStop s;
            s.node_id = target;
            out_route.stops.push_back(s);
            curr_batt -= leg2_e;
            curr_load -= nodes[target].demand;
            curr_node = target;
        }
    }

    // Return to depot
    double e_depot = instance.energy_cost(curr_node, 0, 0.0);
    if (curr_batt < e_depot) {
        int st = -1;
        double min_d = std::numeric_limits<double>::infinity();
        for (int st_id : station_ids) {
            double e1 = instance.energy_cost(curr_node, st_id, 0.0);
            double e2 = instance.energy_cost(st_id, 0, 0.0);
            if (curr_batt >= e1 && params.battery_capacity >= e2) {
                double d = instance.distance(curr_node, st_id) + instance.distance(st_id, 0);
                if (d < min_d) {
                    min_d = d;
                    st = st_id;
                }
            }
        }
        if (st == -1) return false;
        RouteStop st_stop;
        st_stop.node_id = st;
        out_route.stops.push_back(st_stop);
        curr_node = st;
    }

    RouteStop d_end;
    d_end.node_id = 0;
    out_route.stops.push_back(d_end);

    return true;
}

/**
 * Builds and evaluates a full Solution from customer routes.
 */
inline bool build_solution_from_customer_routes(
    const VRPTWInstance& instance,
    const std::vector<std::vector<int>>& cust_routes,
    Solution& out_sol
) {
    out_sol = Solution();
    int v_id = 0;
    for (const auto& seq : cust_routes) {
        if (seq.empty()) continue;
        VehicleRoute vr;
        if (!reconstruct_route_with_recharge(instance, seq, ++v_id, vr)) {
            out_sol.is_feasible = false;
            out_sol.infeasibility_reason = "Route reconstruction failed battery/capacity constraint.";
            return false;
        }
        out_sol.routes.push_back(vr);
    }
    return validate_solution(instance, out_sol, false);
}

// ============================================================================
// METHOD 1: Simulated Annealing with Multi-operator Neighborhood Search (SA-MNS)
// ============================================================================

struct SAParams {
    double initial_temp = 150.0;
    double cooling_rate = 0.985;
    int iterations_per_temp = 25;
    int max_iterations = 2500;
};

inline Solution solve_simulated_annealing(
    const VRPTWInstance& instance,
    const Solution& initial_sol,
    const SAParams& sa_params = SAParams(),
    uint64_t seed = 42
) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> uniform_prob(0.0, 1.0);

    // Extract customer IDs only (excluding depot and stations)
    std::vector<std::vector<int>> current_cust_routes;
    for (const auto& r : initial_sol.routes) {
        std::vector<int> custs;
        for (const auto& st : r.stops) {
            if (st.node_id > 0 && instance.nodes[st.node_id].type == NodeType::CUSTOMER) {
                custs.push_back(st.node_id);
            }
        }
        if (!custs.empty()) {
            current_cust_routes.push_back(custs);
        }
    }

    Solution current_sol = initial_sol;
    Solution best_sol = initial_sol;

    double temp = sa_params.initial_temp;
    int total_iters = 0;

    while (total_iters < sa_params.max_iterations && temp > 0.1) {
        for (int step = 0; step < sa_params.iterations_per_temp; ++step) {
            total_iters++;

            // Clean up any empty routes
            std::vector<std::vector<int>> candidate_routes;
            for (const auto& r : current_cust_routes) {
                if (!r.empty()) candidate_routes.push_back(r);
            }
            if (candidate_routes.empty()) break;

            int move_type = rng() % 4;
            bool valid_move = false;

            if (move_type == 0 && !candidate_routes.empty()) {
                // Operator 1: Intra-route 2-Opt
                int r_idx = rng() % candidate_routes.size();
                auto& r = candidate_routes[r_idx];
                if (r.size() >= 3) {
                    int i = rng() % r.size();
                    int j = rng() % r.size();
                    if (i > j) std::swap(i, j);
                    if (j - i >= 1) {
                        std::reverse(r.begin() + i, r.begin() + j + 1);
                        valid_move = true;
                    }
                }
            } else if (move_type == 1 && !candidate_routes.empty()) {
                // Operator 2: Intra-route Relocate
                int r_idx = rng() % candidate_routes.size();
                auto& r = candidate_routes[r_idx];
                if (r.size() >= 3) {
                    int from = rng() % r.size();
                    int to = rng() % r.size();
                    if (from != to) {
                        int node = r[from];
                        r.erase(r.begin() + from);
                        r.insert(r.begin() + to, node);
                        valid_move = true;
                    }
                }
            } else if (move_type == 2 && candidate_routes.size() >= 2) {
                // Operator 3: Inter-route Customer Relocate
                int r1 = rng() % candidate_routes.size();
                int r2 = rng() % candidate_routes.size();
                if (r1 != r2 && !candidate_routes[r1].empty()) {
                    int idx1 = rng() % candidate_routes[r1].size();
                    int node = candidate_routes[r1][idx1];
                    candidate_routes[r1].erase(candidate_routes[r1].begin() + idx1);
                    
                    int ins_pos = rng() % (candidate_routes[r2].size() + 1);
                    candidate_routes[r2].insert(candidate_routes[r2].begin() + ins_pos, node);
                    valid_move = true;
                }
            } else if (move_type == 3 && candidate_routes.size() >= 2) {
                // Operator 4: Inter-route Customer Swap
                int r1 = rng() % candidate_routes.size();
                int r2 = rng() % candidate_routes.size();
                if (r1 != r2 && !candidate_routes[r1].empty() && !candidate_routes[r2].empty()) {
                    int idx1 = rng() % candidate_routes[r1].size();
                    int idx2 = rng() % candidate_routes[r2].size();
                    std::swap(candidate_routes[r1][idx1], candidate_routes[r2][idx2]);
                    valid_move = true;
                }
            }

            if (!valid_move) continue;

            // Evaluate candidate solution
            Solution cand_sol;
            bool feasible = build_solution_from_customer_routes(instance, candidate_routes, cand_sol);
            if (!feasible) continue;

            double delta = cand_sol.objective_cost - current_sol.objective_cost;
            if (delta < 0.0 || uniform_prob(rng) < std::exp(-delta / temp)) {
                current_cust_routes = candidate_routes;
                current_sol = cand_sol;

                if (current_sol.objective_cost < best_sol.objective_cost) {
                    best_sol = current_sol;
                }
            }
        }
        temp *= sa_params.cooling_rate;
    }

    return best_sol;
}

// ============================================================================
// METHOD 2: Large Neighborhood Search / Ruin-and-Recreate (LNS)
// ============================================================================

struct LNSParams {
    int max_iterations = 600;
    int min_destroy = 2;
    int max_destroy = 6;
    double shaw_dist_weight = 1.0;
    double shaw_time_weight = 0.5;
    double shaw_demand_weight = 0.2;
};

inline Solution solve_large_neighborhood_search(
    const VRPTWInstance& instance,
    const Solution& initial_sol,
    const LNSParams& lns_params = LNSParams(),
    uint64_t seed = 42
) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> uniform_prob(0.0, 1.0);

    // Initial customer routes
    std::vector<std::vector<int>> current_cust_routes;
    for (const auto& r : initial_sol.routes) {
        std::vector<int> custs;
        for (const auto& st : r.stops) {
            if (st.node_id > 0 && instance.nodes[st.node_id].type == NodeType::CUSTOMER) {
                custs.push_back(st.node_id);
            }
        }
        if (!custs.empty()) {
            current_cust_routes.push_back(custs);
        }
    }

    Solution current_sol = initial_sol;
    Solution best_sol = initial_sol;

    for (int iter = 0; iter < lns_params.max_iterations; ++iter) {
        // Step 1: Clean up empty routes
        std::vector<std::vector<int>> candidate_routes;
        for (const auto& r : current_cust_routes) {
            if (!r.empty()) candidate_routes.push_back(r);
        }
        if (candidate_routes.empty()) break;

        // Step 2: Ruin (Destroy Operator)
        int num_to_destroy = lns_params.min_destroy + (rng() % (lns_params.max_destroy - lns_params.min_destroy + 1));
        std::vector<int> removed_customers;

        int destroy_type = rng() % 2; // 0 = Random, 1 = Shaw Spatial-Temporal
        if (destroy_type == 0) {
            // Random removal
            for (int d = 0; d < num_to_destroy; ++d) {
                if (candidate_routes.empty()) break;
                int r_idx = rng() % candidate_routes.size();
                if (!candidate_routes[r_idx].empty()) {
                    int c_idx = rng() % candidate_routes[r_idx].size();
                    removed_customers.push_back(candidate_routes[r_idx][c_idx]);
                    candidate_routes[r_idx].erase(candidate_routes[r_idx].begin() + c_idx);
                }
            }
        } else {
            // Shaw Spatial-Temporal Relatedness Removal
            // Pick a seed customer
            int seed_cust = -1;
            int r_idx = rng() % candidate_routes.size();
            if (!candidate_routes[r_idx].empty()) {
                int c_idx = rng() % candidate_routes[r_idx].size();
                seed_cust = candidate_routes[r_idx][c_idx];
                removed_customers.push_back(seed_cust);
                candidate_routes[r_idx].erase(candidate_routes[r_idx].begin() + c_idx);
            }

            if (seed_cust != -1) {
                // Find most related customers to remove
                while (static_cast<int>(removed_customers.size()) < num_to_destroy) {
                    double min_rel = std::numeric_limits<double>::infinity();
                    int best_r = -1;
                    int best_c = -1;
                    int best_id = -1;

                    for (size_t r = 0; r < candidate_routes.size(); ++r) {
                        for (size_t c = 0; c < candidate_routes[r].size(); ++c) {
                            int cand_id = candidate_routes[r][c];
                            double d = instance.distance(seed_cust, cand_id);
                            double dt = std::abs(instance.nodes[seed_cust].ready_time - instance.nodes[cand_id].ready_time);
                            double dq = std::abs(instance.nodes[seed_cust].demand - instance.nodes[cand_id].demand);
                            double relatedness = lns_params.shaw_dist_weight * d +
                                                 lns_params.shaw_time_weight * dt +
                                                 lns_params.shaw_demand_weight * dq;
                            if (relatedness < min_rel) {
                                min_rel = relatedness;
                                best_r = static_cast<int>(r);
                                best_c = static_cast<int>(c);
                                best_id = cand_id;
                            }
                        }
                    }

                    if (best_id == -1) break;
                    removed_customers.push_back(best_id);
                    candidate_routes[best_r].erase(candidate_routes[best_r].begin() + best_c);
                }
            }
        }

        // Step 3: Recreate (Greedy Best-Insertion with battery lookahead)
        for (int cust_to_insert : removed_customers) {
            double best_cost = std::numeric_limits<double>::infinity();
            int best_route_idx = -1;
            int best_insert_pos = -1;

            // Try inserting into each existing route
            for (size_t r = 0; r < candidate_routes.size(); ++r) {
                for (size_t pos = 0; pos <= candidate_routes[r].size(); ++pos) {
                    auto test_routes = candidate_routes;
                    test_routes[r].insert(test_routes[r].begin() + pos, cust_to_insert);

                    Solution test_sol;
                    if (build_solution_from_customer_routes(instance, test_routes, test_sol)) {
                        if (test_sol.objective_cost < best_cost) {
                            best_cost = test_sol.objective_cost;
                            best_route_idx = static_cast<int>(r);
                            best_insert_pos = static_cast<int>(pos);
                        }
                    }
                }
            }

            // Also try opening a new vehicle route if capacity / fleet size allows
            if (candidate_routes.size() < static_cast<size_t>(instance.params.max_vehicles)) {
                auto test_routes = candidate_routes;
                test_routes.push_back({cust_to_insert});
                Solution test_sol;
                if (build_solution_from_customer_routes(instance, test_routes, test_sol)) {
                    if (test_sol.objective_cost < best_cost) {
                        best_cost = test_sol.objective_cost;
                        best_route_idx = static_cast<int>(candidate_routes.size());
                        best_insert_pos = 0;
                    }
                }
            }

            // Apply best insertion found
            if (best_route_idx != -1) {
                if (best_route_idx == static_cast<int>(candidate_routes.size())) {
                    candidate_routes.push_back({cust_to_insert});
                } else {
                    candidate_routes[best_route_idx].insert(
                        candidate_routes[best_route_idx].begin() + best_insert_pos,
                        cust_to_insert
                    );
                }
            } else {
                // Fallback: append to smallest route
                if (!candidate_routes.empty()) {
                    candidate_routes[0].push_back(cust_to_insert);
                } else {
                    candidate_routes.push_back({cust_to_insert});
                }
            }
        }

        // Step 4: Evaluate Candidate Solution
        Solution cand_sol;
        if (build_solution_from_customer_routes(instance, candidate_routes, cand_sol)) {
            double delta = cand_sol.objective_cost - current_sol.objective_cost;
            // Simulated Annealing / Metropolis acceptance threshold
            double temp = 50.0 * (1.0 - static_cast<double>(iter) / lns_params.max_iterations);
            if (delta < 0.0 || (temp > 0.1 && uniform_prob(rng) < std::exp(-delta / temp))) {
                current_cust_routes = candidate_routes;
                current_sol = cand_sol;

                if (current_sol.objective_cost < best_sol.objective_cost) {
                    best_sol = current_sol;
                }
            }
        }
    }

    return best_sol;
}

} // namespace vrptw
