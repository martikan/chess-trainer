# Square Color Trainer — Design

Date: 2026-09-09
Status: approved, ready for implementation planning
Repository: https://github.com/martikan/chess-trainer
Application ID: `io.github.martikan.ChessTrainer`

## 1. Purpose

A chess board-vision trainer for Linux, in the spirit of chessvisiontrainer.com.
Step one ships a single training module — **Square Color** — inside an
application shell that can host further modules without restructuring.

The Square Color drill answers one question repeatedly: given a square name
like `e4`, is that square light or dark? Speed matters as much as accuracy, so
the drill is a timed sprint and the application records per-answer response
times.

### Goals

- One complete, polished training module, not a scaffold.
- A home screen that presents modules and can grow to a dozen of them.
- Per-answer data capture from day one, so later modules can build heatmaps
  and adaptive prompt selection without a data reset.
- Native KDE Plasma look and behaviour, following the system colour scheme.
- Ready to be packaged for Flathub in a later phase.

### Non-goals for this phase

- Any second training module.
- Settings UI (round length, keybindings, theme override).
- Statistics screen, heatmap, or trend charts.
- Board rendering or piece graphics.
- Windows/macOS builds, Snap packaging, translated locales.

## 2. Decisions

| Area | Decision |
|---|---|
| Drill format | Timed sprint, no board shown; coordinate text only |
| Round length | 30 s default, as a named constant |
| Shell | Home screen with module list + one live module |
| Persistence | SQLite, every answer recorded |
| Stats UI | Post-round summary only |
| Input | Keyboard and mouse both |
| Language | C++20 |
| UI framework | Qt 6 Quick + KDE Kirigami + `kf6-qqc2-desktop-style` |
| Theme | Breeze / system colour scheme via `Kirigami.Theme` |
| Drill chrome | Immersive — toolbar hidden for the duration of a round |
| Home layout | Cards list, one card per module |
| Deployment target | Flatpak on `org.kde.Platform`, Flathub, later phase |

## 3. Architecture

Three layers with hard boundaries:

```
src/core/     pure C++20 — no Qt whatsoever
src/store/    SQLite persistence — QtSql
src/app/      QObject bridges exposed to QML — QtCore/QtQml
qml/          Kirigami UI — bindings only, no logic
```

The domain logic — the colour rule, the prompt sequence, the round state
machine, scoring — is the only part of this application that can be
*incorrect*. Keeping it free of Qt means its tests need no event loop and no
display, which matters because the development shell runs inside a container.

That boundary is also the coverage boundary (§9): `core` and `store` are
measured, `app` and `qml` are not.

### 3.1 Source layout

```
chess-trainer/
  CMakeLists.txt
  src/
    core/
      Square.h/.cpp             value type: file, rank, algebraic, isDark()
      PromptGenerator.h/.cpp    seedable RNG, never repeats consecutively
      Clock.h                   IClock + MonotonicClock + FakeClock
      SquareColorRound.h/.cpp   round state machine and scoring
      ModuleDescriptor.h        id, name, description, icon, qmlPage, enabled
      ModuleRegistry.h/.cpp     static list of descriptors
    store/
      Database.h/.cpp           open + migrate via PRAGMA user_version
      RunRepository.h/.cpp      writeRun(), moduleSummary()
    app/
      main.cpp                  QApplication, KAboutData, QQmlApplicationEngine
      SquareColorController.h/.cpp
      ModuleListModel.h/.cpp
  qml/
    main.qml                    Kirigami.ApplicationWindow, globalDrawer, pageStack
    HomePage.qml                Kirigami.ScrollablePage + cards list
    DrillPage.qml               immersive drill
    SummarySheet.qml            post-round result card
    components/
      CountdownRing.qml
      AnswerButton.qml
  tests/
    tst_square.cpp
    tst_promptgenerator.cpp
    tst_round.cpp
    tst_repository.cpp
  data/
    io.github.martikan.ChessTrainer.desktop
    io.github.martikan.ChessTrainer.metainfo.xml
    icons/io.github.martikan.ChessTrainer.svg
  docs/superpowers/specs/
  .github/workflows/ci.yml
  .codecov.yml
```

