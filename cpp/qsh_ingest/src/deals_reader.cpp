#include "qsh/deals_reader.hpp"
#include "qsh/leb128.hpp"

namespace qsh {

bool DealsReader::next(QshFile& file, DealRecord& record) {
    if (file.eof()) return false;

    const uint8_t* data = file.data.data();
    size_t size = file.data.size();
    size_t& offset = file.data_offset;

    try {
        // frame_time_delta
        record.frame_time_delta = read_growing(data, size, offset);

        // flags
        uint8_t flags = read_u8(data, size, offset);

        // Read fields based on flags
        if (has_flag(flags, DealFlags::Timestamp)) {
            prev_.timestamp += read_growing(data, size, offset);
        }
        if (has_flag(flags, DealFlags::DealId)) {
            prev_.deal_id += read_growing(data, size, offset);
        }
        if (has_flag(flags, DealFlags::OrderId)) {
            prev_.order_id += read_leb128(data, size, offset);
        }
        if (has_flag(flags, DealFlags::Price)) {
            prev_.price += read_leb128(data, size, offset);
        }
        if (has_flag(flags, DealFlags::Amount)) {
            prev_.amount = read_leb128(data, size, offset);
        }
        if (has_flag(flags, DealFlags::OI)) {
            prev_.oi += read_leb128(data, size, offset);
        }

        // Side from low 2 bits
        prev_.side = side_from_u8(flags & 0x03);
        prev_.frame_time_delta = record.frame_time_delta;

        record = prev_;
        return true;

    } catch (const std::exception&) {
        return false;
    }
}

size_t DealsReader::scan_all(QshFile& file, const std::function<void(const DealRecord&)>& callback) {
    size_t count = 0;
    DealRecord rec;
    while (next(file, rec)) {
        callback(rec);
        ++count;
    }
    return count;
}

}  // namespace qsh
