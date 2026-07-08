#include "qsh/auxinfo_reader.hpp"
#include "qsh/leb128.hpp"

namespace qsh {

bool AuxInfoReader::next(QshFile& file, AuxInfoRecord& record) {
    if (file.eof()) return false;

    const uint8_t* data = file.data.data();
    size_t size = file.data.size();
    size_t& offset = file.data_offset;

    try {
        // frame_time_delta
        record.frame_time_delta = read_growing(data, size, offset);

        // flags
        uint8_t flags = read_u8(data, size, offset);
        prev_.frame_time_delta = record.frame_time_delta;

        if (has_flag(flags, AuxInfoFlags::Timestamp)) {
            prev_.timestamp += read_growing(data, size, offset);
        }
        if (has_flag(flags, AuxInfoFlags::AskTotal)) {
            prev_.ask_total += read_leb128(data, size, offset);
        }
        if (has_flag(flags, AuxInfoFlags::BidTotal)) {
            prev_.bid_total += read_leb128(data, size, offset);
        }
        if (has_flag(flags, AuxInfoFlags::OI)) {
            prev_.oi += read_leb128(data, size, offset);
        }
        if (has_flag(flags, AuxInfoFlags::Price)) {
            prev_.price += read_leb128(data, size, offset);
        }
        if (has_flag(flags, AuxInfoFlags::SessionInfo)) {
            prev_.hi_limit = read_leb128(data, size, offset);
            prev_.low_limit = read_leb128(data, size, offset);
            prev_.deposit = read_f64_le(data, size, offset);
        }
        if (has_flag(flags, AuxInfoFlags::Rate)) {
            prev_.rate = read_f64_le(data, size, offset);
        }
        if (has_flag(flags, AuxInfoFlags::Message)) {
            prev_.message = read_qsh_string(data, size, offset);
        } else {
            prev_.message.clear();
        }

        record = prev_;
        return true;

    } catch (const std::exception&) {
        return false;
    }
}

size_t AuxInfoReader::scan_all(QshFile& file, const std::function<void(const AuxInfoRecord&)>& callback) {
    size_t count = 0;
    AuxInfoRecord rec;
    while (next(file, rec)) {
        callback(rec);
        ++count;
    }
    return count;
}

}  // namespace qsh
