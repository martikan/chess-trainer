#include "Square.h"

#include <cassert>
#include <cctype>

namespace core {

Square::Square(int file, int rank)
    : m_file(file)
    , m_rank(rank)
{
    assert(file >= 0 && file < 8);
    assert(rank >= 0 && rank < 8);
}

Square Square::fromIndex(int index)
{
    assert(index >= 0 && index < 64);
    return Square(index % 8, index / 8);
}

std::optional<Square> Square::fromAlgebraic(std::string_view text)
{
    if (text.size() != 2) {
        return std::nullopt;
    }

    const auto fileChar = static_cast<char>(
        std::tolower(static_cast<unsigned char>(text[0])));
    const char rankChar = text[1];

    if (fileChar < 'a' || fileChar > 'h') {
        return std::nullopt;
    }
    if (rankChar < '1' || rankChar > '8') {
        return std::nullopt;
    }

    return Square(fileChar - 'a', rankChar - '1');
}

std::string Square::algebraic() const
{
    return std::string{static_cast<char>('a' + m_file),
                       static_cast<char>('1' + m_rank)};
}

} // namespace core
