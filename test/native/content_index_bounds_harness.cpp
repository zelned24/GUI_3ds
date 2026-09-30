#include "content/ContentUpdateStore.hpp"
#include <cstring>

namespace {
class MemoryContentStorage final : public Pokerogue3DS::ContentStorage {
public:
    unsigned reads = 0;
    bool shortRead = false;
    unsigned char bytes[16]{};
    bool read(const char*, uint32_t offset, void* output, uint32_t capacity, uint32_t& actual) override {
        ++reads;
        if (offset > sizeof(bytes) || capacity > sizeof(bytes) - offset) return false;
        actual = shortRead && capacity ? capacity - 1 : capacity;
        std::memcpy(output, bytes + offset, actual);
        return true;
    }
    bool write(const char*, uint32_t, const void*, uint32_t, bool) override { return false; }
    bool directory(const char*) override { return false; }
    uint64_t freeBytes() override { return 0; }
};
}

// Standalone host harness; must be included in the final validation stage.
int main() {
    using namespace Pokerogue3DS;
    uint32_t offset = 99;
    if (!contentRangeValid(16, 16, 0) || contentRangeValid(16, 17, 0) ||
        contentRangeValid(16, 15, 2)) return 1;
    if (!contentTableRecordOffset(16, 4, 3, 4, 2, offset) || offset != 12) return 2;
    offset = 99;
    if (contentTableRecordOffset(16, 4, 4, 4, 0, offset) || offset != 99) return 3;
    if (contentTableRecordOffset(0xffffffffU, 4, 0xffffffffU, 4, 0, offset) ||
        contentTableRecordOffset(16, 4, 3, 0, 0, offset) ||
        contentTableRecordOffset(16, 4, 3, 4, 3, offset)) return 4;
    MemoryContentStorage storage;
    storage.bytes[12] = 42;
    unsigned char record[4]{};
    if (!readContentTableRecord(storage, "trusted-index", 16, 4, 3, 4, 2, record, 4) ||
        record[0] != 42 || storage.reads != 1) return 5;
    if (readContentTableRecord(storage, "trusted-index", 16, 4, 3, 4, 2, record, 3) ||
        storage.reads != 1) return 6;
    storage.shortRead = true;
    if (readContentTableRecord(storage, "trusted-index", 16, 4, 3, 4, 2, record, 4)) return 7;
    if (readContentTableRecord(storage, nullptr, 16, 4, 3, 4, 2, record, 4)) return 8;
    return 0;
}
