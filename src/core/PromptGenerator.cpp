#include "PromptGenerator.h"

namespace core {

PromptGenerator::PromptGenerator(std::uint64_t seed)
    : m_rng(seed)
{
}

Square PromptGenerator::next()
{
    std::uniform_int_distribution<int> distribution(0, 63);

    Square candidate = Square::fromIndex(distribution(m_rng));
    while (m_previous && candidate == *m_previous) {
        candidate = Square::fromIndex(distribution(m_rng));
    }

    m_previous = candidate;
    return candidate;
}

} // namespace core
