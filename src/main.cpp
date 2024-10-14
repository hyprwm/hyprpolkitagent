#include "core/Agent.hpp"

int main(int argc, char* argv[]) {
    g_pAgent = std::make_unique<CAgent>();

    return g_pAgent->start() == false ? 1 : 0;
}
