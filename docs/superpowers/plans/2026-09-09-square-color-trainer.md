# Square Color Trainer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a Linux chess board-vision trainer whose first module is a 30-second Square Color sprint, inside a Kirigami shell that can host further modules.

**Architecture:** Three layers with hard boundaries. `src/core/` holds every rule that can be wrong — the colour rule, prompt sequence, round state machine, scoring — in C++20 with no Qt at all, so its tests need neither an event loop nor a display. `src/store/` persists runs and per-answer response times to SQLite via QtSql. `src/app/` exposes `QObject` bridges to a Kirigami QML UI that holds bindings only. That core boundary is also the coverage boundary.

**Tech Stack:** C++20, CMake 3.28+, Qt 6.6+ (Core, Gui, Widgets, Quick, QuickControls2, Sql, Test), KDE Frameworks 6.5+ (Kirigami, CoreAddons, I18n), `kf6-qqc2-desktop-style`, Extra CMake Modules, QTest + CTest, GitHub Actions, Codecov.

**Spec:** `docs/superpowers/specs/2026-09-09-square-color-trainer-design.md`

## Global Constraints

Every task's requirements implicitly include this section.

- **Application ID:** `io.github.martikan.ChessTrainer` — used verbatim in the desktop entry filename, AppStream metainfo filename, icon filename, and `KAboutData` component name.
- **Binary name:** `chess-trainer`.
- **C++ standard:** C++20. Do not use C++23 features (`std::expected` in particular); error reporting uses `std::optional` returns plus a `QString* errorOut` parameter.
- **`chesstrainer-core` links no Qt libraries.** Not `Qt6::Core`, not for convenience types. Use `std::string`, `std::optional`, `std::chrono`, `std::mt19937_64`. `AUTOMOC` is explicitly `OFF` on that target. Conversion to Qt types happens in `src/app/`.
- **Qt version floor:** `find_package(Qt6 6.6 ...)`. **KF6 floor:** `find_package(KF6 6.5 ...)`.
- **`QApplication`, not `QGuiApplication`.** `qqc2-desktop-style` pulls in QtWidgets; using `QGuiApplication` makes the style fail to load at runtime.
- **All user-visible strings go through `i18n()`** / `i18nc()` from the first commit. No translations ship in this phase; the call sites do.
- **Colours come from `Kirigami.Theme`, spacing from `Kirigami.Units`.** No hardcoded hex colours and no hardcoded pixel margins anywhere in QML. The mockup hex values in the spec were for the mockup only.
- **Round length constant:** `SquareColorRound::kDefaultRoundLength` = 30'000 ms. It is the single definition; nothing else hardcodes 30 seconds.
- **Database column is `round_length_ms`**, never `duration_ms`.
- **Schema version 1** is set via `PRAGMA user_version`. `PRAGMA foreign_keys = ON` on every connection.
- **Commits:** conventional-commit prefixes (`feat:`, `test:`, `chore:`, `docs:`, `ci:`). Signing is currently broken for the personal GPG key, so append `--no-gpg-sign` to every `git commit` until told otherwise.

## Deviations from the spec, recorded

Three, all deliberate.

**QML lives at `src/app/qml/`, not top-level `qml/`** (spec §3.1). `qt_add_qml_module` derives resource aliases from paths relative to the declaring `CMakeLists.txt`, and a top-level `qml/` reached as `../../qml/main.qml` produces mangled resource URLs needing per-file `QT_RESOURCE_ALIAS` fixups. The controllers and the QML that binds to them also change together, so they belong together.

**Round summary computation moved from the controller into the core** (spec §6). The spec put accuracy, mean response time and the slowest-three ranking in `SquareColorController`. But `src/app/` is excluded from coverage by §9.3, and those three are real computations that can be wrong — the slowest-three ranking in particular has to collapse a repeated square to its worst time, and accuracy has to round rather than truncate. Leaving them in the uncovered layer would put untested arithmetic in front of the user, so they became `core::summarise()` in Task 10, with `src/core/RoundSummary.h` and `.cpp` added to the §3.1 tree.

**Seven test executables, not the four listed in §8.** `tst_database` and `tst_repository` are split because the migration ladder and the query layer fail for unrelated reasons and a combined binary would obscure which. `tst_roundsummary` and `tst_moduleregistry` cover units the spec's list predates.

Everything else follows the spec as written.

## File Structure

**`src/core/`** — pure C++20, no Qt, one responsibility each:
- `Square.h` / `Square.cpp` — board square value type: file/rank, algebraic parsing and formatting, the colour rule.
- `PromptGenerator.h` / `PromptGenerator.cpp` — seeded uniform square selection that never repeats consecutively.
- `Clock.h` — `IClock` interface plus `MonotonicClock` and `FakeClock` (header-only; nothing to compile).
- `SquareColorRound.h` / `SquareColorRound.cpp` — the drill: state machine, timing, scoring, answer records.
- `ModuleDescriptor.h` — plain struct describing one training module.
- `ModuleRegistry.h` / `ModuleRegistry.cpp` — the static list of descriptors.

**`src/store/`** — SQLite via QtSql:
- `Database.h` / `Database.cpp` — connection ownership, `PRAGMA` setup, the `user_version` migration ladder.
- `RunRepository.h` / `RunRepository.cpp` — `writeRun()` in one transaction, `moduleSummary()` reads.

**`src/app/`** — Qt/QML bridges:
- `main.cpp` — `QApplication`, `QQuickStyle`, `KAboutData`, `KLocalizedContext`, engine bootstrap, database wiring.
- `SquareColorController.h` / `.cpp` — owns a `SquareColorRound`, drives it with a `QTimer`, exposes properties and invokables, writes the run at the end.
- `ModuleListModel.h` / `.cpp` — `QAbstractListModel` over `ModuleRegistry`, joined with `moduleSummary()`.
- `qml/main.qml` — `Kirigami.ApplicationWindow`, global drawer, page stack.
- `qml/HomePage.qml` — scrollable cards list of modules.
- `qml/DrillPage.qml` — immersive drill; owns all key handling.
- `qml/SummarySheet.qml` — post-round results.
- `qml/components/CountdownRing.qml` — canvas ring bound to remaining time.
- `qml/components/AnswerButton.qml` — non-focusable answer button with tint flash.

**`tests/`** — one QTest executable per core/store unit: `tst_square.cpp`, `tst_promptgenerator.cpp`, `tst_round.cpp`, `tst_database.cpp`, `tst_repository.cpp`, `tst_moduleregistry.cpp`.

**Repository root** — `CMakeLists.txt`, `.github/workflows/ci.yml`, `.codecov.yml`, `data/` (desktop entry, metainfo, icon).

---

### Task 1: Build scaffolding and the Square value type

The colour rule is the one piece of logic the whole application rests on, so it goes first and brings the build system with it.

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/core/CMakeLists.txt`
- Create: `src/core/Square.h`
- Create: `src/core/Square.cpp`
- Create: `tests/CMakeLists.txt`
- Test: `tests/tst_square.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: CMake targets `chesstrainer-core` (static, no Qt) and the `tests/` CTest wiring. `core::Square` with `Square(int file, int rank)`, `static std::optional<Square> fromAlgebraic(std::string_view)`, `int file() const`, `int rank() const`, `std::string algebraic() const`, `bool isDark() const`, and defaulted `operator==`.

- [ ] **Step 1: Install the build dependencies**

Run in the toolbox container:

```bash
sudo dnf install -y cmake ninja-build gcc-c++ extra-cmake-modules \
  qt6-qtbase-devel qt6-qtdeclarative-devel \
  kf6-kirigami-devel kf6-qqc2-desktop-style \
  kf6-kcoreaddons-devel kf6-ki18n-devel
```

- [ ] **Step 2: Write the top-level CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.28)

project(chess-trainer
    VERSION 0.1.0
    DESCRIPTION "Chess board vision trainer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM 6.5 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(ECMInstallIcons)
include(FeatureSummary)

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core Gui Widgets Quick QuickControls2 Sql Test)
find_package(KF6 6.5 REQUIRED COMPONENTS Kirigami CoreAddons I18n)

qt_standard_project_setup(REQUIRES 6.6)

include(CTest)
enable_testing()

add_subdirectory(src/core)
add_subdirectory(tests)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
```

`src/store` and `src/app` are added in later tasks. Adding them now would break configuration.

- [ ] **Step 3: Write src/core/CMakeLists.txt**

```cmake
add_library(chesstrainer-core STATIC
    Square.cpp
)

target_include_directories(chesstrainer-core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(chesstrainer-core PUBLIC cxx_std_20)

# This target must never link Qt. qt_standard_project_setup() turns AUTOMOC on
# globally; switch it off here so a stray Q_OBJECT fails loudly instead of
# quietly pulling Qt into the domain layer.
set_target_properties(chesstrainer-core PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    POSITION_INDEPENDENT_CODE ON)
```

- [ ] **Step 4: Write the failing test**

`tests/tst_square.cpp`:

```cpp
#include <QtTest>

#include "Square.h"

using core::Square;

class TestSquare : public QObject
{
    Q_OBJECT

private slots:
    void allSixtyFourSquaresHaveCorrectColour();
    void cornersMatchARealBoard();
    void algebraicRoundTrips();
    void fromAlgebraicAcceptsUppercase();
    void fromAlgebraicRejectsMalformedInput_data();
    void fromAlgebraicRejectsMalformedInput();
};

void TestSquare::allSixtyFourSquaresHaveCorrectColour()
{
    // A square is dark when file + rank is even, with both indices 0-based.
    // Verified independently here by parity of the algebraic characters.
    for (int file = 0; file < 8; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            const Square square(file, rank);
            const bool expectedDark = ((file + rank) % 2) == 0;
            QCOMPARE(square.isDark(), expectedDark);
        }
    }
}

void TestSquare::cornersMatchARealBoard()
{
    QVERIFY(Square::fromAlgebraic("a1")->isDark());
    QVERIFY(!Square::fromAlgebraic("h1")->isDark());
    QVERIFY(!Square::fromAlgebraic("a8")->isDark());
    QVERIFY(Square::fromAlgebraic("h8")->isDark());
}

void TestSquare::algebraicRoundTrips()
{
    for (int file = 0; file < 8; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            const Square original(file, rank);
            const auto parsed = Square::fromAlgebraic(original.algebraic());
            QVERIFY(parsed.has_value());
            QCOMPARE(*parsed, original);
        }
    }
    QCOMPARE(Square(4, 3).algebraic(), std::string("e4"));
}

void TestSquare::fromAlgebraicAcceptsUppercase()
{
    const auto parsed = Square::fromAlgebraic("E4");
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->algebraic(), std::string("e4"));
}

void TestSquare::fromAlgebraicRejectsMalformedInput_data()
{
    // QString, not std::string: QTest columns require a registered metatype,
    // and std::string is not one by default.
    QTest::addColumn<QString>("text");

    QTest::newRow("empty")          << QString();
    QTest::newRow("file too high")  << QStringLiteral("i4");
    QTest::newRow("rank zero")      << QStringLiteral("e0");
    QTest::newRow("rank too high")  << QStringLiteral("e9");
    QTest::newRow("too long")       << QStringLiteral("e44");
    QTest::newRow("one character")  << QStringLiteral("e");
    QTest::newRow("reversed")       << QStringLiteral("4e");
    QTest::newRow("whitespace")     << QStringLiteral(" e4");
}

void TestSquare::fromAlgebraicRejectsMalformedInput()
{
    QFETCH(QString, text);
    QVERIFY(!Square::fromAlgebraic(text.toStdString()).has_value());
}

QTEST_APPLESS_MAIN(TestSquare)
#include "tst_square.moc"
```

Note `QTEST_APPLESS_MAIN` — these tests construct no `QCoreApplication`, which is what makes a Qt-free core worth having.

- [ ] **Step 5: Write tests/CMakeLists.txt**

```cmake
# One executable per unit under test. Each is registered with CTest.
set(CORE_TESTS
    square
)

foreach(test_name IN LISTS CORE_TESTS)
    add_executable(tst_${test_name} tst_${test_name}.cpp)
    target_link_libraries(tst_${test_name} PRIVATE
        Qt6::Test
        chesstrainer-core)
    add_test(NAME tst_${test_name} COMMAND tst_${test_name})
endforeach()
```

`CORE_TESTS` grows in later tasks. `Qt6::Test` links into the *test* binary, not into `chesstrainer-core` — the core stays Qt-free.

- [ ] **Step 6: Run the test to verify it fails**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Expected: build FAILS with `fatal error: Square.h: No such file or directory`.

- [ ] **Step 7: Write src/core/Square.h**

```cpp
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
```

- [ ] **Step 8: Write src/core/Square.cpp**

```cpp
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
```

- [ ] **Step 9: Run the test to verify it passes**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: `tst_square` PASSES, 6 test functions, `100% tests passed`.

- [ ] **Step 10: Commit**

```bash
git add CMakeLists.txt src/core tests
git commit --no-gpg-sign -m "feat: add Square value type with the board colour rule

Establishes the CMake build with a deliberately Qt-free core target: AUTOMOC
is off on chesstrainer-core so a stray Q_OBJECT fails the build rather than
quietly pulling Qt into the domain layer. Qt6::Test links into the test
binaries only.

Tests cover all 64 squares against an independently computed parity, the four
board corners by name, algebraic round-tripping, uppercase input, and eight
malformed-input cases."
```

---

### Task 2: CI with coverage reporting

Wired second so every later task is guarded, and so the coverage boundary is enforced by configuration rather than by memory.

**Files:**
- Create: `.github/workflows/ci.yml`
- Create: `.codecov.yml`

**Interfaces:**
- Consumes: the `chesstrainer-core` target and `tests/` CTest registration from Task 1.
- Produces: a CI job named `build-and-test` that other tasks rely on for verification, and coverage scoped to `src/core/` and `src/store/`.

- [ ] **Step 1: Write .github/workflows/ci.yml**

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    container: fedora:43

    steps:
      - name: Install dependencies
        run: |
          dnf install -y --setopt=install_weak_deps=False \
            cmake ninja-build gcc-c++ git lcov \
            extra-cmake-modules \
            qt6-qtbase-devel qt6-qtdeclarative-devel \
            kf6-kirigami-devel kf6-qqc2-desktop-style \
            kf6-kcoreaddons-devel kf6-ki18n-devel

      - uses: actions/checkout@v4

      - name: Configure
        run: |
          cmake -B build -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DBUILD_TESTING=ON \
            -DCMAKE_CXX_FLAGS="--coverage -O0 -g" \
            -DCMAKE_EXE_LINKER_FLAGS="--coverage"

      - name: Build
        run: cmake --build build

      - name: Test
        run: ctest --test-dir build --output-on-failure

      - name: Collect coverage
        run: |
          lcov --directory build --capture --output-file coverage.info \
            --ignore-errors mismatch,gcov,empty
          lcov --remove coverage.info \
            '/usr/*' '*/tests/*' '*/src/app/*' '*/build/*' \
            --output-file coverage.info \
            --ignore-errors unused
          lcov --list coverage.info

      - name: Upload coverage
        uses: codecov/codecov-action@v5
        with:
          files: coverage.info
          token: ${{ secrets.CODECOV_TOKEN }}
          fail_ci_if_error: true
```

No `xvfb` and no display: every test in this plan uses `QTEST_APPLESS_MAIN` or `QTEST_GUILESS_MAIN`, and the QML layer is deliberately untested (spec §8).

- [ ] **Step 2: Write .codecov.yml**

```yaml
coverage:
  status:
    project:
      default:
        target: auto
        threshold: 1%
    patch: off

ignore:
  - "src/app/**"      # QObject bridges and QML — untested by design, spec 8
  - "tests/**"
  - "build/**"

comment:
  layout: "reach, diff, files"
  require_changes: true
```

`src/app/**` covers the QML too, since this plan places it at `src/app/qml/`.

- [ ] **Step 3: Add the CODECOV_TOKEN secret**

This step is the repository owner's to perform, in a browser:

1. Open https://app.codecov.io/gh/martikan/chess-trainer and copy the upload token.
2. Open https://github.com/martikan/chess-trainer/settings/secrets/actions
3. Add a repository secret named `CODECOV_TOKEN` with that value.

`fail_ci_if_error: true` means CI fails loudly if the token is missing, rather than silently reporting no coverage.

- [ ] **Step 4: Commit and push, then verify CI is green**

