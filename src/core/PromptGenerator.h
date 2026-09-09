#pragma once

#include <cstdint>
#include <optional>
#include <random>

#include "Square.h"

namespace core {

/// Draws squares uniformly at random, rejecting only an immediate repeat.
/// Seeded construction makes the sequence reproducible so round tests can
/// assert exact prompts.
class PromptGenerator
{
public:
    explicit PromptGenerator(std::uint64_t seed);

    Square next();

private:
    std::mt19937_64 m_rng;
    std::optional<Square> m_previous;
};

} // namespace core