### 3.2 No module base class

Module two needs exactly two things: a row on the home list, and a page of its
own. `ModuleDescriptor` plus a `qmlPage` URL covers both:

```cpp
struct ModuleDescriptor {
    std::string id;            // "square-color"
    std::string displayName;   // "Square Color"
    std::string description;   // shown on the home card
    std::string iconName;      // freedesktop icon name or bundled asset
    std::string qmlPage;       // "qrc:/qml/DrillPage.qml"
    bool enabled;              // false renders a "soon" placeholder card
};
```

An abstract `TrainingModule` interface designed against a single
implementation would guess wrong about what modules actually share. It gets
introduced when module two demonstrates the real commonality.

## 4. Core domain

### 4.1 Square

```cpp
class Square {
public:
    static std::optional<Square> fromAlgebraic(std::string_view text);
    Square(int file, int rank);          // both 0-based, 0..7

    int file() const;
    int rank() const;
    std::string algebraic() const;       // "e4"

    bool isDark() const { return (file_ + rank_) % 2 == 0; }
};
```

The colour rule: a square is dark when `(file + rank)` is even with both
indices 0-based. Checks: `a1` = (0,0) → dark; `h1` = (7,0) → light;
`a8` = (0,7) → light; `h8` = (7,7) → dark. All four corners match a real board.

`fromAlgebraic` returns `std::nullopt` for anything malformed — `i9`, `e0`,
`""`, `e44`, uppercase is accepted and normalised.

### 4.2 PromptGenerator

```cpp
class PromptGenerator {
public:
    explicit PromptGenerator(std::uint64_t seed);
    Square next();                       // uniform over 64, never == previous
private:
    std::mt19937_64 rng_;
    std::optional<Square> previous_;
};
```

Uniform across all 64 squares, rejecting only an immediate repeat. Seeded
construction makes the sequence deterministic, so round tests assert exact
prompts.

### 4.3 Clock

```cpp
class IClock {
public:
    virtual ~IClock() = default;
    virtual std::chrono::steady_clock::time_point now() const = 0;
};

class MonotonicClock final : public IClock { /* steady_clock::now() */ };

class FakeClock final : public IClock {
public:
    void advance(std::chrono::milliseconds delta);
};
```

A steady clock, never a wall clock: a system time adjustment mid-round must not
change the countdown. `FakeClock` lets a test drive a full 30-second round in
microseconds — no waiting, no timing flake.

### 4.4 SquareColorRound

```cpp
enum class RoundState { Idle, Running, Finished, Aborted };
enum class AnswerOutcome { Correct, Wrong, Ignored };

struct Answer {
    int ordinal;                              // 1-based within the run
    Square square;
    bool expectedDark;
    bool answeredDark;
    bool correct;
    std::chrono::milliseconds responseTime;
};

class SquareColorRound {
public:
    static constexpr std::chrono::milliseconds kDefaultRoundLength{30'000};

    SquareColorRound(const IClock& clock,
                     PromptGenerator generator,
                     std::chrono::milliseconds roundLength);

    void start();
    void abort();

    RoundState state() const;
    Square currentPrompt() const;                    // precondition: Running
    std::chrono::milliseconds remaining() const;     // clamped at zero

    bool tick();                                     // true if it just finished
    AnswerOutcome answer(bool answeredDark);

    int correct() const;
    int wrong() const;
    const std::vector<Answer>& answers() const;
};
```

Response time is measured from the moment a prompt becomes current to the
moment its answer arrives. `answer()` records the result and advances to the
next prompt synchronously, so the next coordinate is on screen before any
feedback animation finishes.

`answer()` returns `Ignored` and records nothing when the round is not in
`Running` state — which covers input arriving after the clock has expired.

`kDefaultRoundLength` is the single place round length is defined; a settings
screen later reads and overrides it.

## 5. Persistence

### 5.1 Schema (`user_version = 1`)