```bash
git add .github .codecov.yml
git commit --no-gpg-sign -m "ci: build, test and report coverage on Fedora 43

Runs in a fedora:43 container so CI and local toolbox builds share one
dependency set. Coverage is scoped to src/core and src/store: src/app holds
QObject bridges and QML that are untested by design, and including them would
make the metric track UI glue instead of logic."
git push -u origin main
```

Then:

```bash
gh run watch --repo martikan/chess-trainer
```

Expected: `build-and-test` succeeds, `tst_square` passes, coverage uploads. If the push is rejected, the remote is `git@github-personal:martikan/chess-trainer.git` — the bare `github.com` host maps to a read-only account.

---

### Task 3: Prompt generation and the clock abstraction

Grouped because neither is useful alone: the round needs a prompt source and an injectable time source, and `Clock.h` is header-only with almost no test surface of its own.

**Files:**
- Create: `src/core/Clock.h`
- Create: `src/core/PromptGenerator.h`
- Create: `src/core/PromptGenerator.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/tst_promptgenerator.cpp`

**Interfaces:**
- Consumes: `core::Square` from Task 1.
- Produces:
  - `core::IClock` with `virtual std::chrono::steady_clock::time_point now() const`.
  - `core::MonotonicClock final : IClock`.
  - `core::FakeClock final : IClock` with `void advance(std::chrono::milliseconds)`.
  - `core::PromptGenerator` with `explicit PromptGenerator(std::uint64_t seed)` and `Square next()`.

- [ ] **Step 1: Write the failing test**

`tests/tst_promptgenerator.cpp`:

```cpp
#include <QtTest>

#include <set>

#include "PromptGenerator.h"

using core::PromptGenerator;
using core::Square;

class TestPromptGenerator : public QObject
{
    Q_OBJECT

private slots:
    void neverRepeatsConsecutively();
    void sameSeedProducesSameSequence();
    void differentSeedsDiverge();
    void coversEverySquare();
};

void TestPromptGenerator::neverRepeatsConsecutively()
{
    PromptGenerator generator(1234);

    Square previous = generator.next();
    for (int i = 0; i < 5000; ++i) {
        const Square current = generator.next();
        QVERIFY2(!(current == previous),
                 qPrintable(QStringLiteral("repeated %1 at draw %2")
                                .arg(QString::fromStdString(current.algebraic()))
                                .arg(i)));
        previous = current;
    }
}

void TestPromptGenerator::sameSeedProducesSameSequence()
{
    PromptGenerator first(42);
    PromptGenerator second(42);

    for (int i = 0; i < 200; ++i) {
        QCOMPARE(first.next(), second.next());
    }
}

void TestPromptGenerator::differentSeedsDiverge()
{
    PromptGenerator first(1);
    PromptGenerator second(2);

    bool sawDifference = false;
    for (int i = 0; i < 200 && !sawDifference; ++i) {
        if (!(first.next() == second.next())) {
            sawDifference = true;
        }
    }
    QVERIFY(sawDifference);
}

void TestPromptGenerator::coversEverySquare()
{
    PromptGenerator generator(7);

    std::set<int> seen;
    for (int i = 0; i < 20000; ++i) {
        seen.insert(generator.next().index());
    }
    QCOMPARE(static_cast<int>(seen.size()), 64);
}

QTEST_APPLESS_MAIN(TestPromptGenerator)
#include "tst_promptgenerator.moc"
```

- [ ] **Step 2: Register the test**

In `tests/CMakeLists.txt`, extend the list:

```cmake
set(CORE_TESTS
    square
    promptgenerator
)
```

- [ ] **Step 3: Run the test to verify it fails**

```bash
cmake --build build
```

Expected: build FAILS with `fatal error: PromptGenerator.h: No such file or directory`.

- [ ] **Step 4: Write src/core/Clock.h**

```cpp
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
```

- [ ] **Step 5: Write src/core/PromptGenerator.h**

```cpp
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
```

- [ ] **Step 6: Write src/core/PromptGenerator.cpp**

```cpp
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
```

The rejection loop terminates with probability 1 and expected 64/63 draws, because the distribution has 64 outcomes and only one is excluded.

- [ ] **Step 7: Add PromptGenerator.cpp to the core target**

In `src/core/CMakeLists.txt`:

```cmake
add_library(chesstrainer-core STATIC
    Square.cpp
    PromptGenerator.cpp
)
```

`Clock.h` needs no entry — it is header-only.

- [ ] **Step 8: Run the tests to verify they pass**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: `tst_square` and `tst_promptgenerator` both PASS.

- [ ] **Step 9: Commit**

```bash
git add src/core tests
git commit --no-gpg-sign -m "feat: add seeded prompt generation and clock abstraction

PromptGenerator draws uniformly over all 64 squares and rejects only an
immediate repeat, so a prompt never appears twice in a row. Seeding makes the
sequence reproducible, which is what lets the round tests assert exact prompts.

IClock exists so the round can be driven by FakeClock in tests instead of real
time. It wraps steady_clock rather than a wall clock: a system time adjustment
mid-round must not move the countdown."
```

---

### Task 4: The round state machine

The heart of the drill. Every timing and scoring rule lives here, tested without waiting a single real millisecond.

**Files:**
- Create: `src/core/SquareColorRound.h`
- Create: `src/core/SquareColorRound.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/tst_round.cpp`

**Interfaces:**
- Consumes: `core::Square`, `core::PromptGenerator`, `core::IClock`, `core::FakeClock`.
- Produces:
  - `enum class core::RoundState { Idle, Running, Finished, Aborted }`
  - `enum class core::AnswerOutcome { Correct, Wrong, Ignored }`
  - `struct core::Answer { int ordinal; Square square; bool expectedDark; bool answeredDark; bool correct; std::chrono::milliseconds responseTime; }`
  - `core::SquareColorRound` with `kDefaultRoundLength`, `start()`, `abort()`, `state()`, `currentPrompt()`, `remaining()`, `tick()`, `answer(bool)`, `correct()`, `wrong()`, `answers()`.

- [ ] **Step 1: Write the failing test**

`tests/tst_round.cpp`:

```cpp
#include <QtTest>

#include "Clock.h"
#include "PromptGenerator.h"
#include "SquareColorRound.h"

using namespace std::chrono_literals;

using core::AnswerOutcome;
using core::FakeClock;
using core::PromptGenerator;
using core::RoundState;
using core::SquareColorRound;

namespace {

/// A round wired to a clock the test controls. Seed 42 is arbitrary but fixed,
/// so prompts are reproducible.
struct Fixture {
    FakeClock clock;
    SquareColorRound round{clock, PromptGenerator(42), 30'000ms};
};

/// Answers the current prompt correctly and returns the outcome.
AnswerOutcome answerCorrectly(SquareColorRound &round)
{
    return round.answer(round.currentPrompt().isDark());
}

} // namespace

class TestRound : public QObject
{
    Q_OBJECT

private slots:
    void startsIdleAndBecomesRunning();
    void remainingCountsDownAndClampsAtZero();
    void tickFinishesTheRoundExactlyOnce();
    void correctAnswerScoresAndAdvancesThePrompt();
    void wrongAnswerScoresAsWrong();
    void answerRecordsResponseTime();
    void answerAfterExpiryIsIgnoredAndRecordsNothing();
    void answerBeforeStartIsIgnored();
    void abortStopsTheRoundAndKeepsAnswers();
    void answersCarrySequentialOrdinals();
};

void TestRound::startsIdleAndBecomesRunning()
{
    Fixture fixture;
    QCOMPARE(fixture.round.state(), RoundState::Idle);
    QCOMPARE(fixture.round.correct(), 0);
    QCOMPARE(fixture.round.wrong(), 0);

    fixture.round.start();
    QCOMPARE(fixture.round.state(), RoundState::Running);
}

void TestRound::remainingCountsDownAndClampsAtZero()
{
    Fixture fixture;
    QCOMPARE(fixture.round.remaining(), 30'000ms);

    fixture.round.start();
    QCOMPARE(fixture.round.remaining(), 30'000ms);

    fixture.clock.advance(10'000ms);
    QCOMPARE(fixture.round.remaining(), 20'000ms);

    fixture.clock.advance(25'000ms);
    QCOMPARE(fixture.round.remaining(), 0ms);
}

void TestRound::tickFinishesTheRoundExactlyOnce()
{
    Fixture fixture;
    fixture.round.start();

    fixture.clock.advance(29'999ms);
    QVERIFY(!fixture.round.tick());
    QCOMPARE(fixture.round.state(), RoundState::Running);

    fixture.clock.advance(1ms);
    QVERIFY(fixture.round.tick());
    QCOMPARE(fixture.round.state(), RoundState::Finished);

    // Only the transition reports true, so a 16 ms timer cannot fire the
    // end-of-round handler repeatedly.
    QVERIFY(!fixture.round.tick());
}

void TestRound::correctAnswerScoresAndAdvancesThePrompt()
{
    Fixture fixture;
    fixture.round.start();

    const auto firstPrompt = fixture.round.currentPrompt();
    QCOMPARE(answerCorrectly(fixture.round), AnswerOutcome::Correct);
    QCOMPARE(fixture.round.correct(), 1);
    QCOMPARE(fixture.round.wrong(), 0);
    QVERIFY(!(fixture.round.currentPrompt() == firstPrompt));
}

void TestRound::wrongAnswerScoresAsWrong()
{
    Fixture fixture;
    fixture.round.start();

    const bool wrongAnswer = !fixture.round.currentPrompt().isDark();
    QCOMPARE(fixture.round.answer(wrongAnswer), AnswerOutcome::Wrong);
    QCOMPARE(fixture.round.correct(), 0);
    QCOMPARE(fixture.round.wrong(), 1);
}

void TestRound::answerRecordsResponseTime()
{
    Fixture fixture;
    fixture.round.start();

    const auto prompt = fixture.round.currentPrompt();
    fixture.clock.advance(480ms);
    answerCorrectly(fixture.round);

    fixture.clock.advance(1'320ms);
    answerCorrectly(fixture.round);

    const auto &answers = fixture.round.answers();
    QCOMPARE(static_cast<int>(answers.size()), 2);
    QCOMPARE(answers[0].square, prompt);
    QCOMPARE(answers[0].responseTime, 480ms);
    QCOMPARE(answers[0].expectedDark, prompt.isDark());
    QCOMPARE(answers[0].answeredDark, prompt.isDark());
    QVERIFY(answers[0].correct);
    // Measured from prompt shown, not from round start.
    QCOMPARE(answers[1].responseTime, 1'320ms);
}

void TestRound::answerAfterExpiryIsIgnoredAndRecordsNothing()
{
    Fixture fixture;
    fixture.round.start();
    answerCorrectly(fixture.round);

    fixture.clock.advance(30'000ms);
    QCOMPARE(fixture.round.answer(true), AnswerOutcome::Ignored);

    QCOMPARE(static_cast<int>(fixture.round.answers().size()), 1);
    QCOMPARE(fixture.round.correct(), 1);
    QCOMPARE(fixture.round.wrong(), 0);
    QCOMPARE(fixture.round.state(), RoundState::Finished);
}

void TestRound::answerBeforeStartIsIgnored()
{
    Fixture fixture;
    QCOMPARE(fixture.round.answer(true), AnswerOutcome::Ignored);
    QVERIFY(fixture.round.answers().empty());
    QCOMPARE(fixture.round.state(), RoundState::Idle);
}

void TestRound::abortStopsTheRoundAndKeepsAnswers()
{
    Fixture fixture;
    fixture.round.start();
    answerCorrectly(fixture.round);

    fixture.round.abort();
    QCOMPARE(fixture.round.state(), RoundState::Aborted);
    QCOMPARE(static_cast<int>(fixture.round.answers().size()), 1);
    QCOMPARE(fixture.round.remaining(), 0ms);

    // Aborted is terminal: no further answers, and tick() reports no transition.
    QCOMPARE(fixture.round.answer(true), AnswerOutcome::Ignored);
    QVERIFY(!fixture.round.tick());
    QCOMPARE(fixture.round.state(), RoundState::Aborted);
}

void TestRound::answersCarrySequentialOrdinals()
{
    Fixture fixture;
    fixture.round.start();

    for (int i = 0; i < 5; ++i) {
        fixture.clock.advance(100ms);
        answerCorrectly(fixture.round);
    }

    const auto &answers = fixture.round.answers();
    QCOMPARE(static_cast<int>(answers.size()), 5);
    for (int i = 0; i < 5; ++i) {
        QCOMPARE(answers[i].ordinal, i + 1);
    }
}

QTEST_APPLESS_MAIN(TestRound)
#include "tst_round.moc"
```

- [ ] **Step 2: Register the test**

```cmake
set(CORE_TESTS
    square
    promptgenerator
    round
)
```

- [ ] **Step 3: Run the test to verify it fails**

```bash
cmake --build build
```

Expected: build FAILS with `fatal error: SquareColorRound.h: No such file or directory`.

- [ ] **Step 4: Write src/core/SquareColorRound.h**

```cpp
#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "Clock.h"
#include "PromptGenerator.h"
#include "Square.h"

namespace core {

enum class RoundState {
    Idle,
    Running,
    Finished,
    Aborted,
};

enum class AnswerOutcome {
    Correct,
    Wrong,
    Ignored,
};

struct Answer {
    int ordinal = 0; ///< 1-based position within the round
    Square square{0, 0};
    bool expectedDark = false;
    bool answeredDark = false;
    bool correct = false;
    std::chrono::milliseconds responseTime{0};
};

/// A timed Square Color sprint: show a square name, take a light/dark answer,
/// repeat until the clock runs out.
class SquareColorRound
{
public:
    static constexpr std::chrono::milliseconds kDefaultRoundLength{30'000};

    SquareColorRound(const IClock &clock,
                     PromptGenerator generator,
                     std::chrono::milliseconds roundLength = kDefaultRoundLength);

    void start();

    /// Ends the round early. Answers already given are kept.
    void abort();

    RoundState state() const { return m_state; }

    /// Precondition: state() == RoundState::Running.
    Square currentPrompt() const;

    /// Clamped at zero. Reports the full round length before start() and zero
    /// once the round is finished or aborted.
    std::chrono::milliseconds remaining() const;

    /// Advances the state machine. Returns true only on the transition into
    /// Finished, so a repeating timer cannot fire the end handler twice.
    bool tick();

    /// Records an answer to the current prompt and advances to the next one.
    /// Returns Ignored, recording nothing, unless the round is running and the
    /// clock has not expired.
    AnswerOutcome answer(bool answeredDark);

    int correct() const { return m_correct; }
    int wrong() const { return m_wrong; }
    std::chrono::milliseconds roundLength() const { return m_roundLength; }
    const std::vector<Answer> &answers() const { return m_answers; }

private:
    bool expired() const;
    void showNextPrompt();

    const IClock &m_clock;
    PromptGenerator m_generator;
    std::chrono::milliseconds m_roundLength;

    RoundState m_state = RoundState::Idle;
    std::chrono::steady_clock::time_point m_roundStart{};
    std::chrono::steady_clock::time_point m_promptShownAt{};
    std::optional<Square> m_currentPrompt;

    std::vector<Answer> m_answers;
    int m_correct = 0;
    int m_wrong = 0;
};

} // namespace core
```

- [ ] **Step 5: Write src/core/SquareColorRound.cpp**

