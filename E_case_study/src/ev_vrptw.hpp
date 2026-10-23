#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cassert>
#include <random>
#include <limits>
#include <stdexcept>

namespace vrptw {

/**
 * Node classifications in EV-VRPTW:
 * - DEPOT: Central dispatch base (index 0).
 * - CUSTOMER: Delivery request with demand, time window [ready, due], and service duration.
 * - RECHARGE_STATION: Energy replenishment waypoint with charging capabilities.
 */
enum class NodeType {
    DEPOT = 0,
    CUSTOMER = 1,
    RECHARGE_STATION = 2
};

inline std::string node_type_to_string(NodeType type) {
    switch (type) {
        case NodeType::DEPOT: return "DEPOT";
        case NodeType::CUSTOMER: return "CUSTOMER";
        case NodeType::RECHARGE_STATION: return "RECHARGE_STATION";
    }
    return "UNKNOWN";
}

struct Node {
    int id = 0;
    NodeType type = NodeType::CUSTOMER;
    double x = 0.0;
    double y = 0.0;
    double demand = 0.0;        // kg / units of payload
    double ready_time = 0.0;    // start of delivery window e_i
    double due_time = 0.0;      // end of delivery window l_i
    double service_time = 0.0;  // unloading / dwell duration s_i
};

/**
 * Physical vehicle and fleet operational parameters.
 */
struct ProblemParams {
    double vehicle_capacity = 200.0;      // Maximum payload capacity Q_max (kg)
    double battery_capacity = 100.0;      // Maximum battery energy B_max (kWh)
    double energy_consumption_base = 0.5; // Base kWh per km
    double energy_consumption_load = 0.002; // Extra kWh per km per kg carried
    double recharge_rate = 25.0;          // kWh per hour of fast charging
    double speed = 40.0;                  // Vehicle average cruising speed (km/h)
    
    // Objective cost weights
    double cost_per_km = 1.20;            // Travel distance operating cost ($/km)
    double fixed_cost_per_vehicle = 80.0; // Fixed vehicle deployment cost ($)
    double tardiness_penalty_rate = 2.50; // Late delivery penalty ($/hour)
    
    // Bounds
    int max_vehicles = 20;
    double max_horizon = 24.0;            // Total operational schedule horizon (hours)
};

/**
 * EV-VRPTW Problem Instance definition.
 */
class VRPTWInstance {
public:
    std::string name;
    std::vector<Node> nodes;
    ProblemParams params;
    std::vector<std::vector<double>> dist_matrix;

    VRPTWInstance() = default;

    void compute_distance_matrix() {
        const size_t n = nodes.size();
        dist_matrix.assign(n, std::vector<double>(n, 0.0));
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                double dx = nodes[i].x - nodes[j].x;
                double dy = nodes[i].y - nodes[j].y;
                dist_matrix[i][j] = std::sqrt(dx * dx + dy * dy);
            }
        }
    }

    double distance(int i, int j) const {
        return dist_matrix[i][j];
    }

    double travel_time(int i, int j) const {
        return dist_matrix[i][j] / params.speed;
    }

    double energy_cost(int i, int j, double load) const {
        double d = dist_matrix[i][j];
        return (params.energy_consumption_base + params.energy_consumption_load * load) * d;
    }

    const Node& depot() const {
        return nodes[0];
    }

    std::vector<int> customer_ids() const {
        std::vector<int> cust;
        for (const auto& nd : nodes) {
            if (nd.type == NodeType::CUSTOMER) {
                cust.push_back(nd.id);
            }
        }
        return cust;
    }

    std::vector<int> station_ids() const {
        std::vector<int> st;
        for (const auto& nd : nodes) {
            if (nd.type == NodeType::RECHARGE_STATION) {
                st.push_back(nd.id);
            }
        }
        return st;
    }

    void validate() const {
        if (nodes.empty()) {
            throw std::runtime_error("Instance validation failed: Node set is empty.");
        }
        if (nodes[0].type != NodeType::DEPOT) {
            throw std::runtime_error("Instance validation failed: Node 0 must be the DEPOT.");
        }
        for (size_t i = 1; i < nodes.size(); ++i) {
            if (nodes[i].demand > params.vehicle_capacity) {
                throw std::runtime_error("Instance validation failed: Node " + std::to_string(i) +
                                         " demand exceeds vehicle capacity.");
            }
            if (nodes[i].ready_time > nodes[i].due_time) {
                throw std::runtime_error("Instance validation failed: Inverted time window for node " +
                                         std::to_string(i));
            }
        }
    }
};

