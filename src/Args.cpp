#include <safe_patchelf/Args.h>

#include <algorithm>

#include <getopt.h>

/*static*/ std::optional<std::pair<std::string, std::string> > Args::parse_needed(const char* n) {
    std::string s(n);
    auto it = s.find(',');
    if (it == std::string::npos)
       return std::nullopt;

    std::string old_needed(s.c_str(), it);
    std::string new_needed(s.c_str() + it + 1, s.size() - it);

    if (old_needed.empty() || new_needed.empty())
       return std::nullopt;

    return std::make_pair(old_needed, new_needed);
}

void Args::print(std::ostream& out) const {
    out << "Arguments:" << std::endl;
    if (!filename.empty())
        out << "\tinput file: " << filename << std::endl;
    if (!soname.empty())
        out << "\tnew soname: " << soname << std::endl;
    std::for_each(neededs.begin(), neededs.end(), [&](auto& n) {
        out << "\tnew needed: " << n.first << " -> " << n.second << std::endl;
    });
}

/*static*/ void Args::show_usage(const char *program_name, std::ostream& out) {
    out << "Usage: " << program_name << " <options>"                                  << std::endl;
    out << "Were options are:"                                                        << std::endl;
    out << "\t-f,--filename: File to patch."                                          << std::endl;
    out << "\t-s,--soname  : New ELF soname."                                         << std::endl;
    out << "\t-n,--needed  : New ELF needed in format: <old needed>,<new needed>."    << std::endl;
    out << "\t-h,-?        : Show this help message."                                 << std::endl;
}

/*static*/ std::optional<Args> Args::parse_args(int argc, char** argv) {
    Args args;

    static const char *opt_string = "f:s:n:h?";

    static const struct option long_opts[] = {
        { "filename",   required_argument,  NULL, 'f' },
        { "soname",     required_argument,  NULL, 's' },
        { "needed",     required_argument,  NULL, 'n' },
        { NULL,         no_argument,        NULL, 0 }
    };

    int opt = -1;
    int long_index = -1;

    do {

        opt = getopt_long(argc, argv, opt_string, long_opts, &long_index);
        if (opt == -1)
            break;

        if (opt == 'f' || (opt == 0 && long_index == 0)) {
            args.filename = optarg;
        } else if (opt == 's' || (opt == 0 && long_index == 1)) {
            args.soname = optarg;
        } else if (opt == 'n' || (opt == 0 && long_index == 2)) {
            auto n = parse_needed(optarg);
            if (!n) {
                std::cerr << "error: Wrong needed replacement option: " << optarg << std::endl;
                return std::nullopt;
            }
            args.neededs.insert(*n);
        //} else if (opt == 'h' || opt == '?') {
        //    show_usage(argv[0]);
        //    return std::nullopt;
        } else {
            show_usage(argv[0]);
            return std::nullopt;
        }

    } while (true);

    if (args.filename.empty()) {
        std::cerr << "error: No ELF to process!" << std::endl;
        show_usage(argv[0]);
        return std::nullopt;
    }

    return args;
}

bool Args::have_work() const {
    if (soname.empty() && neededs.empty())
        return false;

    return true;
}
