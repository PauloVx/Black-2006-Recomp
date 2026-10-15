#include "core/TraceLog.h"

#include "core/Text.h"

#include <utility>

namespace black::core {

TraceLog::TraceLog(const std::filesystem::path& path, std::uint64_t limit, std::string tag)
    : m_path(path), m_out(path, std::ios::binary | std::ios::trunc), m_limit(limit), m_tag(std::move(tag)) {
    if (!m_out) {
        m_failure = "Could not create " + toUtf8(path);
    }
}

TraceLog::~TraceLog() {
    complete();
}

void TraceLog::write(std::string_view line) {
    const std::scoped_lock lock(m_mutex);
    if (m_complete || !m_failure.empty()) {
        return;
    }
    const std::uint64_t size = line.size() + 1;
    if (m_limit != 0 && m_written + size > m_limit - kFooterReserve) {
        ++m_dropped;
        return;
    }
    m_out.write(line.data(), static_cast<std::streamsize>(line.size()));
    m_out.put('\n');
    m_out.flush();
    m_written += size;
    if (!m_out) {
        m_failure = "Could not write " + toUtf8(m_path);
    }
}

void TraceLog::complete() {
    const std::scoped_lock lock(m_mutex);
    if (m_complete) {
        return;
    }
    m_complete = true;
    if (m_dropped > 0 && m_failure.empty()) {
        m_out << m_tag << " TRUNCATED: " << m_dropped << " lines discarded (limit " << m_limit << " bytes).\n";
    }
    m_out.flush();
    if (!m_out && m_failure.empty()) {
        m_failure = "Could not write " + toUtf8(m_path);
    }
}

std::string TraceLog::failure() const {
    const std::scoped_lock lock(m_mutex);
    return m_failure;
}

}  // namespace black::core
