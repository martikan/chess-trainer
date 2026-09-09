#pragma once

#include <chrono>

namespace core {

/// A monotonic time source. Deliberately not a wall clock: a system time
/// adjustment mid-round must not change the countdown.
class IClock
{
public:
    virtual ~IClock() = default;
    virtual std::chrono::steady_clock::time_point now() const = 0;
};

class MonotonicClock final : public IClock
{
public:
    std::chrono::steady_clock::time_point now() const override
    {
        return std::chrono::steady_clock::now();
    }
};

/// Lets a test drive a full 30-second round in microseconds.
class FakeClock final : public IClock
{
public:
    std::chrono::steady_clock::time_point now() const override { return m_now; }

    void advance(std::chrono::milliseconds delta) { m_now += delta; }

private:
    std::chrono::steady_clock::time_point m_now{};
};

} // namespace core
