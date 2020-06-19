#pragma once

#include <sstream>
#include <vector>
#include <list>
#include <map>
#include <optional>
#include <algorithm>

#include <cstring>

#include <safe_patchelf/commons.h>

template<ElfClass Class, Endian ElfEndian, Endian HostEndian = GetHostEndian::endian>
class Elf {
public:
    using Traits = ElfClassTraits<Class>;
    using Results = std::list<std::pair<bool, std::string> >;

    Elf(void *content)
        : content_(reinterpret_cast<caddr_t>(content))
        , ehdr_(reinterpret_cast<typename Traits::Ehdr*>(content))
        , phdrs_()
        , shdrs_()
        , executable_(false)
    {
        fill_headers();
    }

    typename Traits::Shdr* shstrtab() {
        return shdrs_[rdi(ehdr_->e_shstrndx)];
    }

    typename Traits::Shdr* find_section(const char *sh_name) {
        auto it = std::find_if(shdrs_.begin(), shdrs_.end(), [this, sh_name](auto* shdr){
            return strcmp(sh_name, section_name(shdr)) == 0;
        });
        if (it != shdrs_.end())
            return *it;
        else
            return nullptr;
    }

    const char *section_name(typename Traits::Shdr* shdr) {
        return content_ + rdi(shstrtab()->sh_offset) + rdi(shdr->sh_name);
    }


    caddr_t section_data(const char* section_name) {
        auto shdr = find_section(section_name);
        return content_ + rdi(shdr->sh_offset);
    }


    bool set_soname(const char* new_soname) {
        bool result = false;

        // Can't set soname for executable
        if (executable_) {
            error("Can't set soname for executable!");
            return result;
        }

        auto dsects = get_dynamic_sections();
        if (!dsects)
            return result;

        auto dynamic    = dsects->first;
        auto dynstr     = dsects->second;

        char* soname = nullptr;
        for (auto dyn = dynamic; rdi(dyn->d_tag) != DT_NULL; ++dyn) {
            if (rdi(dyn->d_tag) == DT_SONAME) {
                soname = dynstr + rdi(dyn->d_un.d_val);
                break;
            }
        }

        if (!soname) {
            error("Can't find soname record in .dynamic section!");
            return result;
        }

        if (new_soname && strcmp(new_soname, soname) == 0) {
            error("New soname is equal to original.");
            return result;
        }

        if (new_soname && ::strcmp(new_soname, soname) != 0) {
            size_t old_soname_size = ::strlen(soname);
            size_t new_soname_size = ::strlen(new_soname);

            bool has_error = false;
            if (new_soname_size > old_soname_size) {
                std::ostringstream msg;
                msg << "New soname string size ("
                    << "'" << new_soname << "' size: "
                    << new_soname_size
                    << " bytes) has greater size than existing ("
                    << "'" << soname << "' size: "
                    << old_soname_size
                    << " bytes).";
                error(msg.str());
                has_error = true;
            } else if (new_soname_size < old_soname_size) {
                std::ostringstream msg;
                msg << "New soname string size ("
                    << "'" << new_soname << "' size: "
                    << new_soname_size
                    << " bytes) has smaller size than existing ("
                    << "'" << soname << "' size: "
                    << old_soname_size
                    << " bytes).";
                warning(msg.str());
            }

            if (!has_error)
                ::strncpy(soname, new_soname, old_soname_size);

            result = !has_error;
        }

        return result;
    }

    bool update_neededs(const std::map<std::string, std::string>& replacements) {
        bool result = false;

        auto dsects = get_dynamic_sections();
        if (!dsects)
            return result;

        auto dynamic    = dsects->first;
        auto dynstr     = dsects->second;

        bool updates_result  = true;
        bool has_updates     = false;

        for (auto dyn = dynamic; rdi(dyn->d_tag) != DT_NULL; ++dyn) {
            if (rdi(dyn->d_tag) == DT_NEEDED) {
                char *needed_str = dynstr + rdi(dyn->d_un.d_val);

                std::for_each(replacements.begin(), replacements.end(), [&](auto& it) {
                    if (::strcmp(it.first.c_str(), needed_str) != 0)
                        return;

                    size_t old_needed_size = ::strlen(needed_str);
                    size_t new_needed_size = ::strlen(it.second.c_str());

                    bool has_error = false;
                    if (new_needed_size > old_needed_size) {
                        std::ostringstream msg;
                        msg << "New needed string size ("
                            << "'" << it.second << "' size: "
                            << new_needed_size
                            << " bytes) has greater size than existing ("
                            << "'" << needed_str << "' size: "
                            << old_needed_size
                            << " bytes).";
                        error(msg.str());
                        has_error = true;
                    } else if (new_needed_size < old_needed_size) {
                        std::ostringstream msg;
                        msg << "New needed string size ("
                            << "'" << it.second << "' size: "
                            << new_needed_size
                            << " bytes) has smaller size than existing ("
                            << "'" << needed_str << "' size: "
                            << old_needed_size
                            << " bytes).";
                        warning(msg.str());
                    }

                    if (!has_error) {
                        ::strncpy(needed_str, it.second.c_str(), old_needed_size);
                        has_updates = true;
                    }

                    updates_result &= !has_error;
                });
            }
        }

        if (!has_updates) {
            error("Where no updates in needed!");
        }

        result = updates_result & has_updates;

        return result;
    }

    const Results& results() const {
        return results_;
    }

protected:

    std::optional<std::pair<typename Traits::Dyn*, caddr_t> > get_dynamic_sections() {
        auto dynamic    = reinterpret_cast<typename Traits::Dyn*>(section_data(".dynamic"));
        if (!dynamic) {
            error("Can't find .dynamic section!");
            return std::nullopt;
        }

        auto dynstr     = section_data(".dynstr");
        if (!dynstr) {
            error("Can't find .dynstr section!");
            return std::nullopt;
        }

        return std::make_pair(dynamic, dynstr);
    }

    void warning(std::string message) const {
        results_.push_back(std::make_pair(false, std::string("warning: ") + std::move(message)));
    }

    void error(std::string message) const {
        results_.push_back(std::make_pair(true, std::string("error: ") + std::move(message)));
    }

    template<typename T>
    T rdi(T elf_val) {
        if (HostEndian == ElfEndian)
            return elf_val;

        return Bswap::bswap<T>(elf_val);
    }

    template<typename T>
    T wdi(T host_val) {
        return rdi<T>(host_val);
    }

    void fill_headers() {

        phdrs_.reserve(rdi(ehdr_->e_phnum));
        for (int i = 0; i < rdi(ehdr_->e_phnum); ++i) {
            phdrs_.push_back(&((typename Traits::Phdr*)(content_ + rdi(ehdr_->e_phoff)))[i]);
            if (rdi(phdrs_[i]->p_type) == PT_INTERP) executable_ = true;
        }

        shdrs_.reserve(rdi(ehdr_->e_shnum));
        for (int i = 0; i < rdi(ehdr_->e_shnum); ++i)
            shdrs_.push_back(&((typename Traits::Shdr*)(content_ + rdi(ehdr_->e_shoff)))[i]);
    }

private:
    caddr_t content_;
    typename Traits::Ehdr* ehdr_;
    std::vector<typename Traits::Phdr*> phdrs_;
    std::vector<typename Traits::Shdr*> shdrs_;
    bool executable_;

    mutable Results results_;
};
