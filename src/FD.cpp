#include <safe_patchelf/FD.h>

#include <fcntl.h>

#include <algorithm>

FD::FD(int initial)
    : fd_(initial)
{
}

FD::FD(FD&& src)
    : fd_(src.fd_)
    , maps_(src.maps_)
{
    src.fd_ = BAD;
    src.maps_.clear();
}

FD::~FD() {
    close();
}

void FD::close() {
    if (bad()) return;
    std::for_each(maps_.begin(), maps_.end(), [](auto& it) { ::munmap(it.first, it.second); });
    maps_.clear();
    ::close(fd_);
    fd_ = BAD;
}

int FD::get() const { return fd_; }
bool FD::bad() const { return fd_ == BAD; }

struct ::stat FD::stat() const {
    struct ::stat st{0};
    if (bad()) return st;
    ::fstat(fd_, &st);
    return st;
}

size_t FD::size() const {
    return stat().st_size;
}

void* FD::mmap(off_t off, size_t in_len, int prot, int flags) {
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
