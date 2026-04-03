#include <cassert>
#include <iostream>
#include "utils.h"

void test_trim() {
    assert(trim("  hello  ") == "hello");
    assert(trim("no-spaces")  == "no-spaces");
    assert(trim("")            == "");
    std::cout << "  PASS  test_trim\n";
}

void test_split() {
    auto v = split("a b c", ' ');
    assert(v.size() == 3);
    assert(v[0] == "a" && v[1] == "b" && v[2] == "c");
    std::cout << "  PASS  test_split\n";
}

int main() {
    std::cout << "Running parser tests...\n";
    test_trim();
    test_split();
    std::cout << "All tests passed.\n";
    return 0;
}