/**
 * Route stop record tracking arrival, wait, departure, payload, and battery SoC.
 */
struct RouteStop {
    int node_id = 0;
    double arrival_time = 0.0;
    double wait_time = 0.0;
    double departure_time = 0.0;
    double battery_on_arrival = 0.0;
    double battery_on_departure = 0.0;
    double load_on_departure = 0.0;
    double tardiness = 0.0;
};

/**
 * Sequence of stops executed by an individual vehicle.
 */
struct VehicleRoute {
    int vehicle_id = 0;
    std::vector<RouteStop> stops;
    double total_distance = 0.0;
    double total_time = 0.0;
    double energy_consumed = 0.0;
    double total_tardiness = 0.0;
    int recharges_count = 0;
};

/**
 * Global fleet routing solution.
 */
struct Solution {
    std::vector<VehicleRoute> routes;
    double total_distance = 0.0;
    int active_vehicles = 0;
    double total_tardiness = 0.0;
    double total_energy = 0.0;
    int total_recharges = 0;
    double makespan = 0.0;
    double objective_cost = 0.0;
    bool is_feasible = false;
    std::string infeasibility_reason = "Unvalidated";
};

/**
 * Deterministic Solomon-style Benchmark Instance Generator.
 * Distribution types:
 * - 'R': Uniformly random customer dispersion.
 * - 'C': Clustered customer distributions around commercial centers.
 * - 'RC': Hybrid clustered and random distribution.
 */
