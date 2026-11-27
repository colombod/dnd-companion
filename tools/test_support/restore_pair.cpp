// Each phase runs in a fresh process. Carry only the production store's opaque
// NVS value and FAT bank bytes for each synthetic character across the boundary.
#include "session_store.h"
#include <Preferences.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

constexpr uint32_t caster = 7, companion = 8;

void transfer(const std::string &path, std::vector<uint8_t> &bytes, bool save) {
    if (save && !bytes.empty()) {
        std::ofstream file(path, std::ios::binary);
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        file.close();
    } else if (save) {
        std::filesystem::remove(path);
    } else if (std::filesystem::exists(path)) {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Could not read fixture");
        bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        if (file.bad()) throw std::runtime_error("Incomplete fixture");
    }
}

void transferSessions(bool save) {
    for (auto id : {caster, companion}) {
        std::string name = "s" + std::to_string(id);
        std::string key = "dnd-session/" + name;
        auto &nvs = Preferences::data[key];
        transfer(name, nvs, save);
        if (nvs.empty()) Preferences::data.erase(key);
        for (bool bank : {false, true}) {
            auto location = std::make_pair(id, bank);
            auto &bytes = sessionfiles::files[location];
            transfer(name + (bank ? "-b" : "-a"), bytes, save);
            if (bytes.empty()) sessionfiles::files.erase(location);
        }
    }
}

void checkSlots(uint32_t id, const session::Build &build, int expected) {
    int actual = session::remaining(build, session::read(id, build), 0);
    if (actual != expected)
        throw std::runtime_error("Character " + std::to_string(id) + " remaining slots: expected " +
                                 std::to_string(expected) + ", read " + std::to_string(actual));
}

void cast(uint32_t id, const session::Build &build) {
    if (const char *error = session::commit(id, build, {session::Op::Cast, 0}))
        throw std::runtime_error(error);
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    const std::string step = argv[1];
    try {
        session::Build build{};
        build.limit[0] = 3;
        transferSessions(false);
        if (step == "seed") {
            cast(caster, build);
            cast(companion, build);
            cast(companion, build);
        } else {
            checkSlots(companion, build, 1);
            if (step == "play") {
                checkSlots(caster, build, 2);
                cast(caster, build);
                checkSlots(caster, build, 1);
            } else if (step == "restore") {
                checkSlots(caster, build, 1);
                session::Op op;
                if (!session::last(caster, op) || op != session::Op::Cast || !session::undo(caster))
                    throw std::runtime_error("Restored firmware could not undo the new cast");
                checkSlots(caster, build, 2);
            } else if (step == "reopen") {
                checkSlots(caster, build, 2);
                session::Op op;
                if (session::last(caster, op)) throw std::runtime_error("Undo remained after reopening");
            } else return 2;
        }
        transferSessions(true);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