```cpp
#include "SquareColorRound.h"

#include <algorithm>
#include <cassert>

namespace core {

using std::chrono::duration_cast;
using std::chrono::milliseconds;

SquareColorRound::SquareColorRound(const IClock &clock,
                                   PromptGenerator generator,
                                   milliseconds roundLength)
    : m_clock(clock)
    , m_generator(std::move(generator))
    , m_roundLength(roundLength)
{
}

void SquareColorRound::start()
{
    m_roundStart = m_clock.now();
    m_state = RoundState::Running;
    m_answers.clear();
    m_correct = 0;
    m_wrong = 0;
    showNextPrompt();
}

void SquareColorRound::abort()
{
    if (m_state == RoundState::Running) {
        m_state = RoundState::Aborted;
    }
}

Square SquareColorRound::currentPrompt() const
{
    assert(m_state == RoundState::Running);
    assert(m_currentPrompt.has_value());
    return *m_currentPrompt;
}

milliseconds SquareColorRound::remaining() const
{
    switch (m_state) {
    case RoundState::Idle:
        return m_roundLength;
    case RoundState::Finished:
    case RoundState::Aborted:
        return milliseconds{0};
    case RoundState::Running:
        break;
    }

    const auto elapsed = duration_cast<milliseconds>(m_clock.now() - m_roundStart);
    return std::max(milliseconds{0}, m_roundLength - elapsed);
}

bool SquareColorRound::expired() const
{
    const auto elapsed = duration_cast<milliseconds>(m_clock.now() - m_roundStart);
    return elapsed >= m_roundLength;
}

bool SquareColorRound::tick()
{
    if (m_state != RoundState::Running || !expired()) {
        return false;
    }

    m_state = RoundState::Finished;
    return true;
}

void SquareColorRound::showNextPrompt()
{
    m_currentPrompt = m_generator.next();
    m_promptShownAt = m_clock.now();
}

AnswerOutcome SquareColorRound::answer(bool answeredDark)
{
    if (m_state != RoundState::Running) {
        return AnswerOutcome::Ignored;
    }

    // The clock may have run out since the last tick. An answer arriving after
    // expiry belongs to a round that is already over.
    if (expired()) {
        m_state = RoundState::Finished;
        return AnswerOutcome::Ignored;
    }

    const Square square = *m_currentPrompt;
    const bool expectedDark = square.isDark();
    const bool correct = answeredDark == expectedDark;

    m_answers.push_back(Answer{
        .ordinal = static_cast<int>(m_answers.size()) + 1,
        .square = square,
        .expectedDark = expectedDark,
        .answeredDark = answeredDark,
        .correct = correct,
        .responseTime = duration_cast<milliseconds>(m_clock.now() - m_promptShownAt),
    });

    if (correct) {
        ++m_correct;
    } else {
        ++m_wrong;
    }

    showNextPrompt();
    return correct ? AnswerOutcome::Correct : AnswerOutcome::Wrong;
}

} // namespace core
```

- [ ] **Step 6: Add SquareColorRound.cpp to the core target**

```cmake
add_library(chesstrainer-core STATIC
    Square.cpp
    PromptGenerator.cpp
    SquareColorRound.cpp
)
```

- [ ] **Step 7: Run the tests to verify they pass**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: all three test executables PASS, `tst_round` reporting 10 passing functions.

- [ ] **Step 8: Commit**

```bash
git add src/core tests
git commit --no-gpg-sign -m "feat: add the square color round state machine

Holds every timing and scoring rule for the drill: prompt advance, response
time measurement from prompt-shown rather than round-start, and the four
terminal-state guards.

tick() returns true only on the transition into Finished, so the 16 ms UI
timer cannot fire the end-of-round handler more than once. answer() rejects
input once the clock has expired, which is what keeps a late keypress from
scoring against a round that is already over.

Driven by FakeClock, so a full 30-second round runs in microseconds with no
timing flake."
```

---

### Task 5: Module registry

Small, and needed before the home screen can list anything.

**Files:**
- Create: `src/core/ModuleDescriptor.h`
- Create: `src/core/ModuleRegistry.h`
- Create: `src/core/ModuleRegistry.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/tst_moduleregistry.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces:
  - `struct core::ModuleDescriptor { std::string id; std::string displayName; std::string description; std::string iconName; std::string qmlPage; bool enabled; }`
  - `const std::vector<core::ModuleDescriptor> &core::moduleRegistry()`
  - `constexpr std::string_view core::kSquareColorModuleId = "square-color"`

- [ ] **Step 1: Write the failing test**

`tests/tst_moduleregistry.cpp`:

```cpp
#include <QtTest>

#include <algorithm>
#include <set>

#include "ModuleRegistry.h"

class TestModuleRegistry : public QObject
{
    Q_OBJECT

private slots:
    void containsTheSquareColorModuleAsEnabled();
    void identifiersAreUnique();
    void everyEnabledModuleHasAPage();
    void everyModuleHasNameAndDescription();
};

void TestModuleRegistry::containsTheSquareColorModuleAsEnabled()
{
    const auto &modules = core::moduleRegistry();

    const auto found = std::find_if(
        modules.begin(), modules.end(), [](const core::ModuleDescriptor &module) {
            return module.id == core::kSquareColorModuleId;
        });

    QVERIFY(found != modules.end());
    QVERIFY(found->enabled);
}

void TestModuleRegistry::identifiersAreUnique()
{
    std::set<std::string> seen;
    for (const auto &module : core::moduleRegistry()) {
        QVERIFY2(seen.insert(module.id).second,
                 qPrintable(QStringLiteral("duplicate module id: %1")
                                .arg(QString::fromStdString(module.id))));
    }
}

void TestModuleRegistry::everyEnabledModuleHasAPage()
{
    for (const auto &module : core::moduleRegistry()) {
        if (module.enabled) {
            QVERIFY2(!module.qmlPage.empty(),
                     qPrintable(QStringLiteral("enabled module %1 has no page")
                                    .arg(QString::fromStdString(module.id))));
        }
    }
}

void TestModuleRegistry::everyModuleHasNameAndDescription()
{
    for (const auto &module : core::moduleRegistry()) {
        QVERIFY(!module.displayName.empty());
        QVERIFY(!module.description.empty());
        QVERIFY(!module.iconName.empty());
    }
}

QTEST_APPLESS_MAIN(TestModuleRegistry)
#include "tst_moduleregistry.moc"
```

- [ ] **Step 2: Register the test**

```cmake
set(CORE_TESTS
    square
    promptgenerator
    round
    moduleregistry
)
```

- [ ] **Step 3: Run the test to verify it fails**

```bash
cmake --build build
```

Expected: build FAILS with `fatal error: ModuleRegistry.h: No such file or directory`.

- [ ] **Step 4: Write src/core/ModuleDescriptor.h**

```cpp
#pragma once

#include <string>

namespace core {

/// Everything the home screen needs to render one training module, and
/// everything the shell needs to open it.
///
/// There is deliberately no abstract TrainingModule base class: module two
/// needs exactly a row on the home list and a page of its own, and an
/// interface designed against a single implementation would guess wrong about
/// what modules actually share.
struct ModuleDescriptor {
    std::string id;
    std::string displayName;
    std::string description;
    std::string iconName; ///< freedesktop icon name
    std::string qmlPage;  ///< empty for modules that are not enabled yet
    bool enabled = false;
};

} // namespace core
```

- [ ] **Step 5: Write src/core/ModuleRegistry.h**

```cpp
#pragma once

#include <string_view>
#include <vector>

#include "ModuleDescriptor.h"

namespace core {

inline constexpr std::string_view kSquareColorModuleId = "square-color";

/// The modules this build knows about, in display order.
const std::vector<ModuleDescriptor> &moduleRegistry();

} // namespace core
```

- [ ] **Step 6: Write src/core/ModuleRegistry.cpp**

Descriptions here are English source strings; the app layer passes them through `i18n()` when building the list model.

```cpp
#include "ModuleRegistry.h"

namespace core {

const std::vector<ModuleDescriptor> &moduleRegistry()
{
    static const std::vector<ModuleDescriptor> modules = {
        ModuleDescriptor{
            .id = std::string(kSquareColorModuleId),
            .displayName = "Square Color",
            .description = "Name a square - is it light or dark? 30 second sprint.",
            .iconName = "games-config-board",
            .qmlPage = "qrc:/qt/qml/ChessTrainer/qml/DrillPage.qml",
            .enabled = true,
        },
        ModuleDescriptor{
            .id = "coordinates",
            .displayName = "Coordinates",
            .description = "Find the named square on a blank board.",
            .iconName = "grid-rectangular",
            .qmlPage = {},
            .enabled = false,
        },
        ModuleDescriptor{
            .id = "knight-path",
            .displayName = "Knight Path",
            .description = "Shortest knight route between two squares.",
            .iconName = "path-mode-polyline",
            .qmlPage = {},
            .enabled = false,
        },
        ModuleDescriptor{
            .id = "board-vision",
            .displayName = "Board Vision",
            .description = "Recognise legal moves at a glance.",
            .iconName = "view-visible",
            .qmlPage = {},
            .enabled = false,
        },
    };

    return modules;
}

} // namespace core
```

The `qrc:` path matches the QML module URI `ChessTrainer` declared in Task 8. `qt_add_qml_module` maps URI `ChessTrainer` plus the file's path relative to `src/app/` onto `qrc:/qt/qml/ChessTrainer/qml/DrillPage.qml`. If that URI changes, this string changes with it.

- [ ] **Step 7: Add ModuleRegistry.cpp to the core target**

```cmake
add_library(chesstrainer-core STATIC
    Square.cpp
    PromptGenerator.cpp
    SquareColorRound.cpp
    ModuleRegistry.cpp
)
```

- [ ] **Step 8: Run the tests to verify they pass**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: four test executables PASS.

- [ ] **Step 9: Commit**

```bash
git add src/core tests
git commit --no-gpg-sign -m "feat: add the training module registry

One descriptor per module, carrying what the home screen renders and what the
shell needs to open it. Three placeholder modules ship disabled so the home
list can be judged at realistic length.

No abstract TrainingModule base class: module two needs a list row and a page,
both of which a descriptor already covers. The shared interface gets
introduced when a second implementation shows what is actually common."
```

---

### Task 6: Database connection and migration ladder

**Files:**
- Create: `src/store/CMakeLists.txt`
- Create: `src/store/Database.h`
- Create: `src/store/Database.cpp`
- Modify: `CMakeLists.txt` (add `add_subdirectory(src/store)`)
- Modify: `tests/CMakeLists.txt`
- Test: `tests/tst_database.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: CMake target `chesstrainer-store` linking `Qt6::Sql` and `chesstrainer-core`. `store::Database` with `bool open(const QString &path, QString *errorOut)`, `bool migrate(QString *errorOut)`, `int schemaVersion() const`, `QSqlDatabase handle() const`, `bool isOpen() const`, and `static constexpr int kCurrentSchemaVersion = 1`.

- [ ] **Step 1: Write the failing test**

`tests/tst_database.cpp`:

```cpp
#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>

#include "Database.h"

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void migratesAnEmptyDatabaseToVersionOne();
    void migrationIsIdempotent();
    void createsBothTables();
    void enforcesForeignKeys();
    void reportsAnErrorForAnUnwritablePath();
    void independentInstancesDoNotShareAConnection();
};

void TestDatabase::migratesAnEmptyDatabaseToVersionOne()
{
    store::Database database;
    QString error;

    QVERIFY2(database.open(QStringLiteral(":memory:"), &error), qPrintable(error));
    QCOMPARE(database.schemaVersion(), 0);

    QVERIFY2(database.migrate(&error), qPrintable(error));
    QCOMPARE(database.schemaVersion(), store::Database::kCurrentSchemaVersion);
}

void TestDatabase::migrationIsIdempotent()
{
    store::Database database;
    QString error;
    QVERIFY(database.open(QStringLiteral(":memory:"), &error));

    QVERIFY2(database.migrate(&error), qPrintable(error));
    QVERIFY2(database.migrate(&error), qPrintable(error));
    QCOMPARE(database.schemaVersion(), 1);
}

void TestDatabase::createsBothTables()
{
    store::Database database;
    QString error;
    QVERIFY(database.open(QStringLiteral(":memory:"), &error));
    QVERIFY(database.migrate(&error));

    QSqlQuery query(database.handle());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name")));

    QStringList tables;
    while (query.next()) {
        tables << query.value(0).toString();
    }

    QVERIFY(tables.contains(QStringLiteral("run")));
    QVERIFY(tables.contains(QStringLiteral("answer")));
}

void TestDatabase::enforcesForeignKeys()
{
    store::Database database;
    QString error;
    QVERIFY(database.open(QStringLiteral(":memory:"), &error));
    QVERIFY(database.migrate(&error));

    // Without PRAGMA foreign_keys = ON, SQLite silently accepts this row.
    QSqlQuery query(database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO answer (run_id, ordinal, square, expected_dark, "
        "answered_dark, correct, response_ms) "
        "VALUES (9999, 1, 'e4', 1, 1, 1, 500)"));
    QVERIFY2(!query.exec(), "insert with a dangling run_id should be rejected");
}

void TestDatabase::reportsAnErrorForAnUnwritablePath()
{
    store::Database database;
    QString error;

    QVERIFY(!database.open(QStringLiteral("/proc/definitely/not/writable.db"),
                           &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!database.isOpen());
}

void TestDatabase::independentInstancesDoNotShareAConnection()
{
    store::Database first;
    store::Database second;
    QString error;

    QVERIFY(first.open(QStringLiteral(":memory:"), &error));
    QVERIFY(second.open(QStringLiteral(":memory:"), &error));

    QVERIFY(first.migrate(&error));
    // second is a distinct in-memory database, so it is still unmigrated.
    QCOMPARE(second.schemaVersion(), 0);
}

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
```

`QTEST_GUILESS_MAIN`, not `QTEST_APPLESS_MAIN`: `QSqlDatabase` needs a `QCoreApplication` for its driver plugin loading.

- [ ] **Step 2: Register the test**

`tests/CMakeLists.txt` gains a second list, because store tests need one more library:

```cmake
set(CORE_TESTS
    square
    promptgenerator
    round
    moduleregistry
)

foreach(test_name IN LISTS CORE_TESTS)
    add_executable(tst_${test_name} tst_${test_name}.cpp)
    target_link_libraries(tst_${test_name} PRIVATE
        Qt6::Test
        chesstrainer-core)
    add_test(NAME tst_${test_name} COMMAND tst_${test_name})
endforeach()

set(STORE_TESTS
    database
)

foreach(test_name IN LISTS STORE_TESTS)
    add_executable(tst_${test_name} tst_${test_name}.cpp)
    target_link_libraries(tst_${test_name} PRIVATE
        Qt6::Test
        chesstrainer-core
        chesstrainer-store)
    add_test(NAME tst_${test_name} COMMAND tst_${test_name})
endforeach()
```

- [ ] **Step 3: Add the store subdirectory**

In the top-level `CMakeLists.txt`, between the core and tests lines:

```cmake
add_subdirectory(src/core)
add_subdirectory(src/store)
add_subdirectory(tests)
```

- [ ] **Step 4: Run the test to verify it fails**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build
```

Expected: configuration FAILS with `add_subdirectory given source "src/store" which is not an existing directory`.

- [ ] **Step 5: Write src/store/CMakeLists.txt**

```cmake
add_library(chesstrainer-store STATIC
    Database.cpp
)