```sql
PRAGMA user_version = 1;

CREATE TABLE run (
  id              INTEGER PRIMARY KEY,
  module_id       TEXT    NOT NULL,
  started_at      TEXT    NOT NULL,   -- ISO-8601 UTC
  round_length_ms INTEGER NOT NULL,   -- configured length, e.g. 30000
  correct         INTEGER NOT NULL,
  wrong           INTEGER NOT NULL,
  completed       INTEGER NOT NULL    -- 0 when aborted with Esc
);

CREATE TABLE answer (
  id            INTEGER PRIMARY KEY,
  run_id        INTEGER NOT NULL REFERENCES run(id) ON DELETE CASCADE,
  ordinal       INTEGER NOT NULL,
  square        TEXT    NOT NULL,     -- 'e4'
  expected_dark INTEGER NOT NULL,
  answered_dark INTEGER NOT NULL,
  correct       INTEGER NOT NULL,
  response_ms   INTEGER NOT NULL
);

CREATE INDEX idx_answer_run    ON answer(run_id);
CREATE INDEX idx_answer_square ON answer(square);
```

`PRAGMA foreign_keys = ON` is set on every connection, otherwise SQLite
ignores the cascade.

Three decisions worth recording:

**Writes happen once, at round end, in a single transaction.** Answers
accumulate in a `std::vector` during the round. No disk I/O while the user is
being timed — an fsync stall mid-drill would corrupt the very response-time
data being collected.

**Aborted rounds are stored with `completed = 0`.** Their answers still feed
per-square statistics, because that data is real. Best-score and
accuracy-trend queries filter `completed = 1`, so a three-second rage-quit
cannot become a personal record.

**`expected_dark` is stored although derivable from `square`.** It costs one
byte per row and makes every statistics query a plain aggregate with no colour
logic in SQL. It also keeps old rows meaningful if the prompt format changes.

The column is named `round_length_ms`, not `duration_ms`, because for an
aborted run it describes the configured length rather than how long the round
actually lasted.

### 5.2 Interface

```cpp
class Database {
public:
    bool open(const QString& path, QString* errorOut);
    bool migrate(QString* errorOut);     // applies the user_version ladder
    QSqlDatabase& handle();
};

struct RunRecord {
    QString moduleId;
    QDateTime startedAtUtc;
    int roundLengthMs;
    int correct;
    int wrong;
    bool completed;
    std::vector<Answer> answers;
};

struct ModuleSummary {
    std::optional<int> bestScore;        // nullopt when no completed run exists
    std::optional<int> meanResponseMs;
};

class RunRepository {
public:
    explicit RunRepository(Database& db);
    std::optional<qint64> writeRun(const RunRecord& record, QString* errorOut);
    std::optional<ModuleSummary> moduleSummary(const QString& moduleId,
                                               QString* errorOut);
};
```

`moduleSummary` backs the home card's "best 26 · avg 780 ms" line with two
queries over completed runs only:

```sql
SELECT MAX(correct) FROM run
 WHERE module_id = ? AND completed = 1;

SELECT AVG(a.response_ms) FROM answer a
  JOIN run r ON a.run_id = r.id
 WHERE r.module_id = ? AND r.completed = 1;
```

The mean is taken over *every* answer in completed runs, correct and wrong
alike — it reports how fast the user responds, not how fast they are when they
happen to be right. Both values are `nullopt` before any round has been
completed, and the card then shows no statistics line at all.

### 5.3 Location and migrations

`QStandardPaths::AppDataLocation` → `~/.local/share/chess-trainer/trainer.db`.
Under Flatpak this resolves inside the sandbox automatically, with no code
change.

Migrations run on open, driven by `PRAGMA user_version`. Version 1 is the
schema above. The ladder exists from the first release so that module two can
extend the schema without a data reset.

## 6. Application layer

`SquareColorController` is a `QObject` owning a `SquareColorRound` and exposing
it to QML:

- Properties: `remainingMs`, `promptText`, `correct`, `wrong`, `running`.
- `Q_INVOKABLE void startRound()`, `void answer(bool dark)`, `void abort()`.
- A 16 ms `QTimer` calls `tick()` and drives the countdown property.
- On finish or abort it builds a `RunRecord` and calls `RunRepository::writeRun`
  once, then exposes summary values for the sheet.

