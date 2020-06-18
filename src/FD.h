#pragma once

#include <map>
#include <algorithm>

#include <unistd.h>

class FD {
public:
    enum { BAD = -1 };

    explicit FD(int initial = BAD): fd_(initial) {}

    FD(FD&& src): fd_(src.fd_), maps_(src.maps_)
    {
        src.fd_ = BAD;
        src.maps_.clear();
    }

    ~FD() {
        close();
    }

    void close() {
        if (!bad()) return;
        std::for_each(maps_.begin(), maps_.end(), [](auto& it) { ::munmap(it.first, it.second); });
        maps_.clear();
        ::close(fd_);
        fd_ = BAD;
    }

    int get() const { return fd_; }
    bool bad() const { return fd_ == BAD; }

    struct ::stat stat() const {
        struct ::stat st{0};
        if (bad()) return st;
        ::fstat(fd_, &st);
        return st;
    }

    size_t size() const {
        return stat().st_size;
    }

    void* mmap(off_t off = 0, size_t in_len = 0, int prot = PROT_READ, int flags = MAP_FILE | MAP_SHARED) {
        if (bad())
            return nullptr;

        size_t len = (in_len != 0) ? (in_len) : (size() - off);

        auto addr = ::mmap(nullptr, len, prot, flags, fd_, off);
        if (!addr)
            return nullptr;

        auto ret = maps_.insert(std::make_pair(addr, len));
        if (!ret.second) {
            maps_[addr] = len;
        }

        return addr;
    }

private:
    // Do not copy and move
    FD(const FD&) = delete;

    int fd_;
    std::map<void*,size_t> maps_;
};
