// Studio Voice Pro — point d'entrée des tests. Un argument optionnel filtre
// une suite (io, convert, dsp, metering, transport, audio).
#include <cstdio>
#include <string>

#include "svp_test.hpp"

int main(int argc, char** argv)
{
    const std::string suite = (argc > 1) ? argv[1] : std::string{};
    return svptest::run(suite);
}