target_include_directories(chesstrainer-store
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(chesstrainer-store
    PUBLIC
        Qt6::Core
        Qt6::Sql
        chesstrainer-core)

set_target_properties(chesstrainer-store PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
```

- [ ] **Step 6: Write src/store/Database.h**

```cpp
#pragma once

#include <QSqlDatabase>
#include <QString>

namespace store {

/// Owns one SQLite connection and applies the schema migration ladder.
///
/// Each instance takes a unique Qt connection name, so several databases can
/// be open at once - which is what lets tests use independent ":memory:"
/// databases without colliding.
class Database
{
public:
    static constexpr int kCurrentSchemaVersion = 1;

    Database();
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    /// Opens the database, creating parent directories as needed, and sets
    /// PRAGMA foreign_keys = ON. Pass ":memory:" for a transient database.
    bool open(const QString &path, QString *errorOut);

    bool isOpen() const;

    /// Applies every pending migration. Safe to call repeatedly.
    bool migrate(QString *errorOut);

    /// Reads PRAGMA user_version. Zero means an empty, unmigrated database.
    int schemaVersion() const;

    QSqlDatabase handle() const;

private:
    bool applyVersion1(QString *errorOut);
    bool setSchemaVersion(int version, QString *errorOut);

    QString m_connectionName;
};

} // namespace store
```

- [ ] **Step 7: Write src/store/Database.cpp**

```cpp
#include "Database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace store {

namespace {

/// Assigns the error message if the caller asked for one, and always returns
/// false so call sites can `return fail(...)`.
bool fail(QString *errorOut, const QString &message)
{
    if (errorOut) {
        *errorOut = message;
    }
    return false;
}

} // namespace

Database::Database()
    : m_connectionName(QStringLiteral("chess-trainer-")
                       + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Database::~Database()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        // The QSqlDatabase copy must be out of scope before removeDatabase,
        // otherwise Qt warns about a connection still in use.
        {
            QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
            if (database.isOpen()) {
                database.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Database::open(const QString &path, QString *errorOut)
{
    if (path != QStringLiteral(":memory:")) {
        const QDir parent = QFileInfo(path).dir();
        if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
            return fail(errorOut,
                        QStringLiteral("Cannot create directory %1")
                            .arg(parent.absolutePath()));
        }
    }

    QSqlDatabase database =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(path);

    if (!database.open()) {
        const QString message = database.lastError().text();
        QSqlDatabase::removeDatabase(m_connectionName);
        return fail(errorOut, message);
    }

    // SQLite ignores REFERENCES clauses unless this is enabled, per connection.
    QSqlQuery pragma(database);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        return fail(errorOut, pragma.lastError().text());
    }

    return true;
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName)
        && QSqlDatabase::database(m_connectionName, false).isOpen();
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

int Database::schemaVersion() const
{
    QSqlQuery query(handle());
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool Database::setSchemaVersion(int version, QString *errorOut)
{
    // PRAGMA does not accept bound parameters, so the value is interpolated.
    // It is an int from our own constant, never external input.
    QSqlQuery query(handle());
    if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version))) {
        return fail(errorOut, query.lastError().text());
    }
    return true;
}

bool Database::migrate(QString *errorOut)
{
    if (!isOpen()) {
        return fail(errorOut, QStringLiteral("Database is not open"));
    }

    if (schemaVersion() >= kCurrentSchemaVersion) {
        return true;
    }

    if (schemaVersion() < 1 && !applyVersion1(errorOut)) {
        return false;
    }

    return true;
}

bool Database::applyVersion1(QString *errorOut)
{
    QSqlDatabase database = handle();
    if (!database.transaction()) {
        return fail(errorOut, database.lastError().text());
    }

    const QStringList statements = {
        QStringLiteral("CREATE TABLE run ("
                       "  id              INTEGER PRIMARY KEY,"
                       "  module_id       TEXT    NOT NULL,"
                       "  started_at      TEXT    NOT NULL,"
                       "  round_length_ms INTEGER NOT NULL,"
                       "  correct         INTEGER NOT NULL,"
                       "  wrong           INTEGER NOT NULL,"
                       "  completed       INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE answer ("
                       "  id            INTEGER PRIMARY KEY,"
                       "  run_id        INTEGER NOT NULL"
                       "                REFERENCES run(id) ON DELETE CASCADE,"
                       "  ordinal       INTEGER NOT NULL,"
                       "  square        TEXT    NOT NULL,"
                       "  expected_dark INTEGER NOT NULL,"
                       "  answered_dark INTEGER NOT NULL,"
                       "  correct       INTEGER NOT NULL,"
                       "  response_ms   INTEGER NOT NULL)"),
        QStringLiteral("CREATE INDEX idx_answer_run ON answer(run_id)"),
        QStringLiteral("CREATE INDEX idx_answer_square ON answer(square)"),
    };

    for (const QString &statement : statements) {
        QSqlQuery query(database);
        if (!query.exec(statement)) {
            const QString message = query.lastError().text();
            database.rollback();
            return fail(errorOut, message);
        }
    }

    if (!database.commit()) {
        return fail(errorOut, database.lastError().text());
    }

    return setSchemaVersion(1, errorOut);
}

} // namespace store
```

`PRAGMA user_version` cannot be set inside a transaction that is rolled back cleanly on some SQLite builds, so it is written after the commit — if the process dies between the two, the next `migrate()` sees version 0 and re-runs `CREATE TABLE`, which fails and reports an error rather than corrupting anything. Task 12 handles that case by moving the file aside.

- [ ] **Step 8: Run the tests to verify they pass**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: five test executables PASS, `tst_database` reporting 6 passing functions.

- [ ] **Step 9: Commit**

```bash
git add CMakeLists.txt src/store tests
git commit --no-gpg-sign -m "feat: add SQLite connection and schema migration

Schema version 1 creates the run and answer tables with the indexes the stats
queries need. The ladder is driven by PRAGMA user_version so module two can
extend the schema without a data reset.

Each Database instance takes a UUID connection name, which is what lets tests
open several independent :memory: databases at once. PRAGMA foreign_keys is
set per connection, without which SQLite silently accepts answer rows with a
dangling run_id - covered by a test that asserts the insert is rejected."
```

---

### Task 7: Run repository

**Files:**
- Create: `src/store/RunRepository.h`
- Create: `src/store/RunRepository.cpp`
- Modify: `src/store/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/tst_repository.cpp`

**Interfaces:**
- Consumes: `store::Database` from Task 6; `core::Answer` and `core::Square` from Tasks 1 and 4.
- Produces:
  - `struct store::RunRecord { QString moduleId; QDateTime startedAtUtc; int roundLengthMs; int correct; int wrong; bool completed; std::vector<core::Answer> answers; }`
  - `struct store::ModuleSummary { std::optional<int> bestScore; std::optional<int> meanResponseMs; }`
  - `store::RunRepository` with `explicit RunRepository(Database &)`, `std::optional<qint64> writeRun(const RunRecord &, QString *errorOut)`, `std::optional<ModuleSummary> moduleSummary(const QString &moduleId, QString *errorOut)`.

- [ ] **Step 1: Write the failing test**

`tests/tst_repository.cpp`:

```cpp
#include <QtTest>

#include <QSqlQuery>

#include "Database.h"
#include "RunRepository.h"

using namespace std::chrono_literals;

namespace {

core::Answer makeAnswer(int ordinal,
                        const char *square,
                        bool correct,
                        std::chrono::milliseconds responseTime)
{
    const auto parsed = core::Square::fromAlgebraic(square);
    Q_ASSERT(parsed.has_value());

    core::Answer answer;
    answer.ordinal = ordinal;
    answer.square = *parsed;
    answer.expectedDark = parsed->isDark();
    answer.answeredDark = correct ? parsed->isDark() : !parsed->isDark();
    answer.correct = correct;
    answer.responseTime = responseTime;
    return answer;
}

store::RunRecord makeRun(int correct, int wrong, bool completed,
                         std::vector<core::Answer> answers)
{
    store::RunRecord record;
    record.moduleId = QStringLiteral("square-color");
    record.startedAtUtc = QDateTime(QDate(2026, 9, 9), QTime(8, 0), QTimeZone::UTC);
    record.roundLengthMs = 30'000;
    record.correct = correct;
    record.wrong = wrong;
    record.completed = completed;
    record.answers = std::move(answers);
    return record;
}

} // namespace

class TestRepository : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void writeRunReturnsTheNewRunId();
    void writeRunPreservesEveryAnswerField();
    void moduleSummaryIsEmptyOnAFreshDatabase();
    void moduleSummaryReportsBestScoreAndMeanResponseTime();
    void moduleSummaryIgnoresAbortedRuns();
    void moduleSummaryIsScopedToOneModule();
    void deletingARunCascadesToItsAnswers();

private:
    std::unique_ptr<store::Database> m_database;
    std::unique_ptr<store::RunRepository> m_repository;
};

void TestRepository::init()
{
    m_database = std::make_unique<store::Database>();
    QString error;
    QVERIFY2(m_database->open(QStringLiteral(":memory:"), &error), qPrintable(error));
    QVERIFY2(m_database->migrate(&error), qPrintable(error));
    m_repository = std::make_unique<store::RunRepository>(*m_database);
}

void TestRepository::writeRunReturnsTheNewRunId()
{
    QString error;
    const auto first = m_repository->writeRun(
        makeRun(1, 0, true, {makeAnswer(1, "e4", true, 500ms)}), &error);
    QVERIFY2(first.has_value(), qPrintable(error));

    const auto second = m_repository->writeRun(
        makeRun(1, 0, true, {makeAnswer(1, "d5", true, 600ms)}), &error);
    QVERIFY2(second.has_value(), qPrintable(error));

    QVERIFY(*second > *first);
}

