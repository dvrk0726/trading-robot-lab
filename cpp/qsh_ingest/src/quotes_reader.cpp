#include "qsh/quotes_reader.hpp"
#include "qsh/leb128.hpp"

namespace qsh {

bool QuotesReader::next(QshFile& file, QuotesRecord& record) {
    if (file.eof()) return false;

    const uint8_t* data = file.data.data();
    size_t size = file.data.size();
    size_t& offset = file.data_offset;

    try {
        record.levels.clear();

        // frame_time_delta
        record.frame_time_delta = read_growing(data, size, offset);

        // number of rows
        int64_t nrows = read_leb128(data, size, offset);

        // Read incremental book updates
        for (int64_t i = 0; i < nrows; ++i) {
            key_ += read_leb128(data, size, offset);
            Volume v = read_leb128(data, size, offset);
            if (v == 0) {
                book_.erase(key_);
            } else {
                book_[key_] = v;
            }
        }

        // Build snapshot from current book state
        for (const auto& [price, volume] : book_) {
            record.levels.push_back({price, volume});
        }

        return true;

    } catch (const std::exception&) {
        return false;
    }
}

size_t QuotesReader::scan_all(QshFile& file, const std::function<void(const QuotesRecord&)>& callback) {
    size_t count = 0;
    QuotesRecord rec;
    while (next(file, rec)) {
        callback(rec);
        ++count;
    }
    return count;
}

}  // namespace qsh
