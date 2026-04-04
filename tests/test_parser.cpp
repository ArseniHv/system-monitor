#include <cassert>
#include <iostream>
#include <sstream>
#include "utils.h"

// ── utils tests ───────────────────────────────────────────────────

void test_trim() {
    assert(trim("  hello  ") == "hello");
    assert(trim("no-spaces")  == "no-spaces");
    assert(trim("")            == "");
    assert(trim("   ")         == "");
    std::cout << "  PASS  test_trim\n";
}

void test_split() {
    auto v = split("a b c", ' ');
    assert(v.size() == 3);
    assert(v[0] == "a" && v[1] == "b" && v[2] == "c");

    auto v2 = split("  a  b  ", ' ');
    assert(v2.size() == 2);
    std::cout << "  PASS  test_split\n";
}

void test_format_kb() {
    assert(format_kb(1024)        == "1.0 MB");
    assert(format_kb(1024 * 1024) == "1.0 GB");
    assert(format_kb(512)         == "512 KB");
    std::cout << "  PASS  test_format_kb\n";
}

// ── /proc/meminfo parser simulation ──────────────────────────────

void test_parse_meminfo_logic() {
    std::string sample =
        "MemTotal:       16384000 kB\n"
        "MemFree:         8192000 kB\n"
        "Buffers:          512000 kB\n"
        "Cached:          1024000 kB\n"
        "SwapTotal:       8192000 kB\n"
        "SwapFree:        8192000 kB\n";

    long mem_total = 0, mem_free = 0, mem_buffers = 0,
         mem_cached = 0, swap_total = 0, swap_free = 0;

    std::istringstream ss(sample);
    std::string line;
    while (std::getline(ss, line)) {
        std::istringstream ls(line);
        std::string key; long value;
        ls >> key >> value;
        if      (key == "MemTotal:")   mem_total   = value;
        else if (key == "MemFree:")    mem_free    = value;
        else if (key == "Buffers:")    mem_buffers = value;
        else if (key == "Cached:")     mem_cached  = value;
        else if (key == "SwapTotal:")  swap_total  = value;
        else if (key == "SwapFree:")   swap_free   = value;
    }

    long used = mem_total - mem_free - mem_buffers - mem_cached;
    assert(mem_total              == 16384000);
    assert(used                   == 6656000);
    assert(swap_total             == 8192000);
    assert(swap_total - swap_free == 0);
    (void)swap_free;
    (void)swap_total;
    (void)used;
    std::cout << "  PASS  test_parse_meminfo_logic\n";
}

// ── /proc/stat CPU delta simulation ──────────────────────────────

void test_cpu_delta_logic() {
    long long u1=100, n1=0, s1=50, id1=850, io1=0, irq1=0, si1=0, st1=0;
    long long u2=150, n2=0, s2=75, id2=875, io2=0, irq2=0, si2=0, st2=0;

    long long active1 = u1+n1+s1+irq1+si1+st1;
    long long total1  = active1+id1+io1;
    long long active2 = u2+n2+s2+irq2+si2+st2;
    long long total2  = active2+id2+io2;

    long long ad  = active2 - active1;
    long long td  = total2  - total1;
    double    pct = static_cast<double>(ad) / td * 100.0;

    assert(ad == 75);
    assert(td == 100);
    assert(pct > 74.9 && pct < 75.1);
    (void)pct;
    std::cout << "  PASS  test_cpu_delta_logic\n";
}

// ── /proc/net/dev parser simulation ──────────────────────────────

void test_parse_netdev_logic() {
    std::string line =
        "  eth0: 1000000  100  0  0  0  0  0  0  500000  50  0  0  0  0  0  0";

    auto colon = line.find(':');
    assert(colon != std::string::npos);

    std::istringstream ss(line.substr(colon + 1));
    long long rx, tx, dummy;
    ss >> rx >> dummy >> dummy >> dummy
       >> dummy >> dummy >> dummy >> dummy
       >> tx;

    assert(rx == 1000000);
    assert(tx == 500000);
    std::cout << "  PASS  test_parse_netdev_logic\n";
}

// ── main ──────────────────────────────────────────────────────────

int main() {
    std::cout << "Running parser tests...\n";
    test_trim();
    test_split();
    test_format_kb();
    test_parse_meminfo_logic();
    test_cpu_delta_logic();
    test_parse_netdev_logic();
    std::cout << "\nAll tests passed.\n";
    return 0;
}