void TestRepository::writeRunPreservesEveryAnswerField()
{
    QString error;
    const auto runId = m_repository->writeRun(
        makeRun(1, 1, true,
                {makeAnswer(1, "e4", true, 480ms),
                 makeAnswer(2, "h6", false, 1'320ms)}),
        &error);
    QVERIFY2(runId.has_value(), qPrintable(error));

    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "SELECT ordinal, square, expected_dark, answered_dark, correct, "
        "response_ms FROM answer WHERE run_id = ? ORDER BY ordinal"));
    query.addBindValue(*runId);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QCOMPARE(query.value(1).toString(), QStringLiteral("e4"));
    QCOMPARE(query.value(2).toBool(), false); // e4 is light
    QCOMPARE(query.value(3).toBool(), false);
    QCOMPARE(query.value(4).toBool(), true);
    QCOMPARE(query.value(5).toInt(), 480);

    QVERIFY(query.next());
    QCOMPARE(query.value(1).toString(), QStringLiteral("h6"));
    QCOMPARE(query.value(4).toBool(), false);
    QCOMPARE(query.value(5).toInt(), 1'320);

    QVERIFY(!query.next());
}

void TestRepository::moduleSummaryIsEmptyOnAFreshDatabase()
{
    QString error;
    const auto summary =
        m_repository->moduleSummary(QStringLiteral("square-color"), &error);

    QVERIFY2(summary.has_value(), qPrintable(error));
    QVERIFY(!summary->bestScore.has_value());
    QVERIFY(!summary->meanResponseMs.has_value());
}

void TestRepository::moduleSummaryReportsBestScoreAndMeanResponseTime()
{
    QString error;
    QVERIFY(m_repository->writeRun(
        makeRun(12, 1, true,
                {makeAnswer(1, "e4", true, 400ms),
                 makeAnswer(2, "d5", true, 600ms)}),
        &error));
    QVERIFY(m_repository->writeRun(
        makeRun(26, 2, true,
                {makeAnswer(1, "a1", true, 800ms),
                 makeAnswer(2, "h8", false, 1'200ms)}),
        &error));

    const auto summary =
        m_repository->moduleSummary(QStringLiteral("square-color"), &error);
    QVERIFY2(summary.has_value(), qPrintable(error));

    QCOMPARE(*summary->bestScore, 26);
    // Mean over every answer in completed runs, correct and wrong alike:
    // (400 + 600 + 800 + 1200) / 4
    QCOMPARE(*summary->meanResponseMs, 750);
}

void TestRepository::moduleSummaryIgnoresAbortedRuns()
{
    QString error;
    QVERIFY(m_repository->writeRun(
        makeRun(10, 0, true, {makeAnswer(1, "e4", true, 500ms)}), &error));
    // An aborted run with a higher score must not become the record.
    QVERIFY(m_repository->writeRun(
        makeRun(99, 0, false, {makeAnswer(1, "d5", true, 100ms)}), &error));

    const auto summary =
        m_repository->moduleSummary(QStringLiteral("square-color"), &error);
    QVERIFY(summary.has_value());
    QCOMPARE(*summary->bestScore, 10);
    QCOMPARE(*summary->meanResponseMs, 500);
}

void TestRepository::moduleSummaryIsScopedToOneModule()
{
    QString error;
    QVERIFY(m_repository->writeRun(
        makeRun(10, 0, true, {makeAnswer(1, "e4", true, 500ms)}), &error));

    auto other = makeRun(50, 0, true, {makeAnswer(1, "d5", true, 100ms)});
    other.moduleId = QStringLiteral("coordinates");
    QVERIFY(m_repository->writeRun(other, &error));

    const auto summary =
        m_repository->moduleSummary(QStringLiteral("square-color"), &error);
    QVERIFY(summary.has_value());
    QCOMPARE(*summary->bestScore, 10);
}

void TestRepository::deletingARunCascadesToItsAnswers()
{
    QString error;
    const auto runId = m_repository->writeRun(
        makeRun(1, 0, true, {makeAnswer(1, "e4", true, 500ms)}), &error);
    QVERIFY(runId.has_value());

    QSqlQuery remove(m_database->handle());
    remove.prepare(QStringLiteral("DELETE FROM run WHERE id = ?"));
    remove.addBindValue(*runId);
    QVERIFY2(remove.exec(), qPrintable(remove.lastError().text()));

    QSqlQuery count(m_database->handle());
    QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM answer")));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 0);
}

QTEST_GUILESS_MAIN(TestRepository)
#include "tst_repository.moc"
```

The `init()` slot runs before every test function, so each one gets a fresh migrated database.

- [ ] **Step 2: Register the test**

```cmake
set(STORE_TESTS
    database
    repository
)
```

- [ ] **Step 3: Run the test to verify it fails**

```bash
cmake --build build
```

Expected: build FAILS with `fatal error: RunRepository.h: No such file or directory`.

- [ ] **Step 4: Write src/store/RunRepository.h**

```cpp
#pragma once

#include <optional>
#include <vector>

#include <QDateTime>
#include <QString>

#include "SquareColorRound.h"

namespace store {

class Database;

/// One completed or aborted round, ready to persist.
struct RunRecord {
    QString moduleId;
    QDateTime startedAtUtc;
    int roundLengthMs = 0;
    int correct = 0;
    int wrong = 0;
    bool completed = false;
    std::vector<core::Answer> answers;
};

/// Aggregates over a module's completed runs. Both fields are nullopt until
/// at least one run has been completed.
struct ModuleSummary {
    std::optional<int> bestScore;
    std::optional<int> meanResponseMs;
};

class RunRepository
{
public:
    explicit RunRepository(Database &database);

    /// Writes the run and all its answers in a single transaction. Returns the
    /// new run id, or nullopt with errorOut set.
    std::optional<qint64> writeRun(const RunRecord &record, QString *errorOut);

    /// Best score and mean response time across the module's completed runs.
    std::optional<ModuleSummary> moduleSummary(const QString &moduleId,
                                               QString *errorOut);

private:
    Database &m_database;
};

} // namespace store
```

- [ ] **Step 5: Write src/store/RunRepository.cpp**

```cpp
#include "RunRepository.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "Database.h"

namespace store {

namespace {

const QString kIsoFormat = QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzzZ");

} // namespace

RunRepository::RunRepository(Database &database)
    : m_database(database)
{
}

std::optional<qint64> RunRepository::writeRun(const RunRecord &record,
                                              QString *errorOut)
{
    QSqlDatabase database = m_database.handle();

    if (!database.transaction()) {
        if (errorOut) {
            *errorOut = database.lastError().text();
        }
        return std::nullopt;
    }

    const auto rollback = [&](const QString &message) -> std::optional<qint64> {
        database.rollback();
        if (errorOut) {
            *errorOut = message;
        }
        return std::nullopt;
    };

    QSqlQuery insertRun(database);
    insertRun.prepare(QStringLiteral(
        "INSERT INTO run (module_id, started_at, round_length_ms, correct, "
        "wrong, completed) VALUES (?, ?, ?, ?, ?, ?)"));
    insertRun.addBindValue(record.moduleId);
    insertRun.addBindValue(record.startedAtUtc.toUTC().toString(kIsoFormat));
    insertRun.addBindValue(record.roundLengthMs);
    insertRun.addBindValue(record.correct);
    insertRun.addBindValue(record.wrong);
    insertRun.addBindValue(record.completed ? 1 : 0);

    if (!insertRun.exec()) {
        return rollback(insertRun.lastError().text());
    }

    const qint64 runId = insertRun.lastInsertId().toLongLong();

    QSqlQuery insertAnswer(database);
    insertAnswer.prepare(QStringLiteral(
        "INSERT INTO answer (run_id, ordinal, square, expected_dark, "
        "answered_dark, correct, response_ms) VALUES (?, ?, ?, ?, ?, ?, ?)"));

    for (const core::Answer &answer : record.answers) {
        insertAnswer.bindValue(0, runId);
        insertAnswer.bindValue(1, answer.ordinal);
        insertAnswer.bindValue(
            2, QString::fromStdString(answer.square.algebraic()));
        insertAnswer.bindValue(3, answer.expectedDark ? 1 : 0);
        insertAnswer.bindValue(4, answer.answeredDark ? 1 : 0);
        insertAnswer.bindValue(5, answer.correct ? 1 : 0);
        insertAnswer.bindValue(
            6, static_cast<int>(answer.responseTime.count()));

        if (!insertAnswer.exec()) {
            return rollback(insertAnswer.lastError().text());
        }
    }

    if (!database.commit()) {
        return rollback(database.lastError().text());
    }

    return runId;
}

std::optional<ModuleSummary> RunRepository::moduleSummary(const QString &moduleId,
                                                          QString *errorOut)
{
    QSqlDatabase database = m_database.handle();
    ModuleSummary summary;

    QSqlQuery best(database);
    best.prepare(QStringLiteral(
        "SELECT MAX(correct) FROM run WHERE module_id = ? AND completed = 1"));
    best.addBindValue(moduleId);

    if (!best.exec() || !best.next()) {
        if (errorOut) {
            *errorOut = best.lastError().text();
        }
        return std::nullopt;
    }
    // MAX over no rows yields SQL NULL, which is a null QVariant.
    if (!best.value(0).isNull()) {
        summary.bestScore = best.value(0).toInt();
    }

    QSqlQuery mean(database);
    mean.prepare(QStringLiteral(
        "SELECT AVG(a.response_ms) FROM answer a "
        "JOIN run r ON a.run_id = r.id "
        "WHERE r.module_id = ? AND r.completed = 1"));
    mean.addBindValue(moduleId);

    if (!mean.exec() || !mean.next()) {
        if (errorOut) {
            *errorOut = mean.lastError().text();
        }
        return std::nullopt;
    }
    if (!mean.value(0).isNull()) {
        summary.meanResponseMs = static_cast<int>(qRound(mean.value(0).toDouble()));
    }

    return summary;
}

} // namespace store
```

- [ ] **Step 6: Add RunRepository.cpp to the store target**

```cmake
add_library(chesstrainer-store STATIC
    Database.cpp
    RunRepository.cpp
)
```

- [ ] **Step 7: Run the tests to verify they pass**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: six test executables PASS, `tst_repository` reporting 7 passing functions.

- [ ] **Step 8: Commit**

```bash
git add src/store tests
git commit --no-gpg-sign -m "feat: add run repository with single-transaction writes

writeRun persists a round and all of its answers in one transaction, rolling
back on any failure so a partial round never lands. The controller calls it
once at round end rather than per answer, so no fsync stall can occur while
the user is being timed.

moduleSummary reports best score and mean response time over completed runs
only, so an aborted round cannot become a personal record. Both values are
nullopt before the first completed run, because MAX and AVG over no rows
return SQL NULL rather than zero - which would otherwise render as a real
score of 0 on the home card."
```

---

### Task 8: Application shell

First task with a visible deliverable: a Kirigami window that opens on Wayland from inside the toolbox. No drill yet.

There are no automated tests here by design (spec §8) — QML holds bindings only, and `qmltestrunner` would need a display. Verification is manual and specified exactly.

**Files:**
- Create: `src/app/CMakeLists.txt`
- Create: `src/app/main.cpp`
- Create: `src/app/qml/main.qml`
- Create: `src/app/qml/HomePage.qml` (placeholder; filled in by Task 9)
- Modify: `CMakeLists.txt` (add `add_subdirectory(src/app)`)

**Interfaces:**
- Consumes: `chesstrainer-core` and `chesstrainer-store` targets.
- Produces: the `chess-trainer` executable; QML module URI `ChessTrainer` version 1.0; a `store::Database` and `store::RunRepository` owned by `main()`. Neither is exposed to QML — they are passed by pointer to the model in Task 9 and the controller in Task 11. QML sees only the context properties `storageReady` (bool) and `storageError` (QString), and `main.qml`'s `pageStack` for later tasks to push onto.

- [ ] **Step 1: Write src/app/CMakeLists.txt**

```cmake
qt_add_executable(chess-trainer
    main.cpp
)

qt_add_qml_module(chess-trainer
    URI ChessTrainer
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/HomePage.qml
)

target_link_libraries(chess-trainer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
    KF6::Kirigami
    KF6::CoreAddons
    KF6::I18n
    chesstrainer-core
    chesstrainer-store)

install(TARGETS chess-trainer ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
```

`QML_FILES` grows in Tasks 9, 11 and 12. `Qt6::Widgets` is required even though no widget is used: `qqc2-desktop-style` links against it.

- [ ] **Step 2: Write src/app/main.cpp**

```cpp
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>

#include <KAboutData>
#include <KLocalizedContext>
#include <KLocalizedString>

#include "Database.h"
#include "RunRepository.h"

int main(int argc, char *argv[])
{
    // QApplication, not QGuiApplication: qqc2-desktop-style links QtWidgets
    // and fails to load under a QGuiApplication.
    QApplication application(argc, argv);

    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("chess-trainer"));

    KAboutData about(QStringLiteral("io.github.martikan.ChessTrainer"),
                     i18n("Chess Trainer"),
                     QStringLiteral("0.1.0"),
                     i18n("Train your chess board vision"),
                     KAboutLicense::MIT,
                     i18n("© 2026 Richard Martikan"));
    about.addAuthor(i18n("Richard Martikan"),
                    i18n("Author"),
                    QStringLiteral("ric.martikan@gmail.com"));
    about.setHomepage(QStringLiteral("https://github.com/martikan/chess-trainer"));
    about.setBugAddress(
        QByteArrayLiteral("https://github.com/martikan/chess-trainer/issues"));
    KAboutData::setApplicationData(about);

    QApplication::setWindowIcon(
        QIcon::fromTheme(QStringLiteral("io.github.martikan.ChessTrainer")));

    // Storage failure must never block training, so an error here is carried
    // into the UI as a message rather than aborting startup. Task 13 renders
    // it; for now it only affects whether persistence works.
    const QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    store::Database database;
    QString storageError;
    const bool storageReady =
        database.open(dataDirectory + QStringLiteral("/trainer.db"), &storageError)
        && database.migrate(&storageError);

    store::RunRepository repository(database);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("storageReady"),
                                             storageReady);
    engine.rootContext()->setContextProperty(QStringLiteral("storageError"),
                                             storageError);

    engine.loadFromModule(QStringLiteral("ChessTrainer"), QStringLiteral("main"));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return QApplication::exec();
}
```

`AppDataLocation` resolves to `~/.local/share/chess-trainer/` because that is the `KAboutData` component name's application directory; `Database::open` creates it if missing.

- [ ] **Step 3: Write src/app/qml/main.qml**

```qml
import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Chess Trainer")

    minimumWidth: Kirigami.Units.gridUnit * 22
    minimumHeight: Kirigami.Units.gridUnit * 28
    width: Kirigami.Units.gridUnit * 30
    height: Kirigami.Units.gridUnit * 38

    globalDrawer: Kirigami.GlobalDrawer {
        isMenu: true

        actions: [
            Kirigami.Action {
                text: i18n("About Chess Trainer")
                icon.name: "help-about"
                onTriggered: root.pageStack.layers.push(aboutPage)
            },
            Kirigami.Action {
                text: i18n("Quit")
                icon.name: "application-exit"
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        ]
    }

    Component {
        id: aboutPage
        Kirigami.AboutPage {}
    }

    pageStack.initialPage: HomePage {}
}
```

`Kirigami.AboutPage` reads `KAboutData::applicationData()` with no wiring — that is why `main.cpp` sets it.

- [ ] **Step 4: Write the placeholder src/app/qml/HomePage.qml**

Task 9 replaces the body. It exists now so the window has something to show.

```qml
import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    title: i18n("Chess Trainer")

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - (Kirigami.Units.largeSpacing * 4)
        text: i18n("Training modules will appear here.")
    }
}
```

- [ ] **Step 5: Add the app subdirectory**

```cmake
add_subdirectory(src/core)
add_subdirectory(src/store)
add_subdirectory(src/app)
add_subdirectory(tests)
```

- [ ] **Step 6: Build and run it**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/chess-trainer
```

Expected, verified by eye:
1. A window opens titled "Chess Trainer".
2. Its colours match your current Plasma colour scheme — not a hardcoded palette. Switch Plasma between Breeze Light and Breeze Dark in System Settings and reopen; the window follows.
3. Controls look like Breeze, not like plain Qt Quick's default style. If they look flat and generic, `QQuickStyle::setStyle` did not take effect or `kf6-qqc2-desktop-style` is not installed.
4. The hamburger menu opens and "About Chess Trainer" shows version 0.1.0, the MIT licence, and the GitHub homepage.
5. No warnings on stderr about missing QML modules.

If the window does not appear at all, confirm the Wayland socket is visible from the container: `ls $XDG_RUNTIME_DIR/wayland-0`.

- [ ] **Step 7: Run the test suite to confirm nothing regressed**

```bash
ctest --test-dir build --output-on-failure
```

Expected: six test executables still PASS.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt src/app
git commit --no-gpg-sign -m "feat: add the Kirigami application shell

A Kirigami.ApplicationWindow with a global drawer and the standard KDE about
page, which reads KAboutData with no further wiring.

Uses QApplication rather than QGuiApplication because qqc2-desktop-style links
QtWidgets and silently fails to load without it, leaving the app with the
generic Qt Quick style instead of Breeze.

Storage is opened at startup but a failure does not abort: the result is passed
into QML as storageReady/storageError so training still works without
persistence. Task 13 renders the message."
```

---

### Task 9: Module list model and the home page

**Files:**
- Create: `src/app/ModuleListModel.h`
- Create: `src/app/ModuleListModel.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `src/app/main.cpp`
- Modify: `src/app/qml/HomePage.qml` (replace the placeholder body)

**Interfaces:**
- Consumes: `core::moduleRegistry()`, `core::ModuleDescriptor`, `store::RunRepository::moduleSummary()`.
- Produces: `app::ModuleListModel : QAbstractListModel` with roles `ModuleIdRole`, `DisplayNameRole`, `DescriptionRole`, `IconNameRole`, `QmlPageRole`, `EnabledRole`, `BestScoreRole`, `MeanResponseMsRole` (QML names `moduleId`, `displayName`, `description`, `iconName`, `qmlPage`, `enabled`, `bestScore`, `meanResponseMs`), plus `Q_INVOKABLE void refresh()`. Exposed to QML as the context property `moduleList`. `bestScore` and `meanResponseMs` are `-1` when no completed run exists, which QML tests with `>= 0`.

- [ ] **Step 1: Write src/app/ModuleListModel.h**

```cpp
#pragma once

#include <QAbstractListModel>

#include "ModuleDescriptor.h"

namespace store {
class RunRepository;
}

namespace app {

/// Presents core::moduleRegistry() to QML, joined with each module's
/// best-score and mean-response-time aggregates.
class ModuleListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ModuleIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        DescriptionRole,
        IconNameRole,
        QmlPageRole,
        EnabledRole,
        BestScoreRole,
        MeanResponseMsRole,
    };

    /// repository may be null when storage failed to open; the model then
    /// reports no statistics and every module still lists normally.
    explicit ModuleListModel(store::RunRepository *repository,
                             QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Re-reads the aggregates. Called after a round is written.
    Q_INVOKABLE void refresh();

private:
    struct Row {
        core::ModuleDescriptor descriptor;
        int bestScore = -1;
        int meanResponseMs = -1;
    };

    void loadStatistics();

    store::RunRepository *m_repository;
    std::vector<Row> m_rows;
};

} // namespace app
```

- [ ] **Step 2: Write src/app/ModuleListModel.cpp**

```cpp
#include "ModuleListModel.h"

#include <KLocalizedString>

#include "ModuleRegistry.h"
#include "RunRepository.h"

namespace app {

ModuleListModel::ModuleListModel(store::RunRepository *repository,
                                  QObject *parent)
    : QAbstractListModel(parent)
    , m_repository(repository)
{
    for (const core::ModuleDescriptor &descriptor : core::moduleRegistry()) {
        m_rows.push_back(Row{descriptor, -1, -1});
    }
    loadStatistics();
}

int ModuleListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant ModuleListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_rows.size())) {
        return {};
    }

    const Row &row = m_rows[static_cast<std::size_t>(index.row())];

    switch (role) {
    case ModuleIdRole:
        return QString::fromStdString(row.descriptor.id);
    case DisplayNameRole:
        // The registry holds English source strings; they become translatable
        // at this boundary.
        return i18n(row.descriptor.displayName.c_str());
    case DescriptionRole:
        return i18n(row.descriptor.description.c_str());
    case IconNameRole:
        return QString::fromStdString(row.descriptor.iconName);
    case QmlPageRole:
        return QString::fromStdString(row.descriptor.qmlPage);
    case EnabledRole:
        return row.descriptor.enabled;
    case BestScoreRole:
        return row.bestScore;
    case MeanResponseMsRole:
        return row.meanResponseMs;
    default:
        return {};
    }
}

QHash<int, QByteArray> ModuleListModel::roleNames() const
{
    return {
        {ModuleIdRole, QByteArrayLiteral("moduleId")},
        {DisplayNameRole, QByteArrayLiteral("displayName")},
        {DescriptionRole, QByteArrayLiteral("description")},
        {IconNameRole, QByteArrayLiteral("iconName")},
        {QmlPageRole, QByteArrayLiteral("qmlPage")},
        {EnabledRole, QByteArrayLiteral("enabled")},
        {BestScoreRole, QByteArrayLiteral("bestScore")},
        {MeanResponseMsRole, QByteArrayLiteral("meanResponseMs")},
    };
}

void ModuleListModel::loadStatistics()
{
    if (!m_repository) {
        return;
    }

    for (Row &row : m_rows) {
        QString error;
        const auto summary = m_repository->moduleSummary(
            QString::fromStdString(row.descriptor.id), &error);
        if (!summary) {
            continue;
        }
        row.bestScore = summary->bestScore.value_or(-1);
        row.meanResponseMs = summary->meanResponseMs.value_or(-1);
    }
}

void ModuleListModel::refresh()
{
    loadStatistics();
    if (!m_rows.empty()) {
        Q_EMIT dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1),
                           {BestScoreRole, MeanResponseMsRole});
    }
}

} // namespace app
```

- [ ] **Step 3: Add the model to the target and expose it**

In `src/app/CMakeLists.txt`:

```cmake
qt_add_executable(chess-trainer
    main.cpp
    ModuleListModel.h
    ModuleListModel.cpp
)
```

In `src/app/main.cpp`, add the include and create the model after the repository:

```cpp
#include "ModuleListModel.h"
```

```cpp
    store::RunRepository repository(database);
    app::ModuleListModel moduleList(storageReady ? &repository : nullptr);
```

and register it before `loadFromModule`:

```cpp
    engine.rootContext()->setContextProperty(QStringLiteral("moduleList"),
                                             &moduleList);
```

- [ ] **Step 4: Rewrite src/app/qml/HomePage.qml**

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page

    title: i18n("Chess Trainer")

    signal moduleRequested(string qmlPage)

    Kirigami.CardsListView {
        id: cards

        model: moduleList

        delegate: Kirigami.AbstractCard {
            required property string displayName
            required property string description
            required property string iconName
            required property string qmlPage
            required property bool enabled
            required property int bestScore
            required property int meanResponseMs

            // Disabled modules stay legible but visibly out of reach.
            opacity: enabled ? 1.0 : 0.45

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: iconName
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        level: 4
                        text: displayName
                        Layout.fillWidth: true
                    }

                    Controls.Label {
                        text: description
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        visible: enabled

                        Controls.Button {
                            text: i18n("Start")
                            icon.name: "media-playback-start"
                            onClicked: page.moduleRequested(qmlPage)
                        }

                        Controls.Label {
                            // bestScore is -1 until a round has been completed.
                            visible: bestScore >= 0
                            color: Kirigami.Theme.highlightColor
                            text: meanResponseMs >= 0
                                ? i18nc("best score and mean response time",
                                        "best %1 · avg %2 ms",
                                        bestScore, meanResponseMs)
                                : i18nc("best score", "best %1", bestScore)
                        }
                    }

                    Controls.Label {
                        visible: !enabled
                        text: i18nc("module not implemented yet", "Soon")
                        opacity: 0.7
                    }
                }
            }
        }
    }
}
```

- [ ] **Step 5: Wire the signal in main.qml**

Replace the `pageStack.initialPage` line with:

```qml
    pageStack.initialPage: HomePage {
        onModuleRequested: (qmlPage) => root.pageStack.push(qmlPage)
    }
```

- [ ] **Step 6: Build and run it**

```bash
cmake --build build && ./build/bin/chess-trainer
```

Expected, verified by eye:
1. Four cards: Square Color, Coordinates, Knight Path, Board Vision.
2. Only Square Color is fully opaque and has a Start button. The other three are dimmed and show "Soon".
3. No statistics line on any card yet, because no round has been completed.
4. Clicking Start produces a QML error on stderr about `DrillPage.qml` not existing — expected, that page arrives in Task 11.
5. Icons render. If any icon is a blank square, the freedesktop name in `ModuleRegistry.cpp` is not in your icon theme; substitute one that is and note the change.

- [ ] **Step 7: Run the test suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: six test executables PASS.

- [ ] **Step 8: Commit**

```bash
git add src/app
git commit --no-gpg-sign -m "feat: list training modules on the home page

