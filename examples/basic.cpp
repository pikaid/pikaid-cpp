// SPDX-License-Identifier: MIT
//
// Minimal usage example. It is compiled and run by the test suite.

#include <pikaid/pikaid.hpp>

#include <iostream>
#include <string>

int main()
{
    const std::string id = pikaid::generate();
    std::cout << "id:         " << id << '\n';
    std::cout << "timestamp:  " << pikaid::timestamp(id) << '\n';
    std::cout << "randomness: " << pikaid::randomness_hex(id) << '\n';
    std::cout << "valid:      " << (pikaid::is_valid(id) ? "yes" : "no") << '\n';

    const pikaid::binary bytes = pikaid::to_binary(id);
    std::cout << "round trip: " << (pikaid::from_binary(bytes) == id ? "ok" : "failed")
              << '\n';

    return pikaid::is_valid(id) && pikaid::from_binary(bytes) == id ? 0 : 1;
}
