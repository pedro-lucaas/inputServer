// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QtGlobal>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

// Windows also closes the job handle if the GUI crashes or is force-killed.
// Only children explicitly assigned here are terminated, never other apps.
class ProcessGuard
{
public:
    ProcessGuard() = default;
    ProcessGuard(const ProcessGuard&) = delete;
    ProcessGuard& operator=(const ProcessGuard&) = delete;
    ~ProcessGuard() { reset(); }
    void reset()
    {
#ifdef Q_OS_WIN
        if (job_) {
            CloseHandle(job_);
            job_ = nullptr;
        }
#endif
    }

    bool attach(qint64 pid)
    {
#ifdef Q_OS_WIN
        reset();
        job_ = CreateJobObjectW(nullptr, nullptr);
        if (!job_) {
            return false;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job_, JobObjectExtendedLimitInformation,
                                     &limits, sizeof(limits))) {
            reset();
            return false;
        }
        HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE,
                                   FALSE, static_cast<DWORD>(pid));
        const bool assigned = child && AssignProcessToJobObject(job_, child);
        if (child) {
            CloseHandle(child);
        }
        if (!assigned) {
            reset();
        }
        return assigned;
#else
        Q_UNUSED(pid);
        return true;
#endif
    }
private:
#ifdef Q_OS_WIN
    HANDLE job_ = nullptr;
#endif
};
