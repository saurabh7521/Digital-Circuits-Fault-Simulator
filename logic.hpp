#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <optional>

enum class GateType { AND_, OR_, NAND_, NOR_, INV, BUF, XOR_ };
GateType parseGateType(const std::string& s);

struct Gate {
    GateType type;
    std::vector<int> inputs; // net IDs
    int output;              // net ID
};

struct GateNode {
    Gate g;
    GateNode* next{nullptr};
    explicit GateNode(const Gate& gg) : g(gg) {}
};

struct Circuit {
    std::unordered_map<int, std::optional<int>> nets; // unknown = nullopt
    std::vector<int> input_nets, output_nets;
    GateNode* head{nullptr};
    GateNode* tail{nullptr};

    ~Circuit();
    void ensure_net(int nid);
    void push_gate(const Gate& g);
    void reset_values();
};

// API
Circuit parse_netlist(const std::string& path);
std::unordered_map<int,int> simulate_once(Circuit& c,
    const std::unordered_map<int,int>& input_assign);