inline VRPTWInstance generate_solomon_instance(
    int num_customers,
    int num_stations,
    const std::string& distribution_type = "RC",
    uint64_t seed = 42,
    ProblemParams params = ProblemParams()
) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> coord_dist(10.0, 90.0);
    std::uniform_real_distribution<double> demand_dist(10.0, 35.0);
    std::uniform_real_distribution<double> service_dist(0.2, 0.5); // 12 - 30 minutes
    std::uniform_real_distribution<double> window_width_dist(1.5, 4.0);

    VRPTWInstance inst;
    inst.name = "EV-VRPTW-" + distribution_type + "-" + std::to_string(num_customers) + "-seed" + std::to_string(seed);
    inst.params = params;

    // Node 0: Central Depot
    Node depot;
    depot.id = 0;
    depot.type = NodeType::DEPOT;
    depot.x = 50.0;
    depot.y = 50.0;
    depot.demand = 0.0;
    depot.ready_time = 0.0;
    depot.due_time = params.max_horizon;
    depot.service_time = 0.0;
    inst.nodes.push_back(depot);

    int current_id = 1;

    // Customer generation
    if (distribution_type == "C" || distribution_type == "RC") {
        // Clustered seed points
        int num_clusters = std::max(2, num_customers / 8);
        std::vector<std::pair<double, double>> cluster_centers;
        for (int c = 0; c < num_clusters; ++c) {
            cluster_centers.push_back({coord_dist(rng), coord_dist(rng)});
        }
        std::normal_distribution<double> cluster_spread(0.0, 7.5);

        for (int i = 0; i < num_customers; ++i) {
            Node cust;
            cust.id = current_id++;
            cust.type = NodeType::CUSTOMER;
            
            bool pick_cluster = (distribution_type == "C") || (rng() % 2 == 0);
            if (pick_cluster) {
                int c_idx = rng() % num_clusters;
                cust.x = std::clamp(cluster_centers[c_idx].first + cluster_spread(rng), 5.0, 95.0);
                cust.y = std::clamp(cluster_centers[c_idx].second + cluster_spread(rng), 5.0, 95.0);
            } else {
                cust.x = coord_dist(rng);
                cust.y = coord_dist(rng);
            }

            cust.demand = std::round(demand_dist(rng) * 10.0) / 10.0;
            cust.service_time = std::round(service_dist(rng) * 100.0) / 100.0;

            // Generate time window based on distance from depot
            double dist_depot = std::sqrt(std::pow(cust.x - depot.x, 2) + std::pow(cust.y - depot.y, 2));
            double min_arrival = dist_depot / params.speed;
            
            std::uniform_real_distribution<double> start_time_dist(min_arrival, std::max(min_arrival + 0.1, params.max_horizon * 0.7));
            cust.ready_time = std::round(start_time_dist(rng) * 100.0) / 100.0;
            double width = window_width_dist(rng);
            cust.due_time = std::min(params.max_horizon, cust.ready_time + width);

            inst.nodes.push_back(cust);
        }
    } else { // "R" Uniform random
        for (int i = 0; i < num_customers; ++i) {
            Node cust;
            cust.id = current_id++;
            cust.type = NodeType::CUSTOMER;
            cust.x = coord_dist(rng);
            cust.y = coord_dist(rng);
            cust.demand = std::round(demand_dist(rng) * 10.0) / 10.0;
            cust.service_time = std::round(service_dist(rng) * 100.0) / 100.0;

            double dist_depot = std::sqrt(std::pow(cust.x - depot.x, 2) + std::pow(cust.y - depot.y, 2));
            double min_arrival = dist_depot / params.speed;
            std::uniform_real_distribution<double> start_time_dist(min_arrival, std::max(min_arrival + 0.1, params.max_horizon * 0.7));
            cust.ready_time = std::round(start_time_dist(rng) * 100.0) / 100.0;
            double width = window_width_dist(rng);
            cust.due_time = std::min(params.max_horizon, cust.ready_time + width);

            inst.nodes.push_back(cust);
        }
    }

    // Strategic Recharge Stations: distributed around quadrant midpoints
    std::vector<std::pair<double, double>> fixed_quadrants = {
        {25.0, 25.0}, {75.0, 25.0}, {25.0, 75.0}, {75.0, 75.0},
        {50.0, 20.0}, {20.0, 50.0}, {80.0, 50.0}, {50.0, 80.0}
    };
    for (int s = 0; s < num_stations; ++s) {
        Node st;
        st.id = current_id++;
        st.type = NodeType::RECHARGE_STATION;
        if (s < static_cast<int>(fixed_quadrants.size())) {
            st.x = fixed_quadrants[s].first;
            st.y = fixed_quadrants[s].second;
        } else {
            st.x = coord_dist(rng);
            st.y = coord_dist(rng);
        }
        st.demand = 0.0;
        st.ready_time = 0.0;
        st.due_time = params.max_horizon;
        st.service_time = 0.0; // Variable charging duration calculated dynamically
        inst.nodes.push_back(st);
    }

    inst.compute_distance_matrix();
    inst.validate();
    return inst;
}

/**
 * Rigorous Solution Validator: verifies fleet capacity, time windows, battery feasibility,
 * and flow conservation with high-precision physics checks.
 */
