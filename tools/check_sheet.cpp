// Build against the same pinned ArduinoJson headers as the dial.
#include "../firmware/Companion/sheet.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char **argv) {
    const std::string payload = R"({"success":true,"data":{"id":42,"name":"Birch","classes":[{"level":3,"definition":{"name":"Druid"}},{"level":2,"definition":{"name":"Fighter"}}]}})";
    std::stringstream source(payload);
    Sheet s{};
    assert(!parseSheet(source, 42, s));
    assert(s.level == 5 && std::string(s.classes) == "Druid 3 / Fighter 2");
    for (auto invalid : {std::string("bad"), std::string("{}"), payload.substr(0, 90), payload}) {
        std::stringstream bad(invalid);
        assert(parseSheet(bad, 24, s));
        assert(s.id == 42 && s.level == 5);
    }
    std::stringstream badLevel(R"({"success":true,"data":{"id":42,"name":"Birch","classes":[{"level":0,"definition":{"name":"Druid"}}]}})");
    assert(parseSheet(badLevel, 42, s));
    std::cout << "Switch-safe parse: multiclass, wrong ID, malformed/truncated and invalid level passed.\n";
    if (argc == 3) {
        std::ifstream file(argv[1]);
        assert(file && !parseSheet(file, static_cast<uint32_t>(std::stoul(argv[2])), s));
        std::cout << "Live sheet: " << s.name << " | " << s.classes << " | level " << int(s.level) << "\n";
    }
}