A cards list over the module registry, joined with each module's best score
and mean response time. Disabled modules render dimmed with a Soon label so
the list reads at realistic length rather than looking like a one-item menu.

Statistics use -1 as the not-yet-recorded sentinel: an absent aggregate must
not render as a real score of zero, which is what a default-constructed int
would have shown.

Registry strings pass through i18n() at the model boundary, keeping the
Qt-free core free of KLocalizedString too."
```

---

### Task 10: Round summary computation

The post-round card needs accuracy, mean response time, and the three slowest squares. That is real logic that can be wrong, so it goes in the tested core rather than in the uncovered controller.

**Files:**
- Create: `src/core/RoundSummary.h`
- Create: `src/core/RoundSummary.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/tst_roundsummary.cpp`

**Interfaces:**
- Consumes: `core::Answer`, `core::Square`.
- Produces:
  - `struct core::RoundSummary { int correct; int wrong; int total; int accuracyPercent; int meanResponseMs; std::vector<Square> slowestSquares; }`
  - `core::RoundSummary core::summarise(const std::vector<Answer> &answers, std::size_t slowestCount = 3)`

- [ ] **Step 1: Write the failing test**

`tests/tst_roundsummary.cpp`:

```cpp
#include <QtTest>

#include "RoundSummary.h"

using namespace std::chrono_literals;

using core::Answer;
using core::Square;

namespace {

Answer answerFor(const char *square, bool correct,
                 std::chrono::milliseconds responseTime)
{
    const auto parsed = Square::fromAlgebraic(square);
    Q_ASSERT(parsed.has_value());

    Answer answer;
    answer.square = *parsed;
    answer.expectedDark = parsed->isDark();
    answer.answeredDark = correct ? parsed->isDark() : !parsed->isDark();
    answer.correct = correct;
    answer.responseTime = responseTime;
    return answer;
}

std::vector<std::string> names(const std::vector<Square> &squares)
{
    std::vector<std::string> result;
    for (const Square &square : squares) {
        result.push_back(square.algebraic());
    }
    return result;
}

} // namespace

class TestRoundSummary : public QObject
{
    Q_OBJECT

private slots:
    void emptyRoundSummarisesToZeroes();
    void countsCorrectAndWrong();
    void accuracyRoundsToNearestPercent();
    void meanResponseTimeCoversEveryAnswer();
    void reportsTheThreeSlowestSquares();
    void collapsesRepeatedSquaresToTheirWorstTime();
    void reportsFewerThanThreeWhenTheRoundWasShort();
};

void TestRoundSummary::emptyRoundSummarisesToZeroes()
{
    const auto summary = core::summarise({});

    QCOMPARE(summary.correct, 0);
    QCOMPARE(summary.wrong, 0);
    QCOMPARE(summary.total, 0);
    QCOMPARE(summary.accuracyPercent, 0);
    QCOMPARE(summary.meanResponseMs, 0);
    QVERIFY(summary.slowestSquares.empty());
}

void TestRoundSummary::countsCorrectAndWrong()
{
    const auto summary = core::summarise({
        answerFor("e4", true, 400ms),
        answerFor("d5", false, 500ms),
        answerFor("a1", true, 600ms),
    });

    QCOMPARE(summary.correct, 2);
    QCOMPARE(summary.wrong, 1);
    QCOMPARE(summary.total, 3);
}

void TestRoundSummary::accuracyRoundsToNearestPercent()
{
    // 2 of 3 is 66.67%, which must round to 67 rather than truncate to 66.
    const auto summary = core::summarise({
        answerFor("e4", true, 400ms),
        answerFor("d5", true, 400ms),
        answerFor("a1", false, 400ms),
    });

    QCOMPARE(summary.accuracyPercent, 67);
}