Summary values are computed from the in-memory answer vector, not read back
from the database: correct, wrong, accuracy, mean response time over all
answers in the run, and the three slowest squares. "Slowest three" means: group
the run's answers by square, take each square's worst response time, sort
descending, keep the first three. Best-ever comes from `moduleSummary`.

`ModuleListModel` is a `QAbstractListModel` over `ModuleRegistry`, with roles
for name, description, icon, page URL, enabled flag, best score, and mean
response time. The last two come from `moduleSummary` and are refreshed when a
round finishes.

## 7. User interface

Kirigami on the system colour scheme. Every colour comes from
`Kirigami.Theme` and every spacing from `Kirigami.Units`, so Breeze light and
dark both work with no palette of our own. `kf6-qqc2-desktop-style` gives
native Breeze controls.

`main.qml` is a `Kirigami.ApplicationWindow` with a global drawer holding About
and (later) Settings, keeping app-level actions out of the module area.

### 7.1 Home page

`Kirigami.ScrollablePage` containing a cards list, one card per module. A card
shows icon, module name, a one-line description, a Start button, and its best
score with mean response time. Disabled modules render dimmed with a "soon"
badge and no Start button.

### 7.2 Drill page — immersive

The drill hides the Kirigami toolbar on entry and restores it on exit, so the
summary returns with normal chrome. Layout, top to bottom: countdown ring with
seconds remaining, the prompt coordinate at display size, a compact
correct/wrong tally, then two full-width answer buttons, then a dim `Esc end
round` hint.

The countdown ring binds to `remainingMs` with a `Behavior` for a smooth
sweep. The animation is cosmetic; the authoritative clock is in C++.

### 7.3 Input

- `Left` or `L` — light
- `Right` or `D` — dark
- `Esc` — abort the round
- Mouse click on either answer button

Three rules keep the response-time data honest:

**Auto-repeat is dropped.** A held-down arrow key must not spray answers, so
events with `event.isAutoRepeat` are ignored.

**One interaction produces one answer.** Keys are handled at page level and the
answer buttons are non-focusable, so `Space`/`Enter` on a focused button cannot
fire alongside the key handler. Mouse and keyboard both route through the same
`controller.answer(bool)` entry point.

**There is no minimum response time.** Genuinely fast answers around 250 ms are
real and must count; a debounce floor would silently discard the user's best
work. Input arriving after expiry is rejected by round state, not by a timer
threshold.

### 7.4 Feedback and summary

On an answer, the pressed button tints for 110 ms using
`Kirigami.Theme.positiveTextColor` or `negativeTextColor`. The next prompt is
already displayed by then — the flash overlaps the next question rather than
delaying it. Nothing in the feedback path costs drill time.

When the clock expires, the summary sheet shows correct, wrong, accuracy, mean
response time, the three slowest squares, and best-ever, with Again and Home
actions.

## 8. Testing

QTest driven by CTest. Tests are written before the implementation they cover.

**`tst_square`** — colour assertions for all 64 squares including the four
corners; algebraic round-trip; `fromAlgebraic` rejects `i9`, `e0`, `""`,
`e44`; uppercase input normalises.

**`tst_promptgenerator`** — never returns the same square twice in a row across
a long run; a fixed seed reproduces an exact sequence; all 64 squares appear
across enough draws.

**`tst_round`** — `FakeClock` drives a whole round in microseconds. Covers:
expiry moves state to `Finished`; response times recorded per answer; scoring;
`abort()` moves to `Aborted`; input after expiry returns `Ignored` and records
nothing; `answer()` advances the prompt.

**`tst_repository`** — SQLite `:memory:`. Covers migration from an empty
database; write-then-read round-trip preserving every answer field;
`moduleSummary()` ignoring `completed = 0` rows and returning `nullopt` on an
empty database; cascade delete removing a run's answers.

**No QML tests in this phase**, deliberately. `qmltestrunner` needs a display,
and every rule that can be wrong lives in Qt-free C++. QML holds bindings only.

