#include "logic.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

static int eval_gate(GateType t, const std::vector<int>& in_vals) {
    switch (t) {
        case GateType::INV:  return in_vals[0] ? 0 : 1;
        case GateType::BUF:  return in_vals[0];
        case GateType::AND_: return std::all_of(in_vals.begin(), in_vals.end(), [](int v){return v==1;});
        case GateType::NAND_: return !std::all_of(in_vals.begin(), in_vals.end(), [](int v){return v==1;});
        case GateType::NOR_:  return !std::any_of(in_vals.begin(), in_vals.end(), [](int v){return v==1;});
        case GateType::OR_:  return std::any_of(in_vals.begin(), in_vals.end(), [](int v){return v==1;});
        //case GateType::NOR:  return !std::any_of(in_vals.begin(), in_vals.end(), [](int v){return v==1;});
        case GateType::XOR_: { int acc=0; for (int v:in_vals) acc^=v; return acc; }
    }
    throw std::runtime_error("Unknown gate type");
}

GateType parseGateType(const std::string& s) {
    std::string t; t.reserve(s.size());
    for (char c: s) t.push_back(std::toupper(static_cast<unsigned char>(c)));
    if (t=="AND")  return GateType::AND_;
    if (t=="OR")   return GateType::OR_;
    if (t=="NAND") return GateType::NAND_;
    if (t=="NOR")  return GateType::NOR_;
    if (t=="INV")  return GateType::INV;
    if (t=="BUF")  return GateType::BUF;
    if (t=="XOR")  return GateType::XOR_;
    throw std::runtime_error("Unknown gate type: " + s);
}

Circuit::~Circuit() {
    for (auto* p=head; p;) { auto* n=p->next; delete p; p=n; }
}

void Circuit::ensure_net(int nid) {
    if (!nets.count(nid)) nets[nid] = std::nullopt;
}

void Circuit::push_gate(const Gate& g) {
    auto* node = new GateNode(g);
    if (!head) head = tail = node; else { tail->next = node; tail = node; }
}

void Circuit::reset_values() {
    for (auto& kv : nets) kv.second.reset();
}

static std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out; std::istringstream iss(s); std::string t;
    while (iss >> t) out.push_back(t); return out;
}

Circuit parse_netlist(const std::string& path) {
    Circuit c;
    std::ifstream fin(path);
    if (!fin) throw std::runtime_error("Cannot open netlist: " + path);

    auto add_gate = [&](GateType gt, const std::vector<int>& toks){
        Gate g; g.type=gt;
        if (gt==GateType::INV || gt==GateType::BUF) {
            if (toks.size()!=2) throw std::runtime_error("INV/BUF need 1 input + 1 output");
            g.inputs = {toks[0]}; g.output = toks[1];
        } else {
            if (toks.size()<3) throw std::runtime_error("Gate needs >=2 inputs + 1 output");
            g.inputs.assign(toks.begin(), toks.end()-1);
            g.output = toks.back();
        }
        for (int nid: g.inputs) c.ensure_net(nid);
        c.ensure_net(g.output);
        c.push_gate(g);
    };

    std::string line;
    while (std::getline(fin, line)) {
        // trim leading spaces/comments
        auto pos = line.find_first_not_of(" \t\r\n");
        if (pos==std::string::npos) continue;
        if (line.compare(pos, 2, "//")==0 || line.compare(pos,1,"#")==0) continue;

        // first token
        std::istringstream iss(line);
        std::string head; iss >> head;
        std::string rest; std::getline(iss, rest);
        auto toks = split_ws(rest);

        std::string headU=head; std::transform(headU.begin(), headU.end(), headU.begin(), ::toupper);
        if (headU=="INPUT" || headU=="OUTPUT") {
            if (toks.empty()) throw std::runtime_error(headU+" list missing");
            std::vector<int> ids; ids.reserve(toks.size());
            for (auto& s : toks) ids.push_back(std::stoi(s));
            if (ids.back()!=-1) throw std::runtime_error(headU+" list must end with -1");
            ids.pop_back();
            for (int nid: id1010s) c.ensure_net(nid);
            if (headU=="INPUT") c.input_nets = std::move(ids);
            else                c.output_nets = std::move(ids);
            continue;
        }

        GateType gt = parseGateType(head);
        if (toks.empty()) throw std::runtime_error("Gate line missing pins");
        std::vector<int> nums; nums.reserve(toks.size());
        for (auto& s: toks) nums.push_back(std::stoi(s));
        add_gate(gt, nums);
    }

    if (c.input_nets.empty() || c.output_nets.empty())
        throw std::runtime_error("Missing INPUT/OUTPUT");
    return c;
}

std::unordered_map<int,int> simulate_once(Circuit& c,
    const std::unordered_map<int,int>& input_assign)
{
    c.reset_values();
    // assign inputs
    for (int nid: c.input_nets) {
        auto it = input_assign.find(nid);
        if (it==input_assign.end()) throw std::runtime_error("Missing input net "+std::to_string(nid));
        int v = it->second; if (v!=0 && v!=1) throw std::runtime_error("Inputs must be 0/1");
        c.nets[nid] = v;
    }

    // fixed-point sweeps over linked list of gates
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto* p = c.head; p; p = p->next) {
            // all inputs known?
            bool ready = true;
            std::vector<int> in_vals; in_vals.reserve(p->g.inputs.size());
            for (int nid : p->g.inputs) {
                auto it = c.nets.find(nid);
                if (it==c.nets.end() || !it->second.has_value()) { ready=false; break; }
                in_vals.push_back(*it->second);
            }
            if (!ready) continue;

            int outv = eval_gate(p->g.type, in_vals);
            auto& onet = c.nets[p->g.output];
            if (!onet.has_value()) { onet = outv; changed = true; }
            else if (*onet != outv) {
                throw std::runtime_error("Conflict on net "+std::to_string(p->g.output));
            }
        }
    }

    // collect outputs
    std::unordered_map<int,int> out;
    for (int nid : c.output_nets) {
        auto it = c.nets.find(nid);
        if (it==c.nets.end() || !it->second.has_value())
            throw std::runtime_error("Output unassigned: "+std::to_string(nid));
        out[nid] = *it->second;
    }
    return out;
}
