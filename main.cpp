#include "logic.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif
#include <cstdio>
#ifndef _WIN32
#include <unistd.h>
#endif
static std::unordered_map<int,int>
parse_vector_line(const std::string& line, const std::vector<int>& order) {
    std::string bits_only; for (char c: line) if (c=='0'||c=='1') bits_only.push_back(c);
    std::vector<int> bits;
    if ((int)bits_only.size() == (int)order.size()) {
        for (char c: bits_only) bits.push_back(c=='1');
    } else {
        std::istringstream iss(line);
        std::string t; while (iss >> t) if (t=="0"||t=="1") bits.push_back(t=="1");
    }
    if ((int)bits.size() != (int)order.size())
        throw std::runtime_error("Vector length mismatch with INPUT count");
    std::unordered_map<int,int> m;
    for (size_t i=0;i<order.size();++i) m[order[i]] = bits[i];
    return m;
}

int main(int argc, char** argv) {
    if (argc<2 || argc>3) {
        std::cerr << "Usage: " << argv[0] << " <netlist.txt> [vectors.txt]\n";
        return 2;
    }
    try {
        Circuit c = parse_netlist(argv[1]);

        std::istream* in = &std::cin;
        std::ifstream fin;
        if (argc==3) {
            fin.open(argv[2]);
            if (!fin) throw std::runtime_error(std::string("Cannot open vectors: ")+argv[2]);
            in = &fin;
        } else if (isatty(fileno(stdin))) {
            std::cerr << "# Enter vectors (0/1) per line in INPUT order; Ctrl-D/Z to finish.\n";
        }

        // Echo header
        std::cout << "INPUT ";
        for (int nid : c.input_nets) std::cout << nid << " ";
        std::cout << -1 << "\n";
        std::cout << "OUTPUT ";
        for (int nid : c.output_nets) std::cout << nid << " ";
        std::cout << -1 << "\n";

        std::string line;
        while (std::getline(*in, line)) {
            if (line.find_first_not_of(" \t\r\n")==std::string::npos) continue;
            auto vec = parse_vector_line(line, c.input_nets);
            auto out = simulate_once(c, vec);

            std::cout << "IN ";
            for (int nid : c.input_nets) std::cout << vec[nid];
            std::cout << " -> OUT ";
            for (int nid : c.output_nets) std::cout << out[nid];
            std::cout << "\n";
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
