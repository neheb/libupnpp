/* Copyright (C) 2006-2016 J.F.Dockes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *   02110-1301 USA
 */
#include "log.h"

#include <errno.h>
#include <fstream>
#include <time.h>

#ifdef _MSC_VER
#define localtime_r(A,B) localtime_s(B,A)
#endif

Logger::Logger(const std::string& fn)
    : m_fn(fn)
{
    reopen(fn);
}

Logger::~Logger()
{
    if (!m_tocerr && m_stream.is_open()) {
        m_stream.close();
    }
}

bool Logger::reopen(const std::string& fn)
{
#if LOGGER_THREADSAFE
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
#endif
    if (!fn.empty()) {
        m_fn = fn;
    }
    if (!m_tocerr && m_stream.is_open()) {
        m_stream.close();
    }
    if (!m_fn.empty() && m_fn != "stderr") {
        m_stream.open(m_fn, std::fstream::out | std::ofstream::trunc);
        if (!m_stream.is_open()) {
            std::cerr << "Logger::Logger: log open failed: for [" <<
                 fn << "] errno " << errno << "\n";
            m_tocerr = true;
        } else {
            m_tocerr = false;
        }
    } else {
        m_tocerr = true;
    }
    return true;
}

const char *Logger::datestring()
{
    time_t clk = time(nullptr);
    struct tm tmb;
    localtime_r(&clk, &tmb);
    if (strftime(m_datebuf, LOGGER_DATESIZE, m_datefmt.c_str(), &tmb)) {
        return m_datebuf;
    } else {
        return "";
    }
}
static Logger *theLog;

Logger *Logger::getTheLog(const std::string& fn)
{
    if (nullptr == theLog)
        theLog = new Logger(fn);
    return theLog;
}

void Logger::cleanup()
{
    delete theLog;
    theLog = nullptr;
}