inline bool validate_solution(
    const VRPTWInstance& instance,
    Solution& sol,
    bool strict_time_windows = false
) {
    sol.total_distance = 0.0;
    sol.active_vehicles = 0;
    sol.total_tardiness = 0.0;
    sol.total_energy = 0.0;
    sol.total_recharges = 0;
    sol.makespan = 0.0;
    sol.is_feasible = true;
    sol.infeasibility_reason = "Valid";

    const auto& nodes = instance.nodes;
    const auto& params = instance.params;
    std::vector<bool> visited(nodes.size(), false);

    for (size_t r = 0; r < sol.routes.size(); ++r) {
        auto& route = sol.routes[r];
        if (route.stops.empty()) continue;

        sol.active_vehicles++;
        if (route.stops.front().node_id != 0 || route.stops.back().node_id != 0) {
            sol.is_feasible = false;
            sol.infeasibility_reason = "Route " + std::to_string(r) + " must begin and end at Depot 0.";
            return false;
        }

        // Calculate initial payload load by summing all customer demands on this route
        double initial_load = 0.0;
        for (const auto& stop : route.stops) {
            if (nodes[stop.node_id].type == NodeType::CUSTOMER) {
                initial_load += nodes[stop.node_id].demand;
            }
        }

        if (initial_load > params.vehicle_capacity + 1e-6) {
            sol.is_feasible = false;
            sol.infeasibility_reason = "Route " + std::to_string(r) + " exceeds payload capacity (" +
                                       std::to_string(initial_load) + " > " + std::to_string(params.vehicle_capacity) + ").";
            return false;
        }

        double current_load = initial_load;
        double current_battery = params.battery_capacity;
        double current_time = 0.0;

        route.stops[0].arrival_time = 0.0;
        route.stops[0].wait_time = 0.0;
        route.stops[0].departure_time = 0.0;
        route.stops[0].battery_on_arrival = params.battery_capacity;
        route.stops[0].battery_on_departure = params.battery_capacity;
        route.stops[0].load_on_departure = current_load;

        route.total_distance = 0.0;
        route.energy_consumed = 0.0;
        route.total_tardiness = 0.0;
        route.recharges_count = 0;

        for (size_t s = 1; s < route.stops.size(); ++s) {
            int prev_node = route.stops[s - 1].node_id;
            int curr_node = route.stops[s].node_id;

            double dist = instance.distance(prev_node, curr_node);
            double leg_time = instance.travel_time(prev_node, curr_node);
            double energy_used = instance.energy_cost(prev_node, curr_node, current_load);

            route.total_distance += dist;
            route.energy_consumed += energy_used;

            // Battery check
            current_battery -= energy_used;
            if (current_battery < -1e-5) {
                sol.is_feasible = false;
                sol.infeasibility_reason = "Route " + std::to_string(r) + " ran out of battery at node " +
                                           std::to_string(curr_node) + " (deficit: " + std::to_string(-current_battery) + " kWh).";
                return false;
            }
            route.stops[s].battery_on_arrival = current_battery;

            // Arrival and wait times
            current_time += leg_time;
            route.stops[s].arrival_time = current_time;

            const auto& node_info = nodes[curr_node];
            double wait = 0.0;
            if (current_time < node_info.ready_time) {
                wait = node_info.ready_time - current_time;
                current_time = node_info.ready_time;
            }
            route.stops[s].wait_time = wait;

            // Tardiness calculation
            double tardiness = std::max(0.0, current_time - node_info.due_time);
            route.stops[s].tardiness = tardiness;
            route.total_tardiness += tardiness;

            if (strict_time_windows && tardiness > 1e-4) {
                sol.is_feasible = false;
                sol.infeasibility_reason = "Strict time window violated at node " + std::to_string(curr_node) +
                                           " (late by " + std::to_string(tardiness) + " hrs).";
                return false;
            }

            // Node operations
            if (node_info.type == NodeType::CUSTOMER) {
                if (visited[curr_node]) {
                    sol.is_feasible = false;
                    sol.infeasibility_reason = "Customer node " + std::to_string(curr_node) + " visited multiple times.";
                    return false;
                }
                visited[curr_node] = true;
                current_load -= node_info.demand;
                current_time += node_info.service_time;
            } else if (node_info.type == NodeType::RECHARGE_STATION) {
                // Recharging restores battery to full
                route.recharges_count++;
                double recharge_amount = params.battery_capacity - current_battery;
                double charge_duration = recharge_amount / params.recharge_rate;
                current_battery = params.battery_capacity;
                current_time += charge_duration;
            }

            route.stops[s].departure_time = current_time;
            route.stops[s].battery_on_departure = current_battery;
            route.stops[s].load_on_departure = current_load;
        }

        route.total_time = current_time;
        sol.total_distance += route.total_distance;
        sol.total_energy += route.energy_consumed;
        sol.total_tardiness += route.total_tardiness;
        sol.total_recharges += route.recharges_count;
        sol.makespan = std::max(sol.makespan, route.total_time);
    }

    // Check all customers visited
    for (const auto& nd : nodes) {
        if (nd.type == NodeType::CUSTOMER && !visited[nd.id]) {
            sol.is_feasible = false;
            sol.infeasibility_reason = "Customer " + std::to_string(nd.id) + " was never visited.";
            return false;
        }
    }

    // Objective function value
    sol.objective_cost = (params.cost_per_km * sol.total_distance) +
                         (params.fixed_cost_per_vehicle * sol.active_vehicles) +
                         (params.tardiness_penalty_rate * sol.total_tardiness);

    return sol.is_feasible;
}

