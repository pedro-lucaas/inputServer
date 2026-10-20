// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <utility>

// Stops only the supplied child, without blocking the GUI event loop.
class AsyncProcessStopper : public QObject
{
public:
    explicit AsyncProcessStopper(int graceMs = 3000, int killMs = 2000)
        : graceMs_(graceMs), killMs_(killMs)
    {
        timer_.setSingleShot(true);
        connect(&timer_, &QTimer::timeout, this, [this] {
            if (!process_ || process_->state() == QProcess::NotRunning) return complete(true);
            if (killing_) return complete(false);
            killing_ = true;
            process_->kill();
            timer_.start(killMs_);
        });
    }

    bool busy() const { return bool(completion_); }

    void stop(QProcess* process, std::function<void(bool)> completion)
    {
        if (busy()) return;
        process_ = process;
        completion_ = std::move(completion);
        killing_ = false;
        if (!process_ || process_->state() == QProcess::NotRunning) {
            QTimer::singleShot(0, this, [this] { complete(true); });
            return;
        }
        finished_ = connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                            this, [this] { complete(true); });
        started_ = connect(process_, &QProcess::started, this, [this] {
            if (process_) process_->terminate();
        });
        destroyed_ = connect(process_, &QObject::destroyed, this, [this] { complete(true); });
        timer_.start(graceMs_);
        process_->terminate();
    }

private:
    void complete(bool success)
    {
        if (!busy()) return;
        timer_.stop();
        disconnect(finished_);
        disconnect(started_);
        disconnect(destroyed_);
        process_.clear();
        auto completion = std::move(completion_);
        completion_ = {};
        completion(success);
    }

    QPointer<QProcess> process_;
    QTimer timer_;
    QMetaObject::Connection finished_, started_, destroyed_;
    std::function<void(bool)> completion_;
    int graceMs_, killMs_;
    bool killing_ = false;
};