## 9. Build

### 9.1 Dependencies

```
sudo dnf install cmake ninja-build gcc-c++ extra-cmake-modules \
  qt6-qtbase-devel qt6-qtdeclarative-devel \
  kf6-kirigami-devel kf6-qqc2-desktop-style \
  kf6-kcoreaddons-devel kf6-ki18n-devel
```

`kcoreaddons` provides `KAboutData`; `ki18n` provides `i18n()`. Wrapping user
strings in `i18n()` from the first commit costs nothing now and is miserable to
retrofit, even though no translations ship in this phase.

### 9.2 CMake

CMake 3.28+, C++20, Extra CMake Modules for the KDE install layout.

- `find_package(Qt6 6.6 REQUIRED COMPONENTS Core Quick QuickControls2 Sql Test)`
- `find_package(KF6 6.5 REQUIRED COMPONENTS Kirigami CoreAddons I18n)`
- Targets: `chesstrainer-core` (static), `chesstrainer-store` (static),
  `chess-trainer` (executable), and one test executable per `tst_*.cpp`.
- QML registered with `qt_add_qml_module`.
- `chesstrainer-core` links no Qt libraries — enforced by review, and visible
  in `CMakeLists.txt` as an absent dependency.

Install paths:

```
bin/chess-trainer
share/applications/io.github.martikan.ChessTrainer.desktop
share/metainfo/io.github.martikan.ChessTrainer.metainfo.xml
share/icons/hicolor/scalable/apps/io.github.martikan.ChessTrainer.svg
```

The application runs from the development container against the host's
passed-through Wayland socket.

### 9.3 CI and coverage

`.github/workflows/ci.yml`, on push and pull request, in a `fedora:43`
container so CI and local builds share one dependency set:

1. Install the package set above plus `lcov`.
2. Configure with `-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON` and
   `--coverage -O0 -g` in `CMAKE_CXX_FLAGS` and the linker flags.
3. Build, then `ctest --output-on-failure`.
4. Upload with `codecov/codecov-action@v5`.

`.codecov.yml` ignores `src/app/**`, `qml/**`, and `tests/**`. Untested-by-design
UI glue would otherwise drag the number down and make the metric meaningless.
Coverage therefore measures exactly the two layers that hold logic.

The workflow needs a `CODECOV_TOKEN` repository secret. Public repositories can
sometimes upload tokenless, but it is unreliable enough to be worth pinning.

## 10. Error handling

Storage failure must never block training.

**Database will not open** — the drill runs normally, persistence is disabled
for the session, and the home page shows a non-blocking
`Kirigami.InlineMessage`.

**Database corrupt or migration failed** — the file is moved aside to
`trainer.db.bak`, a fresh database is created, and the same inline message
explains what happened.

**Kirigami missing at runtime** — fail immediately with an explicit message.
That is a packaging defect, not a user error, and silently degrading the UI
would hide it.

Malformed input cannot reach the core: prompts are generated internally and
answers are booleans.

## 11. Packaging (later phase)

Prepared for now, built later. A Flatpak manifest on the `org.kde.Platform`
runtime — which already contains Kirigami and `qqc2-desktop-style`, so nothing
needs bundling — plus the AppStream metainfo, desktop entry, and icon installed
under `io.github.martikan.ChessTrainer`. `io.github.<user>` is the standard
Flathub identifier for a project with no domain of its own.

One caveat on store reach: **KDE Discover picks up Flathub cleanly, Ubuntu's
Software Center does not.** Ubuntu ships Snap-first and does not enable Flathub
by default. Flatpak therefore covers Fedora, KDE, and most distributions, while
Ubuntu's own store would require a separate Snap package later. A single package
serving both stores is not achievable.

## 12. Future work

In rough order: a settings screen backed by `kDefaultRoundLength`; a statistics
screen using the per-square data already being collected (heatmap and trend);
module two, at which point the shared `TrainingModule` abstraction earns its
place; adaptive prompt weighting toward weak squares; Flatpak and Flathub
submission; translations.