void TestRoundSummary::meanResponseTimeCoversEveryAnswer()
{
    // Wrong answers count too: the figure reports how fast the user responds,
    // not how fast they are when they happen to be right.
    const auto summary = core::summarise({
        answerFor("e4", true, 400ms),
        answerFor("d5", false, 1'200ms),
    });

    QCOMPARE(summary.meanResponseMs, 800);
}

void TestRoundSummary::reportsTheThreeSlowestSquares()
{
    const auto summary = core::summarise({
        answerFor("e4", true, 300ms),
        answerFor("h6", true, 1'400ms),
        answerFor("a7", true, 1'100ms),
        answerFor("d2", true, 900ms),
        answerFor("b5", true, 400ms),
    });

    QCOMPARE(names(summary.slowestSquares),
             (std::vector<std::string>{"h6", "a7", "d2"}));
}

void TestRoundSummary::collapsesRepeatedSquaresToTheirWorstTime()
{
    // e4 appears twice. It must be listed once, ranked by its worst time.
    const auto summary = core::summarise({
        answerFor("e4", true, 200ms),
        answerFor("e4", false, 1'500ms),
        answerFor("h6", true, 800ms),
        answerFor("a7", true, 700ms),
        answerFor("b5", true, 600ms),
    });

    QCOMPARE(names(summary.slowestSquares),
             (std::vector<std::string>{"e4", "h6", "a7"}));
}

void TestRoundSummary::reportsFewerThanThreeWhenTheRoundWasShort()
{
    const auto summary = core::summarise({
        answerFor("e4", true, 300ms),
        answerFor("h6", true, 900ms),
    });

    QCOMPARE(names(summary.slowestSquares),
             (std::vector<std::string>{"h6", "e4"}));
}

QTEST_APPLESS_MAIN(TestRoundSummary)
#include "tst_roundsummary.moc"
```

- [ ] **Step 2: Register the test**

```cmake
set(CORE_TESTS
    square
    promptgenerator
    round
    roundsummary
    moduleregistry
)
```

- [ ] **Step 3: Run the test to verify it fails**

```bash
cmake --build build
```

Expected: build FAILS with `fatal error: RoundSummary.h: No such file or directory`.

- [ ] **Step 4: Write src/core/RoundSummary.h**

```cpp
#pragma once

#include <cstddef>
#include <vector>

#include "SquareColorRound.h"

namespace core {

struct RoundSummary {
    int correct = 0;
    int wrong = 0;
    int total = 0;
    int accuracyPercent = 0;
    int meanResponseMs = 0;
    /// Up to slowestCount squares, worst first. A square answered more than
    /// once appears once, ranked by its worst response time.
    std::vector<Square> slowestSquares;
};

RoundSummary summarise(const std::vector<Answer> &answers,
                       std::size_t slowestCount = 3);

} // namespace core
```

- [ ] **Step 5: Write src/core/RoundSummary.cpp**

```cpp
#include "RoundSummary.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace core {

RoundSummary summarise(const std::vector<Answer> &answers,
                       std::size_t slowestCount)
{
    RoundSummary summary;
    if (answers.empty()) {
        return summary;
    }

    summary.total = static_cast<int>(answers.size());

    // Worst response time per square, indexed by square. -1 means unanswered.
    std::array<long long, 64> worstBySquare{};
    worstBySquare.fill(-1);

    long long totalResponseMs = 0;

    for (const Answer &answer : answers) {
        if (answer.correct) {
            ++summary.correct;
        } else {
            ++summary.wrong;
        }

        const long long responseMs = answer.responseTime.count();
        totalResponseMs += responseMs;

        const std::size_t index = static_cast<std::size_t>(answer.square.index());
        worstBySquare[index] = std::max(worstBySquare[index], responseMs);
    }

    summary.accuracyPercent = static_cast<int>(
        std::lround(100.0 * summary.correct / summary.total));
    summary.meanResponseMs =
        static_cast<int>(std::lround(static_cast<double>(totalResponseMs)
                                     / summary.total));

    struct Ranked {
        int index;
        long long worstMs;
    };

    std::vector<Ranked> ranked;
    ranked.reserve(answers.size());
    for (int index = 0; index < 64; ++index) {
        const long long worstMs = worstBySquare[static_cast<std::size_t>(index)];
        if (worstMs >= 0) {
            ranked.push_back(Ranked{index, worstMs});
        }
    }

    // Slowest first. Ties break on square index so the order is deterministic
    // rather than dependent on answer sequence.
    std::sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
        if (a.worstMs != b.worstMs) {
            return a.worstMs > b.worstMs;
        }
        return a.index < b.index;
    });

    const std::size_t take = std::min(slowestCount, ranked.size());
    summary.slowestSquares.reserve(take);
    for (std::size_t i = 0; i < take; ++i) {
        summary.slowestSquares.push_back(Square::fromIndex(ranked[i].index));
    }

    return summary;
}

} // namespace core
```

- [ ] **Step 6: Add RoundSummary.cpp to the core target**

```cmake
add_library(chesstrainer-core STATIC
    Square.cpp
    PromptGenerator.cpp
    SquareColorRound.cpp
    RoundSummary.cpp
    ModuleRegistry.cpp
)
```

- [ ] **Step 7: Run the tests to verify they pass**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: seven test executables PASS, `tst_roundsummary` reporting 7 passing functions.

- [ ] **Step 8: Commit**

```bash
git add src/core tests
git commit --no-gpg-sign -m "feat: summarise a finished round in the core

Accuracy, mean response time and the three slowest squares are real
computations that can be wrong, so they live in the tested core rather than in
the controller, which coverage deliberately excludes.

A square answered more than once in a round is listed once, ranked by its
worst time - otherwise a single slow square could fill the whole slowest-three
list. Accuracy rounds rather than truncates, so 2 of 3 reports 67 not 66, and
ties in the slowest ranking break on square index so the output does not
depend on answer order."
```

---

### Task 11: Square Color controller

**Files:**
- Create: `src/app/SquareColorController.h`
- Create: `src/app/SquareColorController.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `src/app/main.cpp`

**Interfaces:**
- Consumes: `core::SquareColorRound`, `core::MonotonicClock`, `core::PromptGenerator`, `core::summarise`, `store::RunRepository`, `app::ModuleListModel`.
- Produces: `app::SquareColorController : QObject`, exposed to QML as the context property `squareColor`, with:
  - properties `running` (bool), `promptText` (QString), `remainingMs` (int), `roundLengthMs` (int), `correct` (int), `wrong` (int)
  - summary properties `summaryCorrect`, `summaryWrong`, `summaryAccuracyPercent`, `summaryMeanResponseMs` (int), `summarySlowestSquares` (QStringList), `bestScore` (int, -1 when none)
  - `Q_INVOKABLE void start()`, `Q_INVOKABLE bool answer(bool dark)` returning whether the answer was correct, `Q_INVOKABLE void abort()`
  - signals `roundFinished()` and `answered(bool correct)`

- [ ] **Step 1: Write src/app/SquareColorController.h**

```cpp
#pragma once

#include <memory>
#include <optional>

#include <QObject>
#include <QStringList>
#include <QTimer>

#include "Clock.h"
#include "SquareColorRound.h"

namespace store {
class RunRepository;
}

namespace app {

class ModuleListModel;

/// Drives a core::SquareColorRound from QML and persists the result.
class SquareColorController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(QString promptText READ promptText NOTIFY promptChanged)
    Q_PROPERTY(int remainingMs READ remainingMs NOTIFY remainingChanged)
    Q_PROPERTY(int roundLengthMs READ roundLengthMs CONSTANT)
    Q_PROPERTY(int correct READ correct NOTIFY scoreChanged)
    Q_PROPERTY(int wrong READ wrong NOTIFY scoreChanged)

    Q_PROPERTY(int summaryCorrect MEMBER m_summaryCorrect NOTIFY summaryChanged)
    Q_PROPERTY(int summaryWrong MEMBER m_summaryWrong NOTIFY summaryChanged)
    Q_PROPERTY(int summaryAccuracyPercent MEMBER m_summaryAccuracyPercent
                   NOTIFY summaryChanged)
    Q_PROPERTY(int summaryMeanResponseMs MEMBER m_summaryMeanResponseMs
                   NOTIFY summaryChanged)
    Q_PROPERTY(QStringList summarySlowestSquares MEMBER m_summarySlowestSquares
                   NOTIFY summaryChanged)
    Q_PROPERTY(int bestScore MEMBER m_bestScore NOTIFY summaryChanged)

public:
    /// repository and moduleList may be null when storage failed to open.
    SquareColorController(store::RunRepository *repository,
                          ModuleListModel *moduleList,
                          QObject *parent = nullptr);

    bool running() const;
    QString promptText() const;
    int remainingMs() const;
    int roundLengthMs() const;
    int correct() const;
    int wrong() const;

    Q_INVOKABLE void start();

    /// Records an answer. Returns true when it was correct; a rejected answer
    /// (round over, or not started) returns false and emits nothing.
    Q_INVOKABLE bool answer(bool dark);

    Q_INVOKABLE void abort();

Q_SIGNALS:
    void stateChanged();
    void promptChanged();
    void remainingChanged();
    void scoreChanged();
    void summaryChanged();
    void answered(bool correct);
    void roundFinished();

private:
    void onTick();
    void finishRound(bool completed);

    core::MonotonicClock m_clock;
    std::unique_ptr<core::SquareColorRound> m_round;
    QTimer m_timer;

    store::RunRepository *m_repository;
    ModuleListModel *m_moduleList;

    int m_summaryCorrect = 0;
    int m_summaryWrong = 0;
    int m_summaryAccuracyPercent = 0;
    int m_summaryMeanResponseMs = 0;
    QStringList m_summarySlowestSquares;
    int m_bestScore = -1;
};

} // namespace app
```

- [ ] **Step 2: Write src/app/SquareColorController.cpp**

```cpp
#include "SquareColorController.h"

#include <chrono>
#include <random>

#include <QDateTime>

#include "ModuleListModel.h"
#include "ModuleRegistry.h"
#include "RoundSummary.h"
#include "RunRepository.h"

namespace app {

namespace {

/// A fresh nondeterministic seed per round, so consecutive rounds do not
/// replay the same prompt sequence.
std::uint64_t makeSeed()
{
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32)
        ^ static_cast<std::uint64_t>(device());
}

} // namespace

SquareColorController::SquareColorController(store::RunRepository *repository,
                                              ModuleListModel *moduleList,
                                              QObject *parent)
    : QObject(parent)
    , m_repository(repository)
    , m_moduleList(moduleList)
{
    // 16 ms keeps the countdown ring smooth at 60 Hz. The authoritative clock
    // is in the core; this only samples it.
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &SquareColorController::onTick);
}

bool SquareColorController::running() const
{
    return m_round && m_round->state() == core::RoundState::Running;
}

QString SquareColorController::promptText() const
{
    if (!running()) {
        return {};
    }
    return QString::fromStdString(m_round->currentPrompt().algebraic());
}

int SquareColorController::remainingMs() const
{
    if (!m_round) {
        return static_cast<int>(core::SquareColorRound::kDefaultRoundLength.count());
    }
    return static_cast<int>(m_round->remaining().count());
}

int SquareColorController::roundLengthMs() const
{
    return static_cast<int>(core::SquareColorRound::kDefaultRoundLength.count());
}

int SquareColorController::correct() const
{
    return m_round ? m_round->correct() : 0;
}

int SquareColorController::wrong() const
{
    return m_round ? m_round->wrong() : 0;
}

void SquareColorController::start()
{
    m_round = std::make_unique<core::SquareColorRound>(
        m_clock,
        core::PromptGenerator(makeSeed()),
        core::SquareColorRound::kDefaultRoundLength);
    m_round->start();
    m_timer.start();

    Q_EMIT stateChanged();
    Q_EMIT promptChanged();
    Q_EMIT remainingChanged();
    Q_EMIT scoreChanged();
}

bool SquareColorController::answer(bool dark)
{
    if (!running()) {
        return false;
    }

    const core::AnswerOutcome outcome = m_round->answer(dark);
    if (outcome == core::AnswerOutcome::Ignored) {
        // The clock expired between the last tick and this keypress.
        finishRound(true);
        return false;
    }

    const bool wasCorrect = outcome == core::AnswerOutcome::Correct;

    Q_EMIT scoreChanged();
    Q_EMIT promptChanged();
    Q_EMIT answered(wasCorrect);
    return wasCorrect;
}

void SquareColorController::abort()
{
    if (!running()) {
        return;
    }
    m_round->abort();
    finishRound(false);
}

void SquareColorController::onTick()
{
    if (!m_round) {
        return;
    }

    Q_EMIT remainingChanged();

    // tick() reports true only on the transition, so this cannot fire twice.
    if (m_round->tick()) {
        finishRound(true);
    }
}

void SquareColorController::finishRound(bool completed)
{
    m_timer.stop();

    const auto summary = core::summarise(m_round->answers());
    m_summaryCorrect = summary.correct;
    m_summaryWrong = summary.wrong;
    m_summaryAccuracyPercent = summary.accuracyPercent;
    m_summaryMeanResponseMs = summary.meanResponseMs;

    m_summarySlowestSquares.clear();
    for (const core::Square &square : summary.slowestSquares) {
        m_summarySlowestSquares << QString::fromStdString(square.algebraic());
    }

    if (m_repository) {
        store::RunRecord record;
        record.moduleId = QString::fromStdString(
            std::string(core::kSquareColorModuleId));
        record.startedAtUtc = QDateTime::currentDateTimeUtc();
        record.roundLengthMs = static_cast<int>(m_round->roundLength().count());
        record.correct = summary.correct;
        record.wrong = summary.wrong;
        record.completed = completed;
        record.answers = m_round->answers();

        QString error;
        // A write failure must not lose the round on screen: the summary is
        // already computed from memory and is shown either way.
        m_repository->writeRun(record, &error);

        if (const auto stats = m_repository->moduleSummary(record.moduleId, &error)) {
            m_bestScore = stats->bestScore.value_or(-1);
        }
    }

    if (m_moduleList) {
        m_moduleList->refresh();
    }

    Q_EMIT stateChanged();
    Q_EMIT summaryChanged();
    Q_EMIT roundFinished();
}

} // namespace app
```

`record.startedAtUtc` uses the current time rather than the round's start instant: the core clock is a `steady_clock` with no calendar meaning, and the difference is at most 30 seconds on a timestamp whose purpose is ordering runs by day.

- [ ] **Step 3: Add the controller to the target and expose it**

In `src/app/CMakeLists.txt`:

```cmake
qt_add_executable(chess-trainer
    main.cpp
    ModuleListModel.h
    ModuleListModel.cpp
    SquareColorController.h
    SquareColorController.cpp
)
```

In `src/app/main.cpp`, add the include:

```cpp
#include "SquareColorController.h"
```

create the controller after the model:

```cpp
    app::SquareColorController squareColor(storageReady ? &repository : nullptr,
                                           &moduleList);
```

and register it:

```cpp
    engine.rootContext()->setContextProperty(QStringLiteral("squareColor"),
                                             &squareColor);
```

- [ ] **Step 4: Build and confirm it still starts**

```bash
cmake --build build && ./build/bin/chess-trainer
```

Expected: the home page behaves exactly as after Task 9 — the controller has no UI yet. No new stderr warnings.

- [ ] **Step 5: Run the test suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: seven test executables PASS.

- [ ] **Step 6: Commit**

```bash
git add src/app
git commit --no-gpg-sign -m "feat: add the square color controller

Owns a core round, samples it from a 16 ms timer for a smooth countdown, and
exposes prompt, score and summary state to QML. The authoritative clock stays
in the core; the timer only reads it.

Each round is seeded from random_device so consecutive rounds do not replay
the same prompt sequence.

An answer arriving after expiry but before the next tick finishes the round
rather than scoring, and a persistence failure is swallowed deliberately: the
summary is computed from memory and must still be shown even when the write
fails."
```

---

### Task 12: Drill screen

The immersive drill. Chrome hides for the round, the coordinate is the largest thing on screen, and all three input rules from spec §7.3 are enforced here.

**Files:**
- Create: `src/app/qml/components/CountdownRing.qml`
- Create: `src/app/qml/components/AnswerButton.qml`
- Create: `src/app/qml/DrillPage.qml`
- Modify: `src/app/CMakeLists.txt`

**Interfaces:**
- Consumes: the `squareColor` context property from Task 11.
- Produces: `CountdownRing` with properties `remainingMs`, `totalMs`; `AnswerButton` with properties `label`, `shortcutHint`, `primary` and method `flash(bool correct)`; `DrillPage` pushed by `HomePage`'s `moduleRequested` signal.

- [ ] **Step 1: Write src/app/qml/components/CountdownRing.qml**

Canvas rather than `QtQuick.Shapes`, so no extra CMake component is needed.

```qml
import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Item {
    id: root

    property int remainingMs: 0
    property int totalMs: 30000

    readonly property real progress: totalMs > 0
        ? Math.max(0, Math.min(1, remainingMs / totalMs))
        : 0

    readonly property int remainingSeconds: Math.ceil(remainingMs / 1000)

    implicitWidth: Kirigami.Units.gridUnit * 7
    implicitHeight: implicitWidth

    onProgressChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent

        readonly property real strokeWidth: Kirigami.Units.smallSpacing * 1.5

        onPaint: {
            const context = getContext("2d");
            context.reset();

            const centreX = width / 2;
            const centreY = height / 2;
            const radius = Math.min(width, height) / 2 - strokeWidth;

            context.lineWidth = strokeWidth;
            context.lineCap = "round";

            // Track
            context.beginPath();
            context.strokeStyle = Kirigami.Theme.alternateBackgroundColor;
            context.arc(centreX, centreY, radius, 0, 2 * Math.PI);
            context.stroke();

            if (root.progress <= 0) {
                return;
            }

            // Remaining time, sweeping clockwise from twelve o'clock.
            context.beginPath();
            context.strokeStyle = Kirigami.Theme.highlightColor;
            context.arc(centreX, centreY, radius,
                        -Math.PI / 2,
                        -Math.PI / 2 + (2 * Math.PI * root.progress));
            context.stroke();
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 0

        Kirigami.Heading {
            level: 1
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.remainingSeconds
        }

        Controls.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: i18nc("abbreviation for seconds", "SEC")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            font.letterSpacing: 2
            opacity: 0.6
        }
    }
}
```

- [ ] **Step 2: Write src/app/qml/components/AnswerButton.qml**

```qml
import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Controls.Button {
    id: button

    property string shortcutHint: ""
    property bool primary: false

    // Never focusable: keys are handled by DrillPage, and a focused button
    // would let Space or Enter fire an answer alongside the key handler.
    focusPolicy: Qt.NoFocus

    // Buttons only ever grow, so a long round cannot shift the hit targets.
    implicitHeight: Kirigami.Units.gridUnit * 3.5

    highlighted: primary

    /// Tints the button for 110 ms. The next prompt is already on screen by
    /// the time this runs, so the flash never delays the drill.
    function flash(correct) {
        tint.color = correct
            ? Kirigami.Theme.positiveTextColor
            : Kirigami.Theme.negativeTextColor;
        flashAnimation.restart();
    }

    contentItem: Column {
        spacing: 0

        Controls.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: button.text
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.5
        }

        Controls.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: button.shortcutHint.length > 0
            text: button.shortcutHint
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.6
        }
    }

    Rectangle {
        id: tint
        anchors.fill: parent
        radius: Kirigami.Units.cornerRadius
        opacity: 0
        z: 1

        // Clicks must reach the button underneath.
        visible: opacity > 0
    }

    SequentialAnimation {
        id: flashAnimation

        PropertyAction { target: tint; property: "opacity"; value: 0.45 }
        PauseAnimation { duration: 110 }
        NumberAnimation {
            target: tint
            property: "opacity"
            to: 0
            duration: 90
        }
    }
}
```

If your Kirigami is older than 6.5 and `Kirigami.Units.cornerRadius` is undefined, use `Kirigami.Units.smallSpacing` instead and note the substitution.

- [ ] **Step 3: Write src/app/qml/DrillPage.qml**

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

import "components"

Kirigami.Page {
    id: page

    // Immersive: the round gets the whole window. Chrome returns with the
    // summary because leaving this page pops it.
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
    padding: Kirigami.Units.largeSpacing * 2

    // The page owns every key: buttons are not focusable, so no other item
    // can consume or duplicate an answer.
    focus: true

    Component.onCompleted: squareColor.start()

    Keys.onPressed: (event) => {
        // A held-down arrow key must not spray answers.
        if (event.isAutoRepeat) {
            event.accepted = true;
            return;
        }

        switch (event.key) {
        case Qt.Key_Left:
        case Qt.Key_L:
            page.submit(false);
            event.accepted = true;
            break;
        case Qt.Key_Right:
        case Qt.Key_D:
            page.submit(true);
            event.accepted = true;
            break;
        case Qt.Key_Escape:
            squareColor.abort();
            event.accepted = true;
            break;
        default:
            event.accepted = false;
        }
    }

    /// Single entry point for both mouse and keyboard, so one interaction can
    /// never produce two answers.
    function submit(dark) {
        if (!squareColor.running) {
            return;
        }
        const wasCorrect = squareColor.answer(dark);
        const button = dark ? darkButton : lightButton;
        button.flash(wasCorrect);
    }

    Connections {
        target: squareColor
        function onRoundFinished() {
            summarySheet.open();
        }
    }

    SummarySheet {
        id: summarySheet

        onAgainRequested: {
            close();
            squareColor.start();
            page.forceActiveFocus();
        }
        onHomeRequested: {
            close();
            // applicationWindow() is Kirigami's accessor for the root window;
            // the ColumnView attached property is not available on a Page.
            applicationWindow().pageStack.pop();
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width, Kirigami.Units.gridUnit * 22)
        spacing: Kirigami.Units.largeSpacing

        CountdownRing {
            Layout.alignment: Qt.AlignHCenter
            remainingMs: squareColor.remainingMs
            totalMs: squareColor.roundLengthMs
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: squareColor.promptText
            // Deliberately larger than any heading level: this is the one
            // thing the user reads under time pressure.
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 4
            font.weight: Font.Light
            font.letterSpacing: 4
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.largeSpacing

            Controls.Label {
                text: i18nc("correct answer count", "✓ %1", squareColor.correct)
                color: Kirigami.Theme.positiveTextColor
            }
            Controls.Label {
                text: i18nc("wrong answer count", "✗ %1", squareColor.wrong)
                color: Kirigami.Theme.negativeTextColor
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            AnswerButton {
                id: lightButton
                Layout.fillWidth: true
                primary: true
                text: i18nc("light coloured square", "Light")
                shortcutHint: i18nc("keyboard shortcuts", "← / L")
                onClicked: page.submit(false)
            }

            AnswerButton {
                id: darkButton
                Layout.fillWidth: true
                text: i18nc("dark coloured square", "Dark")
                shortcutHint: i18nc("keyboard shortcuts", "→ / D")
                onClicked: page.submit(true)
            }
        }

        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            text: i18nc("keyboard hint", "Esc  end round")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.45
        }
    }
}
```

- [ ] **Step 4: Register the new QML files**

```cmake
qt_add_qml_module(chess-trainer
    URI ChessTrainer
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/HomePage.qml
        qml/DrillPage.qml
        qml/SummarySheet.qml
        qml/components/CountdownRing.qml
        qml/components/AnswerButton.qml
)
```

`SummarySheet.qml` is listed now because `DrillPage.qml` references it; Task 13 writes it. Until then the build succeeds but opening the drill logs a missing-type error.

- [ ] **Step 5: Write a temporary stub so this task is testable on its own**

Create `src/app/qml/SummarySheet.qml` with just enough to instantiate. Task 13 replaces the whole file.

```qml
import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.Dialog {
    signal againRequested()
    signal homeRequested()
    title: i18n("Round complete")
}
```

- [ ] **Step 6: Build and run the drill**

```bash
cmake --build build && ./build/bin/chess-trainer
```

Expected, verified by hand:
1. Click Start on Square Color. The toolbar disappears and the drill fills the window.
2. The ring sweeps down smoothly and the seconds number decreases.
3. A coordinate like `e4` is shown large and centred.
4. Pressing `Left` or `L` answers light; `Right` or `D` answers dark. The pressed button tints green or red for about a tenth of a second, and the next coordinate appears immediately rather than after the flash.
5. Clicking the buttons with the mouse does the same.
6. **Hold** `Left` down. The tally must increase by exactly one, not continuously — this is the auto-repeat guard.
7. Press `Tab` a few times, then press `Left`. It must still answer, because the buttons refuse focus.
8. Let the clock reach zero. An empty dialog titled "Round complete" appears, and further key presses do not change the tally.
9. Press `Esc` mid-round. The same dialog appears.

- [ ] **Step 7: Run the test suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: seven test executables PASS.

- [ ] **Step 8: Commit**

```bash
git add src/app
git commit --no-gpg-sign -m "feat: add the immersive drill screen

The round gets the whole window: the Kirigami toolbar is hidden, the ring and
the coordinate grow, and answer buttons are full-width.

All three input rules from the design live here. Auto-repeat events are
dropped so a held key answers once. Buttons set focusPolicy to NoFocus and
the page owns every key, so Space or Enter on a focused button cannot fire an
answer alongside the key handler. Mouse and keyboard share one submit()
entry point.

The 110 ms tint overlaps the next prompt rather than delaying it, so feedback
costs no drill time."
```

---

### Task 13: Summary sheet

**Files:**
- Modify: `src/app/qml/SummarySheet.qml` (replace the Task 12 stub)

**Interfaces:**
- Consumes: the `squareColor` context property's summary properties from Task 11.
- Produces: `SummarySheet` with signals `againRequested()` and `homeRequested()`, already wired by `DrillPage`.

- [ ] **Step 1: Replace src/app/qml/SummarySheet.qml**

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Kirigami.Dialog {
    id: sheet

    signal againRequested()
    signal homeRequested()

    title: i18n("Round complete")

    // The drill is modal here on purpose: dismissing by clicking away would
    // lose the result the user just earned.
    closePolicy: Controls.Popup.NoAutoClose

    padding: Kirigami.Units.largeSpacing * 2

    // Controls.Dialog, not Kirigami.Dialog: the enum lives on the QtQuick
    // Controls base type.
    standardButtons: Controls.Dialog.NoButton

    customFooterActions: [
        Kirigami.Action {
            text: i18nc("start another round", "Again")
            icon.name: "media-playback-start"
            onTriggered: sheet.againRequested()
        },
        Kirigami.Action {
            text: i18nc("return to the module list", "Home")
            icon.name: "go-home"
            onTriggered: sheet.homeRequested()
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.gridUnit * 2

            ColumnLayout {
                spacing: 0
                Kirigami.Heading {
                    Layout.alignment: Qt.AlignHCenter
                    level: 1
                    text: squareColor.summaryCorrect
                    color: Kirigami.Theme.positiveTextColor
                }
                Controls.Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: i18n("correct")
                    opacity: 0.7
                }
            }

            ColumnLayout {
                spacing: 0
                Kirigami.Heading {
                    Layout.alignment: Qt.AlignHCenter
                    level: 1
                    text: squareColor.summaryWrong
                    color: Kirigami.Theme.negativeTextColor
                }
                Controls.Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: i18n("wrong")
                    opacity: 0.7
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        GridLayout {
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true

            Controls.Label {
                text: i18n("Accuracy")
                opacity: 0.7
            }
            Controls.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: i18nc("percentage", "%1%", squareColor.summaryAccuracyPercent)
            }

            Controls.Label {
                text: i18n("Average response")
                opacity: 0.7
            }
            Controls.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: i18nc("milliseconds", "%1 ms", squareColor.summaryMeanResponseMs)
            }

            Controls.Label {
                visible: squareColor.summarySlowestSquares.length > 0
                text: i18n("Slowest squares")
                opacity: 0.7
            }
            Controls.Label {
                visible: squareColor.summarySlowestSquares.length > 0
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: squareColor.summarySlowestSquares.join("  ")
            }

            Controls.Label {
                // -1 means no completed run has ever been recorded, which
                // happens when storage is unavailable.
                visible: squareColor.bestScore >= 0
                text: i18n("Best ever")
                opacity: 0.7
            }
            Controls.Label {
                visible: squareColor.bestScore >= 0
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                color: Kirigami.Theme.highlightColor
                text: squareColor.bestScore
            }
        }
    }
}
```

- [ ] **Step 2: Build and run a full round**

```bash
cmake --build build && ./build/bin/chess-trainer
```

Expected, verified by hand:
1. Play a 30-second round answering a mix of correct and wrong.
2. On expiry the sheet shows correct and wrong counts, an accuracy percentage consistent with them, an average response in milliseconds, and up to three slowest squares.
3. "Best ever" appears and equals your correct count on the very first completed round.
4. Clicking away from the dialog does not dismiss it.
5. "Again" starts a fresh round immediately, with keyboard input still working — this confirms focus returned to the drill page.
6. "Home" returns to the module list, and the Square Color card now shows `best N · avg M ms`.
7. Run a second, deliberately worse round. "Best ever" and the home card keep the higher score.
8. Start a round and press `Esc` after two answers. The sheet appears; return Home and confirm the card's best score did **not** drop to 2 — aborted runs are excluded.

- [ ] **Step 3: Confirm the data landed**

```bash
sqlite3 ~/.local/share/chess-trainer/trainer.db \
  "SELECT id, correct, wrong, completed FROM run ORDER BY id;" \
  "SELECT COUNT(*), MIN(response_ms), MAX(response_ms) FROM answer;"
```

Expected: one `run` row per round played, `completed = 0` for the aborted one, and an `answer` row count equal to the total answers you gave. If `sqlite3` is not installed, `sudo dnf install -y sqlite`.

- [ ] **Step 4: Run the test suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: seven test executables PASS.

- [ ] **Step 5: Commit**

```bash
git add src/app
git commit --no-gpg-sign -m "feat: show the post-round summary

Correct and wrong counts, accuracy, mean response time, the three slowest
squares and best-ever, with Again and Home actions.

The dialog refuses auto-close: clicking away would discard the result the user
just earned. Rows whose value is absent are hidden rather than shown as zero,
so a round with no recorded best does not claim a best of nothing."
```

---

### Task 14: Storage failure handling

Spec §10: a storage failure must never block training. Tasks 8 to 13 already keep the drill working without a database; this task makes the failure visible and recoverable.

**Files:**
- Modify: `src/store/Database.h`
- Modify: `src/store/Database.cpp`
- Modify: `src/app/main.cpp`
- Modify: `src/app/qml/HomePage.qml`
- Modify: `tests/tst_database.cpp`

**Interfaces:**
- Consumes: `store::Database` from Task 6.
- Produces: `static QString store::Database::moveAside(const QString &path, QString *errorOut)` returning the backup path it created (empty string on failure); `openOrRecover(const QString &path, QString *errorOut, QString *recoveredFromOut)` which retries once after moving a broken file aside.

- [ ] **Step 1: Write the failing test**

Append to `tests/tst_database.cpp` — add the slot declarations to the `private slots:` block and the includes at the top:

```cpp
#include <QFile>
#include <QTemporaryDir>
```

```cpp
void TestDatabase::recoversFromACorruptFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("trainer.db"));

    // Not a SQLite file at all. SQLite opens it lazily, so the failure only
    // surfaces when migrate() runs its first statement.
    QFile broken(path);
    QVERIFY(broken.open(QIODevice::WriteOnly));
    broken.write(QByteArrayLiteral("this is not a database"));
    broken.close();

    store::Database database;
    QString error;
    QString recoveredFrom;
    QVERIFY2(database.openOrRecover(path, &error, &recoveredFrom),
             qPrintable(error));

    // The broken file was moved aside, and a working database took its place.
    QVERIFY(!recoveredFrom.isEmpty());
    QVERIFY(QFile::exists(recoveredFrom));
    QCOMPARE(database.schemaVersion(), store::Database::kCurrentSchemaVersion);
}

void TestDatabase::openOrRecoverLeavesAHealthyFileAlone()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("trainer.db"));

    {
        store::Database first;
        QString error;
        QString recoveredFrom;
        QVERIFY(first.openOrRecover(path, &error, &recoveredFrom));
        QVERIFY(recoveredFrom.isEmpty());
    }

    store::Database second;
    QString error;
    QString recoveredFrom;
    QVERIFY2(second.openOrRecover(path, &error, &recoveredFrom), qPrintable(error));
    QVERIFY2(recoveredFrom.isEmpty(),
             "a healthy database must not be moved aside");
    QCOMPARE(second.schemaVersion(), 1);
}
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake --build build
```

Expected: build FAILS with `no member named 'openOrRecover' in 'store::Database'`.

- [ ] **Step 3: Declare the new API in src/store/Database.h**

Add inside the public section:

```cpp
    /// Opens and migrates, and if that fails on an existing file, moves the
    /// file aside and retries once with a fresh database. On success,
    /// recoveredFromOut receives the backup path, or an empty string when no
    /// recovery was needed.
    bool openOrRecover(const QString &path,
                       QString *errorOut,
                       QString *recoveredFromOut);

    /// Renames path to path + ".bak" (with a numeric suffix if that exists).
    /// Returns the new path, or an empty string on failure.
    static QString moveAside(const QString &path, QString *errorOut);
```

- [ ] **Step 4: Implement them in src/store/Database.cpp**

Add `#include <QFile>` at the top, then:

```cpp
QString Database::moveAside(const QString &path, QString *errorOut)
{
    QString target = path + QStringLiteral(".bak");
    for (int suffix = 2; QFile::exists(target) && suffix < 100; ++suffix) {
        target = path + QStringLiteral(".bak.%1").arg(suffix);
    }

    if (!QFile::rename(path, target)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot move %1 aside").arg(path);
        }
        return {};
    }

    return target;
}

bool Database::openOrRecover(const QString &path,
                              QString *errorOut,
                              QString *recoveredFromOut)
{
    if (recoveredFromOut) {
        recoveredFromOut->clear();
    }

    QString firstError;
    if (open(path, &firstError) && migrate(&firstError)) {
        return true;
    }

    // An in-memory database has nothing to recover, and neither does a path
    // that does not exist - in both cases the first failure is the real one.
    if (path == QStringLiteral(":memory:") || !QFile::exists(path)) {
        return fail(errorOut, firstError);
    }

    // Release the failed connection before touching the file.
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
            if (database.isOpen()) {
                database.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    QString moveError;
    const QString backup = moveAside(path, &moveError);
    if (backup.isEmpty()) {
        return fail(errorOut,
                    QStringLiteral("%1 (and %2)").arg(firstError, moveError));
    }

    QString retryError;
    if (!open(path, &retryError) || !migrate(&retryError)) {
        return fail(errorOut, retryError);
    }

    if (recoveredFromOut) {
        *recoveredFromOut = backup;
    }
    return true;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: seven test executables PASS, `tst_database` now reporting 8 passing functions.

- [ ] **Step 6: Use it from src/app/main.cpp**

Replace the storage block written in Task 8 with:

```cpp
    const QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    store::Database database;
    QString storageError;
    QString recoveredFrom;
    const bool storageReady = database.openOrRecover(
        dataDirectory + QStringLiteral("/trainer.db"),
        &storageError,
        &recoveredFrom);
```

and replace the two context properties with three:

```cpp
    engine.rootContext()->setContextProperty(QStringLiteral("storageReady"),
                                             storageReady);
    engine.rootContext()->setContextProperty(
        QStringLiteral("storageMessage"),
        storageReady
            ? (recoveredFrom.isEmpty()
                   ? QString()
                   : i18n("Your statistics database could not be read and was "
                          "moved to %1. A new one has been started.",
                          recoveredFrom))
            : i18n("Statistics cannot be saved: %1. Training still works.",
                   storageError));
```

- [ ] **Step 7: Render the message on the home page**

In `src/app/qml/HomePage.qml`, wrap the existing `Kirigami.CardsListView` so the banner sits above it. Replace the `Kirigami.CardsListView { ... }` block's enclosing scope by giving the page a header:

```qml
    header: Kirigami.InlineMessage {
        width: parent.width
        // Non-blocking by design: storage problems must never stand between
        // the user and a round of training.
        visible: storageMessage.length > 0
        text: storageMessage
        type: storageReady ? Kirigami.MessageType.Warning
                           : Kirigami.MessageType.Error
        showCloseButton: true
    }
```

- [ ] **Step 8: Verify both failure paths by hand**

Corrupt-file recovery:

```bash
pkill chess-trainer 2>/dev/null
echo "not a database" > ~/.local/share/chess-trainer/trainer.db
./build/bin/chess-trainer
```

Expected: a yellow warning banner on the home page naming a `.bak` path, modules still listed, and a round can be played and saved. Confirm the backup exists with `ls ~/.local/share/chess-trainer/`.

Read-only directory:

```bash
pkill chess-trainer 2>/dev/null
chmod a-w ~/.local/share/chess-trainer
./build/bin/chess-trainer
```

Expected: a red error banner saying statistics cannot be saved, and a full round still playable with a summary — only "Best ever" and the card statistics are absent. Then restore:

```bash
chmod u+w ~/.local/share/chess-trainer
```

- [ ] **Step 9: Commit**

```bash
git add src/store src/app tests
git commit --no-gpg-sign -m "feat: recover from an unreadable statistics database

openOrRecover moves a database it cannot open or migrate aside to a .bak path
and retries once with a fresh file, so a corrupt database costs history rather
than the ability to train. A healthy file is never touched, and an in-memory
or missing path reports the original error instead of attempting recovery.

The outcome surfaces as a non-blocking InlineMessage on the home page: a
warning when history was moved aside, an error when nothing can be saved at
all. Neither blocks a round."
```

---

### Task 15: Desktop entry, AppStream metadata and icon

Last task. Makes the application installable and prepares the Flathub submission without performing it.

**Files:**
- Create: `data/io.github.martikan.ChessTrainer.desktop`
- Create: `data/io.github.martikan.ChessTrainer.metainfo.xml`
- Create: `data/icons/io.github.martikan.ChessTrainer.svg`
- Create: `data/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: the `chess-trainer` install target from Task 8.
- Produces: installed desktop entry, AppStream metainfo and scalable icon under the application ID.

- [ ] **Step 1: Write data/io.github.martikan.ChessTrainer.desktop**

```ini
[Desktop Entry]
Type=Application
Name=Chess Trainer
GenericName=Chess Board Vision Trainer
Comment=Train your chess board vision
Exec=chess-trainer
Icon=io.github.martikan.ChessTrainer
Terminal=false
Categories=Game;BoardGame;Education;
Keywords=chess;board;vision;training;squares;coordinates;
StartupNotify=true
StartupWMClass=chess-trainer
```

- [ ] **Step 2: Write data/io.github.martikan.ChessTrainer.metainfo.xml**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>io.github.martikan.ChessTrainer</id>

  <name>Chess Trainer</name>
  <summary>Train your chess board vision</summary>

  <metadata_license>CC0-1.0</metadata_license>
  <project_license>MIT</project_license>

  <developer id="io.github.martikan">
    <name>Richard Martikan</name>
  </developer>

  <description>
    <p>
      Chess Trainer drills the board skills that strong players take for
      granted, one exercise at a time.
    </p>
    <p>
      The Square Color module names a square and asks whether it is light or
      dark, as fast as you can answer, for thirty seconds. Every answer is
      timed, so you can see not just whether you were right but which squares
      still make you hesitate.
    </p>
  </description>

  <launchable type="desktop-id">io.github.martikan.ChessTrainer.desktop</launchable>

  <url type="homepage">https://github.com/martikan/chess-trainer</url>
  <url type="bugtracker">https://github.com/martikan/chess-trainer/issues</url>
  <url type="vcs-browser">https://github.com/martikan/chess-trainer</url>

  <categories>
    <category>Game</category>
    <category>BoardGame</category>
    <category>Education</category>
  </categories>

  <supports>
    <control>keyboard</control>
    <control>pointing</control>
  </supports>

  <content_rating type="oars-1.1"/>

  <releases>
    <release version="0.1.0" date="2026-09-09">
      <description>
        <p>First release, with the Square Color training module.</p>
      </description>
    </release>
  </releases>
</component>
```

Flathub requires at least one screenshot before it will accept a submission. That is deliberately not added here: the screenshot must be a hosted URL, which belongs to the packaging phase rather than this one. `appstreamcli validate` will report the missing `<screenshots>` as a warning, not an error.

- [ ] **Step 3: Write data/icons/io.github.martikan.ChessTrainer.svg**

A scalable icon using the current colour scheme is not possible in an app icon, so this uses fixed colours on purpose — it must read on any desktop background.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">
  <rect x="4" y="4" width="56" height="56" rx="8" fill="#1b1e20"/>
  <g fill="#3daee9">
    <rect x="12" y="12" width="10" height="10"/>
    <rect x="32" y="12" width="10" height="10"/>
    <rect x="22" y="22" width="10" height="10"/>
    <rect x="42" y="22" width="10" height="10"/>
    <rect x="12" y="32" width="10" height="10"/>
    <rect x="32" y="32" width="10" height="10"/>
    <rect x="22" y="42" width="10" height="10"/>
    <rect x="42" y="42" width="10" height="10"/>
  </g>
  <rect x="12" y="12" width="40" height="40" fill="none"
        stroke="#3daee9" stroke-width="1.5" opacity="0.5"/>
</svg>
```

- [ ] **Step 4: Write data/CMakeLists.txt**

```cmake
install(FILES io.github.martikan.ChessTrainer.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})

install(FILES io.github.martikan.ChessTrainer.metainfo.xml
        DESTINATION ${KDE_INSTALL_METAINFODIR})

install(FILES icons/io.github.martikan.ChessTrainer.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
```

- [ ] **Step 5: Add the data subdirectory**

```cmake
add_subdirectory(src/core)
add_subdirectory(src/store)
add_subdirectory(src/app)
add_subdirectory(data)
add_subdirectory(tests)
```

- [ ] **Step 6: Validate the metadata**

```bash
sudo dnf install -y libappstream-glib desktop-file-utils
desktop-file-validate data/io.github.martikan.ChessTrainer.desktop
appstreamcli validate data/io.github.martikan.ChessTrainer.metainfo.xml
```

Expected: `desktop-file-validate` prints nothing, which means it passed. `appstreamcli` reports success with a warning about missing screenshots. Any **error** must be fixed before committing.

- [ ] **Step 7: Verify a full install into a staging prefix**

```bash
cmake -B build-install -G Ninja -DCMAKE_INSTALL_PREFIX=/tmp/ct-prefix
cmake --build build-install
cmake --install build-install
find /tmp/ct-prefix -type f | sort
```

Expected exactly these four paths, modulo `lib64` naming:

```
/tmp/ct-prefix/bin/chess-trainer
/tmp/ct-prefix/share/applications/io.github.martikan.ChessTrainer.desktop
/tmp/ct-prefix/share/icons/hicolor/scalable/apps/io.github.martikan.ChessTrainer.svg
/tmp/ct-prefix/share/metainfo/io.github.martikan.ChessTrainer.metainfo.xml
```

Then clean up: `rm -rf /tmp/ct-prefix build-install`.

- [ ] **Step 8: Run the full suite one last time**

```bash
ctest --test-dir build --output-on-failure
```

Expected: seven test executables, `100% tests passed`.

- [ ] **Step 9: Commit and push**

```bash
git add CMakeLists.txt data
git commit --no-gpg-sign -m "feat: add desktop entry, AppStream metadata and icon

Installs under the application ID io.github.martikan.ChessTrainer, which is
the standard Flathub identifier for a project with no domain of its own.

No screenshots in the metainfo yet: Flathub requires hosted URLs, which
belong to the packaging phase. appstreamcli reports that as a warning rather
than an error, so validation still passes."
git push
```

- [ ] **Step 10: Confirm CI is green**

```bash
gh run watch --repo martikan/chess-trainer
```

Expected: `build-and-test` passes and Codecov reports coverage for `src/core` and `src/store` only.

---

## Definition of done

- `ctest` reports seven passing test executables.
- The application launches from the toolbox, follows the system Breeze colour scheme in both light and dark, and its About page shows version 0.1.0 with the MIT licence.
- A full 30-second round can be played with keyboard or mouse, a held key answers exactly once, and the summary reports counts, accuracy, mean response time, slowest squares and best-ever.
- Best score survives a restart, and aborted rounds never become the record.
- Corrupting the database file produces a warning banner and a working application; a read-only data directory produces an error banner and a still-playable drill.
- CI is green and Codecov shows coverage scoped to `src/core` and `src/store`.
- `desktop-file-validate` and `appstreamcli validate` pass, and a staging install produces exactly the four expected files.

## Deferred, by design

Settings UI, statistics and heatmap screens, module two and the shared abstraction it will justify, adaptive prompt weighting, the Flatpak manifest and Flathub submission, screenshots in the metainfo, translations, and any QML test suite.
