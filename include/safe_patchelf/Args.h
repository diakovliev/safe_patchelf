#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <optional>
#include <map>

struct Args {
    std::string filename;
    std::string soname;
    std::map<std::string, std::string> neededs;

    static std::optional<std::pair<std::string, std::string> > parse_needed(const char* n);

    void print(std::ostream& out = std::cout) const;

    static void show_usage(const char *program_name, std::ostream& out = std::cerr);

    static std::optional<Args> parse_args(int argc, char** argv);

    bool have_work() const;

};