/**
 * Baseline Solver: Greedy Nearest Neighbor with Time-Window and Battery Lookahead.
 * Constructive heuristic that:
 * 1. Sequentially serves nearest unvisited customers while respecting payload capacity.
 * 2. Checks battery reachability: guarantees the vehicle can reach customer AND either
 *    return to Depot or reach a Recharging Station.
 * 3. Detours through nearest Recharging Station when battery drops below safety margin.
 * 4. Dispatches a new vehicle when capacity or scheduling window saturates.
 */
inline Solution solve_greedy_baseline(const VRPTWInstance& instance) {
    const auto& nodes = instance.nodes;
    const auto& params = instance.params;
    auto customer_ids = instance.customer_ids();
    auto station_ids = instance.station_ids();

    std::vector<bool> unserved(nodes.size(), false);
    for (int cid : customer_ids) {
        unserved[cid] = true;
    }
    int unserved_count = static_cast<int>(customer_ids.size());

    Solution sol;
    int vehicle_id = 0;

    auto find_nearest_station = [&](int from_node) -> int {
        int best_st = -1;
        double best_dist = std::numeric_limits<double>::infinity();
        for (int st_id : station_ids) {
            double d = instance.distance(from_node, st_id);
            if (d < best_dist) {
                best_dist = d;
                best_st = st_id;
            }
        }
        return best_st;
    };

    while (unserved_count > 0 && vehicle_id < params.max_vehicles) {
        VehicleRoute route;
        route.vehicle_id = ++vehicle_id;

        // Start at Depot
        RouteStop depot_stop;
        depot_stop.node_id = 0;
        depot_stop.arrival_time = 0.0;
        depot_stop.departure_time = 0.0;
        depot_stop.battery_on_departure = params.battery_capacity;
        depot_stop.load_on_departure = 0.0; // will be updated once route customers chosen
        route.stops.push_back(depot_stop);

        double curr_battery = params.battery_capacity;
        double curr_time = 0.0;
        double remaining_capacity = params.vehicle_capacity;
        int curr_node = 0;

        while (true) {
            int best_cand = -1;
            int best_mid_station = -1;
            double best_score = std::numeric_limits<double>::infinity();

            for (int cand : customer_ids) {
                if (!unserved[cand]) continue;
                if (nodes[cand].demand > remaining_capacity) continue;

                double direct_dist = instance.distance(curr_node, cand);
                double direct_time = direct_dist / params.speed;
                double direct_energy = instance.energy_cost(curr_node, cand, remaining_capacity);

                // Direct travel check
                if (curr_battery >= direct_energy) {
                    double rem_batt = curr_battery - direct_energy;
                    // Lookahead: from candidate, can we reach depot or a station?
                    double energy_to_depot = instance.energy_cost(cand, 0, remaining_capacity - nodes[cand].demand);
                    bool can_escape = (rem_batt >= energy_to_depot);
                    if (!can_escape) {
                        int st = find_nearest_station(cand);
                        if (st != -1) {
                            double energy_to_st = instance.energy_cost(cand, st, remaining_capacity - nodes[cand].demand);
                            if (rem_batt >= energy_to_st) {
                                can_escape = true;
                            }
                        }
                    }

                    if (can_escape) {
                        double arr_time = curr_time + direct_time;
                        double tardiness = std::max(0.0, arr_time - nodes[cand].due_time);
                        double score = direct_dist + 2.0 * tardiness;
                        if (score < best_score) {
                            best_score = score;
                            best_cand = cand;
                            best_mid_station = -1;
                        }
                    }
                }

                // Indirect travel through charging station
                int st = find_nearest_station(curr_node);
                if (st != -1) {
                    double energy_to_st = instance.energy_cost(curr_node, st, remaining_capacity);
                    if (curr_battery >= energy_to_st) {
                        double time_to_st = instance.distance(curr_node, st) / params.speed;
                        double charge_time = (params.battery_capacity - (curr_battery - energy_to_st)) / params.recharge_rate;
                        double depart_time_from_st = curr_time + time_to_st + charge_time;
                        double leg2_dist = instance.distance(st, cand);
                        double leg2_time = leg2_dist / params.speed;
                        double leg2_energy = instance.energy_cost(st, cand, remaining_capacity);

                        if (params.battery_capacity >= leg2_energy) {
                            double rem_batt = params.battery_capacity - leg2_energy;
                            double energy_to_depot = instance.energy_cost(cand, 0, remaining_capacity - nodes[cand].demand);
                            if (rem_batt >= energy_to_depot) {
                                double arr_time = depart_time_from_st + leg2_time;
                                double tardiness = std::max(0.0, arr_time - nodes[cand].due_time);
                                double score = (instance.distance(curr_node, st) + leg2_dist) + 2.0 * tardiness + 5.0; // penalty for detour
                                if (score < best_score) {
                                    best_score = score;
                                    best_cand = cand;
                                    best_mid_station = st;
                                }
                            }
                        }
                    }
                }
            }

            if (best_cand == -1) {
                // No more feasible customers for this vehicle
                break;
            }

            // Append intermediate recharge station if necessary
            if (best_mid_station != -1) {
                RouteStop st_stop;
                st_stop.node_id = best_mid_station;
                route.stops.push_back(st_stop);
                curr_battery = params.battery_capacity;
                curr_node = best_mid_station;
            }

            // Append candidate customer
            RouteStop cust_stop;
            cust_stop.node_id = best_cand;
            route.stops.push_back(cust_stop);

            unserved[best_cand] = false;
            unserved_count--;
            remaining_capacity -= nodes[best_cand].demand;
            curr_node = best_cand;

            double en_used = instance.energy_cost(route.stops[route.stops.size() - 2].node_id, curr_node, remaining_capacity + nodes[best_cand].demand);
            curr_battery -= en_used;
        }

        // Return to depot: check if recharge is required first
        double energy_to_depot = instance.energy_cost(curr_node, 0, 0.0);
        if (curr_battery < energy_to_depot) {
            int st = find_nearest_station(curr_node);
            if (st != -1 && curr_battery >= instance.energy_cost(curr_node, st, 0.0)) {
                RouteStop st_stop;
                st_stop.node_id = st;
                route.stops.push_back(st_stop);
                curr_battery = params.battery_capacity;
                curr_node = st;
            }
        }

        RouteStop final_depot;
        final_depot.node_id = 0;
        route.stops.push_back(final_depot);

        sol.routes.push_back(route);
    }

    validate_solution(instance, sol, false);
    return sol;
}

