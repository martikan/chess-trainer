#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace core {

/// A single board square, identified by 0-based file and rank.
/// File 0 is the a-file, rank 0 is rank 1, so a1 is (0, 0) and h8 is (7, 7).
class Square
{
public:
    /// Both indices must be in [0, 8). Out-of-range input is a programming
    /// error; use fromAlgebraic() for anything derived from text.
    Square(int file, int rank);

    /// Parses "e4". Case-insensitive on the file letter. Returns nullopt for
    /// any input that is not exactly one file letter followed by one rank digit.
    static std::optional<Square> fromAlgebraic(std::string_view text);

    int file() const { return m_file; }
    int rank() const { return m_rank; }

    std::string algebraic() const;

    /// a1 is dark, so a square is dark exactly when file + rank is even.
    bool isDark() const { return ((m_file + m_rank) % 2) == 0; }

    /// Index in [0, 64), file-major: a1 = 0, b1 = 1, a2 = 8.
    int index() const { return m_rank * 8 + m_file; }
    static Square fromIndex(int index);

    friend bool operator==(const Square &, const Square &) = default;

private:
    int m_file;
    int m_rank;
};

} // namespace core
