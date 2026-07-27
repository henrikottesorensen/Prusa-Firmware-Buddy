// A host implementation of PartialFile, backed by an ordinary file.
//
// The shipped one (src/transfers/partial_file.cpp) writes through USB MSC with DMA and sector
// buffering, none of which exists off-device, and the unit tests' partial_file_mock.cpp returns
// "not implemented" from create() and open(). That is fine for tests that never transfer anything,
// but it means the rig refuses every download in Transfer::begin before a single byte moves - so
// the rig cannot exercise the one path it was built for without this.
//
// Everything here is the same class: the header is untouched, and the members it declares (the
// sector pool, the USB request slots) are constructed and simply never used. Only the behaviour is
// substituted, which keeps the firmware under test rather than a reimplementation of it.
//
// The valid-part bookkeeping is NOT simplified. Transfer::PlainGcodeDownloadOrder reads
// has_valid_head/has_valid_tail to decide when to RangeJump, and plain gcode over 512 KiB always
// does one - so getting this wrong would change the download order the rig is meant to verify.
// extend_valid_part below mirrors partial_file.cpp:322-348 exactly.

#include "transfers/partial_file.hpp"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace transfers;

namespace {

// Errors are returned as string literals through PartialFile::Result, so they must outlive the call.
const char *last_error(const char *what) {
    static char buffer[128];
    snprintf(buffer, sizeof buffer, "%s: %s", what, strerror(errno));
    return buffer;
}

} // namespace

// --- SectorPool: present because the header declares it as a member, never used here -------------

PartialFile::SectorPool::SectorPool(UsbhMscRequest::LunNbr lun, UsbhMscRequestCallback callback, void *callback_param1)
    : semaphore(size, size)
    , slot_mask(0)
    , pool {} {
    (void)lun;
    (void)callback;
    (void)callback_param1;
}

PartialFile::SectorPool::~SectorPool() = default;

// --- PartialFile ---------------------------------------------------------------------------------

// file_lock carries the writable descriptor here, rather than the read-only "don't delete this"
// handle it holds on device. Same member, same lifetime, and the destructor closes it either way.
PartialFile::PartialFile(UsbhMscRequest::LunNbr drive, UsbhMscRequest::SectorNbr first_sector, State state, int file_lock)
    : sector_pool(drive, nullptr, this)
    , write_error(false)
    , first_sector_nbr(first_sector)
    , current_sector(nullptr)
    , current_offset(0)
    , state(state)
    , last_progress_percent(-1)
    , file_lock(file_lock) {}

PartialFile::~PartialFile() {
    if (file_lock >= 0) {
        close(file_lock);
    }
}

PartialFile::Result PartialFile::create(const char *path, size_t size) {
    int fd = ::open(path, O_RDWR | O_CREAT | O_EXCL, 0644);

    if (fd < 0) {
        return last_error("create");
    }

    // Preallocated up front, exactly as the device version does: the file is written out of order
    // (tail first for plain gcode), so it has to be full-size from the start.
    if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
        close(fd);
        return last_error("ftruncate");
    }

    State state {};
    state.total_size = size;

    return Ptr(new PartialFile(0, 0, state, fd));
}

PartialFile::Result PartialFile::open(const char *path, State state, bool ignore_opened) {
    (void)ignore_opened;

    int fd = ::open(path, O_RDWR);

    if (fd < 0) {
        return last_error("open");
    }

    struct stat st;

    if (fstat(fd, &st) != 0) {
        close(fd);
        return last_error("fstat");
    }

    // Contract from the header: total_size is taken from what is actually on disk, not from the
    // caller's idea of it.
    state.total_size = static_cast<size_t>(st.st_size);

    return Ptr(new PartialFile(0, 0, state, fd));
}

PartialFile::Result PartialFile::convert(const char *path, unique_file_ptr file, State state) {
    file.reset();

    return open(path, state, true);
}

bool PartialFile::seek(size_t offset) {
    if (offset > state.total_size) {
        return false;
    }

    current_offset = offset;

    return true;
}

bool PartialFile::write(const uint8_t *data, size_t size) {
    if (file_lock < 0) {
        return false;
    }

    size_t written = 0;

    while (written < size) {
        ssize_t result = pwrite(file_lock, data + written, size - written,
            static_cast<off_t>(current_offset + written));

        if (result <= 0) {
            if (errno == EINTR) {
                continue;
            }

            write_error = true;

            return false;
        }

        written += static_cast<size_t>(result);
    }

    extend_valid_part(ValidPart { current_offset, current_offset + size });
    current_offset += size;

    return true;
}

bool PartialFile::sync() {
    return file_lock < 0 || fsync(file_lock) == 0;
}

void PartialFile::release_file() {
    if (file_lock >= 0) {
        close(file_lock);
        file_lock = -1;
    }
}

PartialFile::State PartialFile::get_state() const {
    return state;
}

bool PartialFile::has_valid_head(size_t bytes) const {
    auto st = get_state();
    return st.valid_head && st.valid_head->start == 0 && st.valid_head->end >= bytes;
}

bool PartialFile::has_valid_tail(size_t bytes) const {
    auto st = get_state();
    return st.valid_tail && st.valid_tail->start <= (st.total_size - bytes) && st.valid_tail->end == st.total_size;
}

// Mirrors partial_file.cpp:322-348. Do not simplify: the download order depends on it.
void PartialFile::extend_valid_part(ValidPart new_part) {
    if (state.valid_head) {
        state.valid_head->merge(new_part);
    } else if (new_part.start == 0) {
        state.valid_head = new_part;
    }

    auto head_end = state.valid_head ? state.valid_head->end : 0;

    if (state.valid_tail) {
        state.valid_tail->merge(new_part);
    } else if (new_part.start > head_end) {
        state.valid_tail = new_part;
    }

    if (state.valid_head && state.valid_head->end == state.total_size) {
        state.valid_tail = state.valid_head;
    }

    if (state.valid_head && state.valid_tail) {
        state.valid_head->merge(*state.valid_tail);
        state.valid_tail->merge(*state.valid_head);
    }
}

void PartialFile::print_progress() {}