/**
 * JSON serialization for problem instance.
 */
inline void save_instance_to_json(const VRPTWInstance& inst, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Unable to open file for writing: " + path);
    }

    out << "{\n";
    out << "  \"name\": \"" << inst.name << "\",\n";
    out << "  \"params\": {\n";
    out << "    \"vehicle_capacity\": " << inst.params.vehicle_capacity << ",\n";
    out << "    \"battery_capacity\": " << inst.params.battery_capacity << ",\n";
    out << "    \"energy_consumption_base\": " << inst.params.energy_consumption_base << ",\n";
    out << "    \"energy_consumption_load\": " << inst.params.energy_consumption_load << ",\n";
    out << "    \"recharge_rate\": " << inst.params.recharge_rate << ",\n";
    out << "    \"speed\": " << inst.params.speed << ",\n";
    out << "    \"cost_per_km\": " << inst.params.cost_per_km << ",\n";
    out << "    \"fixed_cost_per_vehicle\": " << inst.params.fixed_cost_per_vehicle << ",\n";
    out << "    \"tardiness_penalty_rate\": " << inst.params.tardiness_penalty_rate << "\n";
    out << "  },\n";
    out << "  \"nodes\": [\n";

    for (size_t i = 0; i < inst.nodes.size(); ++i) {
        const auto& nd = inst.nodes[i];
        out << "    {\n";
        out << "      \"id\": " << nd.id << ",\n";
        out << "      \"type\": \"" << node_type_to_string(nd.type) << "\",\n";
        out << "      \"x\": " << nd.x << ",\n";
        out << "      \"y\": " << nd.y << ",\n";
        out << "      \"demand\": " << nd.demand << ",\n";
        out << "      \"ready_time\": " << nd.ready_time << ",\n";
        out << "      \"due_time\": " << nd.due_time << ",\n";
        out << "      \"service_time\": " << nd.service_time << "\n";
        out << "    }" << (i + 1 < inst.nodes.size() ? "," : "") << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

/**
 * JSON serialization for routing solution.
 */
inline void save_solution_to_json(const Solution& sol, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Unable to open file for writing: " + path);
    }

    out << "{\n";
    out << "  \"is_feasible\": " << (sol.is_feasible ? "true" : "false") << ",\n";
    out << "  \"infeasibility_reason\": \"" << sol.infeasibility_reason << "\",\n";
    out << "  \"objective_cost\": " << sol.objective_cost << ",\n";
    out << "  \"total_distance\": " << sol.total_distance << ",\n";
    out << "  \"active_vehicles\": " << sol.active_vehicles << ",\n";
    out << "  \"total_tardiness\": " << sol.total_tardiness << ",\n";
    out << "  \"total_energy\": " << sol.total_energy << ",\n";
    out << "  \"total_recharges\": " << sol.total_recharges << ",\n";
    out << "  \"makespan\": " << sol.makespan << ",\n";
    out << "  \"routes\": [\n";

    for (size_t r = 0; r < sol.routes.size(); ++r) {
        const auto& route = sol.routes[r];
        out << "    {\n";
        out << "      \"vehicle_id\": " << route.vehicle_id << ",\n";
        out << "      \"distance\": " << route.total_distance << ",\n";
        out << "      \"time\": " << route.total_time << ",\n";
        out << "      \"energy\": " << route.energy_consumed << ",\n";
        out << "      \"tardiness\": " << route.total_tardiness << ",\n";
        out << "      \"recharges\": " << route.recharges_count << ",\n";
        out << "      \"stops\": [";
        for (size_t s = 0; s < route.stops.size(); ++s) {
            out << route.stops[s].node_id << (s + 1 < route.stops.size() ? ", " : "");
        }
        out << "]\n";
        out << "    }" << (r + 1 < sol.routes.size() ? "," : "") << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

inline void print_solution_summary(const Solution& sol) {
    std::cout << "=========================================================\n";
    std::cout << "               EV-VRPTW SOLUTION SUMMARY                \n";
    std::cout << "=========================================================\n";
    std::cout << " Feasibility       : " << (sol.is_feasible ? "FEASIBLE [PASS]" : "INFEASIBLE [FAIL]") << "\n";
    if (!sol.is_feasible) {
        std::cout << " Reason            : " << sol.infeasibility_reason << "\n";
    }
    std::cout << std::fixed << std::setprecision(2);
    std::cout << " Objective Value   : $" << sol.objective_cost << "\n";
    std::cout << " Active Vehicles   : " << sol.active_vehicles << "\n";
    std::cout << " Total Distance    : " << sol.total_distance << " km\n";
    std::cout << " Total Energy      : " << sol.total_energy << " kWh\n";
    std::cout << " Total Recharges   : " << sol.total_recharges << "\n";
    std::cout << " Total Tardiness   : " << sol.total_tardiness << " vehicle-hours\n";
    std::cout << " Schedule Makespan : " << sol.makespan << " hours\n";
    std::cout << "---------------------------------------------------------\n";
    for (const auto& r : sol.routes) {
        std::cout << " Vehicle #" << r.vehicle_id << " (" << r.stops.size() << " stops, " 
                  << r.total_distance << " km, " << r.energy_consumed << " kWh, " 
                  << r.recharges_count << " charges): 0";
        for (size_t s = 1; s < r.stops.size(); ++s) {
            std::cout << " -> " << r.stops[s].node_id;
        }
        std::cout << "\n";
    }
    std::cout << "=========================================================\n";
}

} // namespace vrptw
