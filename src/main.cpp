#include "game.h"
#include "characters.h"
#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

using turnforge::Game;
using turnforge::Phase;
using turnforge::Position;

namespace {
constexpr int Width = 1280;
constexpr int Height = 720;
constexpr Rectangle Board{72, 128, 512, 512};
constexpr Rectangle StartButton{72, 558, 370, 62};
constexpr Rectangle GuardButton{728, 378, 224, 54};
constexpr Rectangle EndButton{976, 378, 224, 54};
constexpr Rectangle RestartButton{420, 426, 440, 60};
const Color Background{12, 20, 30, 255};
const Color Panel{22, 33, 46, 255};
const Color Border{47, 63, 80, 255};
const Color Text{232, 237, 239, 255};
const Color Muted{148, 165, 181, 255};
const Color Teal{110, 220, 191, 255};
const Color Orange{245, 170, 108, 255};
const Color Gold{237, 207, 137, 255};

void text(const std::string& value, int x, int y, int size, Color color = Text) {
    DrawText(value.c_str(), x, y, size, color);
}
void centered(const std::string& value, Rectangle r, int size, Color color = Text) {
    text(value, static_cast<int>(r.x + (r.width - MeasureText(value.c_str(), size)) / 2),
         static_cast<int>(r.y + (r.height - size) / 2), size, color);
}
void paragraph(const std::string& value, int x, int y, int width, int size, Color color, int maxLines = 2) {
    std::string line;
    std::size_t start = 0;
    int lines = 0;
    while (start < value.size() && lines < maxLines) {
        auto end = value.find(' ', start);
        if (end == std::string::npos) end = value.size();
        const auto word = value.substr(start, end - start);
        const auto candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureText(candidate.c_str(), size) > width) {
            text(line, x, y + lines * (size + 5), size, color);
            ++lines;
            line = word;
        } else line = candidate;
        start = end + 1;
    }
    if (!line.empty() && lines < maxLines) text(line, x, y + lines * (size + 5), size, color);
}
void card(Rectangle r, Color fill = Panel) {
    DrawRectangleRounded(r, 0.09f, 6, fill);
    DrawRectangleRoundedLinesEx(r, 0.09f, 6, 1, Border);
}
bool hovered(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }
void button(const std::string& label, Rectangle r, bool enabled = true) {
    const Color fill = enabled ? (hovered(r) ? Color{143, 237, 210, 255} : Teal) : Border;
    DrawRectangleRounded(r, 0.12f, 8, fill);
    centered(label, r, 20, enabled ? Background : Muted);
}
Position tileAt(Vector2 mouse) {
    if (mouse.x < Board.x || mouse.y < Board.y ||
        mouse.x >= Board.x + Board.width || mouse.y >= Board.y + Board.height) return {-1, -1};
    return {static_cast<int>((mouse.x - Board.x) / 64), static_cast<int>((mouse.y - Board.y) / 64)};
}
Vector2 tileCenter(Position p, Rectangle bounds) {
    const float tile = bounds.width / 8;
    return {bounds.x + (p.x + 0.5f) * tile, bounds.y + (p.y + 0.5f) * tile};
}
void board(const Game& g, const Characters& characters, Rectangle bounds, bool interactive) {
    const float tile = bounds.width / 8;
    const auto range = g.movementDistances();
    const auto hover = interactive ? tileAt(GetMousePosition()) : Position{-1, -1};
    DrawRectangleRec({bounds.x - 8, bounds.y - 8, bounds.width + 16, bounds.height + 16}, Panel);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const Position p{x, y};
            Rectangle r{bounds.x + x * tile + 2, bounds.y + y * tile + 2, tile - 4, tile - 4};
            const bool wall = g.wallAt(p);
            const Color floor = (x + y) % 2 ? Color{39, 49, 56, 255} : Color{34, 44, 51, 255};
            DrawRectangleRec(r, wall ? Color{93, 98, 100, 255} : floor);
            if (wall) {
                DrawLineEx({r.x + 6, r.y + 8}, {r.x + r.width - 6, r.y + 8}, 3, {119, 130, 140, 255});
                DrawLineEx({r.x + 6, r.y + r.height - 7}, {r.x + r.width - 6, r.y + r.height - 7}, 3, Border);
                DrawLineEx({r.x + 4, r.y + r.height / 2}, {r.x + r.width - 4, r.y + r.height / 2}, 2, Border);
                DrawLineEx({r.x + r.width / 2, r.y + 11}, {r.x + r.width / 2, r.y + r.height / 2}, 2, Border);
            } else if (interactive && range[y * 8 + x] > 0 && range[y * 8 + x] <= 3) {
                DrawCircleV(tileCenter(p, bounds), 4, {86, 148, 139, 255});
            }
            if (interactive && hover == p && g.phase == Phase::PlayerTurn)
                DrawRectangleLinesEx(r, 2, Gold);
        }
    }
    if (interactive && turnforge::inBounds(hover) && g.canMove()) {
        const auto route = g.path(g.player.position, hover);
        if (route.size() > 1 && route.size() <= 4)
            for (std::size_t i = 1; i < route.size(); ++i)
                DrawLineEx(tileCenter(route[i - 1], bounds), tileCenter(route[i], bounds), 2, Fade(Teal, 0.65f));
    }
    characters.draw(g, bounds, interactive);
    if (interactive) {
        for (int i = 0; i < 8; ++i) {
            text(std::string(1, static_cast<char>('A' + i)), static_cast<int>(bounds.x + (i + 0.5f) * tile - 5), 105, 14, Muted);
            text(std::to_string(i + 1), 48, static_cast<int>(bounds.y + (i + 0.5f) * tile - 7), 14, Muted);
        }
    }
}
void title(const Game& g, const Characters& characters) {
    text("TACTICAL 2D / TURN-BASED", 74, 80, 18, Teal);
    text("TURNFORGE", 68, 126, 64);
    text("Setiap langkah", 72, 222, 36);
    text("punya konsekuensi.", 72, 266, 36, Muted);
    const std::vector<std::string> rules{
        "01   Bergerak sampai tiga petak.",
        "02   Klik musuh untuk serang; G untuk bertahan.",
        "03   Kalahkan keduanya. Kuasai arena."};
    for (int i = 0; i < 3; ++i) {
        card({72, 342.0f + i * 56, 560, 44});
        text(rules[i], 88, 356 + i * 56, 18);
    }
    button("MULAI PERTANDINGAN  [ENTER]", StartButton);
    text("Single player  /  Offline  /  Pertarungan taktis", 72, 642, 16, Muted);
    board(g, characters, {744, 164, 416, 416}, false);
    text("Goblin mengejar. Bandit mencari sisi lain.", 744, 612, 16, Muted);
}
void hud(const Game& g, const Characters& characters) {
    card({700, 112, 528, 568});
    const bool ours = g.phase == Phase::PlayerTurn;
    text(ours ? "GILIRANMU" : g.phase == Phase::EnemyTurn ? "GILIRAN MUSUH" : "PERTANDINGAN SELESAI", 728, 138, 28, ours ? Teal : Orange);
    text(g.guarding ? "PERISAI AKTIF: damage musuh 2 -> 1." : ours ? (characters.busy() ? "Aksi berlangsung..." : "Pilih posisi. Tentukan serangan.") : "Perhatikan urutan dan posisi lawan.", 728, 177, 16, g.guarding ? Color{135, 202, 250, 255} : Muted);
    text("KESATRIA", 728, 216, 14, Muted);
    text(std::to_string(g.player.hp) + " / 12 HP", 728, 242, 28, Teal);
    for (int i = 0; i < 2; ++i) {
        const auto& e = g.enemies[i];
        const int row = 208 + i * 38;
        text(std::string(i == 0 ? "E1 GOBLIN " : "E2 BANDIT ") + (e.alive() ? std::to_string(e.hp) + "/6 HP" : "TUMBANG"), 976, row, 16, e.alive() ? Orange : Muted);
        text(i == 0 ? "AGRESIF / KEJAR LANGSUNG" : "PENGEPUNG / CARI SISI LAIN", 976, row + 20, 10, Muted);
    }
    card({728, 294, 224, 45}, {29, 44, 59, 255});
    card({976, 294, 224, 45}, {29, 44, 59, 255});
    text(std::string("GERAK  ") + (ours && !g.moved && !g.attacked ? "SIAP" : "--"), 746, 309, 16, ours && g.canMove() ? Teal : Muted);
    text(std::string("SERANG  ") + (ours && !g.attacked ? "SIAP" : "--"), 994, 309, 16, ours && !g.attacked ? Teal : Muted);
    text("Bertahan memakai jatah serang dan gerak.", 728, 351, 16, Muted);
    button(g.guarding ? "PERISAI AKTIF" : "BERTAHAN [G]", GuardButton, g.canGuard() && !characters.busy());
    button("AKHIRI [SPACE]", EndButton, ours && !characters.busy());
    text("CATATAN ARENA", 728, 459, 14, Muted);
    for (std::size_t i = 0; i < g.log.size(); ++i) {
        DrawCircle(732, 495 + static_cast<int>(i) * 24, 2, i + 1 == g.log.size() ? Teal : Muted);
        text(g.log[i], 745, 487 + static_cast<int>(i) * 24, 16, i + 1 == g.log.size() ? Text : Muted);
    }
    DrawLine(728, 617, 1200, 617, Border);
    paragraph(g.message, 728, 633, 472, 16, Gold);
}
void result(const Game& g) {
    DrawRectangle(0, 0, Width, Height, Fade(Background, 0.86f));
    card({300, 170, 680, 384});
    const bool won = g.phase == Phase::Won;
    centered(won ? "ARENA DIKUASAI" : "UNITMU TUMBANG", {320, 210, 640, 60}, 38, won ? Teal : Orange);
    centered(won ? "Dua musuh tumbang. Strategimu berhasil." : "Pelajari posisi lawan, lalu coba lagi.", {320, 282, 640, 40}, 20, Muted);
    centered("RONDE " + std::to_string(g.round) + "   /   HP TERSISA " + std::to_string(g.player.hp), {320, 344, 640, 40}, 20);
    button("MAIN LAGI  [R]", RestartButton);
    centered("[ESC] Kembali ke layar awal", {320, 500, 640, 28}, 16, Muted);
}
void draw(const Game& g, const Characters& characters) {
    ClearBackground(Background);
    if (g.phase == Phase::Title) { title(g, characters); return; }
    text("TURNFORGE", 68, 28, 32);
    text("ARENA 01  /  LATIHAN TAKTIK", 72, 75, 14, Muted);
    text("RONDE " + std::to_string(g.round), 1056, 34, 20, Teal);
    board(g, characters, Board, true);
    hud(g, characters);
    text("KLIK: GERAK / SERANG", 72, 670, 14, Muted);
    text("[ESC] MENU", 466, 670, 14, Muted);
    if ((g.phase == Phase::Won || g.phase == Phase::Lost) && !characters.busy()) result(g);
}
void input(Game& g, float& enemyTimer, const Characters& characters) {
    if (IsKeyPressed(KEY_ESCAPE)) { g.toTitle(); enemyTimer = 0; return; }
    if (characters.busy()) return;
    const bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (g.phase == Phase::Title) {
        if (IsKeyPressed(KEY_ENTER) || (click && hovered(StartButton))) { g.start(); enemyTimer = 0; }
    } else if (g.phase == Phase::Won || g.phase == Phase::Lost) {
        if (IsKeyPressed(KEY_R) || (click && hovered(RestartButton))) { g.start(); enemyTimer = 0; }
    } else if (g.phase == Phase::PlayerTurn) {
        if (IsKeyPressed(KEY_SPACE) || (click && hovered(EndButton))) {
            g.endTurn(); enemyTimer = 0;
        } else if (IsKeyPressed(KEY_G) || (click && hovered(GuardButton))) {
            g.tryGuard();
        } else if (click) {
            const auto target = tileAt(GetMousePosition());
            if (turnforge::inBounds(target)) {
                const int enemy = g.enemyAt(target);
                if (enemy >= 0) g.tryAttack(enemy);
                else g.tryMove(target);
            }
        }
    }
}
void resolve(Game& g) {
    while (g.phase == Phase::EnemyTurn) g.stepEnemy();
}
bool prepareCombat(Game& g) {
    g.start();
    if (!g.tryMove({4, 6})) return false;
    for (int turn = 0; turn < 6 && g.phase == Phase::PlayerTurn && !g.canAttack(0) && !g.canAttack(1); ++turn) {
        g.endTurn(); resolve(g);
    }
    return g.phase == Phase::PlayerTurn && (g.canAttack(0) || g.canAttack(1));
}
bool demoVictory(Game& g) {
    g.start();
    if (!g.tryMove({4, 6})) return false;
    for (int i = 0; i < 12 && g.phase == Phase::PlayerTurn; ++i) {
        int target = -1;
        for (int e = 0; e < 2; ++e)
            if (g.canAttack(e) && (target == -1 || g.enemies[e].hp < g.enemies[target].hp)) target = e;
        if (target != -1) g.tryAttack(target);
        if (g.phase == Phase::Won) break;
        g.endTurn(); resolve(g);
    }
    return g.phase == Phase::Won;
}
bool demoDefeat(Game& g) {
    g.start();
    for (int i = 0; i < 30 && g.phase == Phase::PlayerTurn; ++i) { g.endTurn(); resolve(g); }
    return g.phase == Phase::Lost;
}
bool inputSelfCheck(const Characters& characters) {
    Game game;
    Characters probe = characters;
    probe.update(game, 0);
    float timer = 1;
    auto settle = [&]() { for (int i = 0; i < 40; ++i) probe.update(game, 0.03f); };
    // Event IDs are defined by the pinned raylib 5.5 rcore.c automation format.
    constexpr unsigned int KeyUp = 1, KeyDown = 2, MouseUp = 5, MouseDown = 6, MousePosition = 7;
    auto key = [&](int code) {
        PlayAutomationEvent({0, KeyDown, {code, 0, 0, 0}});
        input(game, timer, probe);
        probe.update(game, 0);
        PlayAutomationEvent({0, KeyUp, {code, 0, 0, 0}});
        PollInputEvents();
    };
    auto click = [&](int x, int y) {
        PlayAutomationEvent({0, MousePosition, {x, y, 0, 0}});
        PlayAutomationEvent({0, MouseDown, {MOUSE_BUTTON_LEFT, 0, 0, 0}});
        input(game, timer, probe);
        probe.update(game, 0);
        PlayAutomationEvent({0, MouseUp, {MOUSE_BUTTON_LEFT, 0, 0, 0}});
        PollInputEvents();
    };
    auto tile = [&](Position p) {
        const auto center = tileCenter(p, Board);
        click(static_cast<int>(center.x), static_cast<int>(center.y));
    };
    auto check = [](bool passed, const char* name) {
        if (!passed) throw std::runtime_error(name);
        std::cout << "PASS input: " << name << '\n';
    };
    try {
        key(KEY_G); key(KEY_SPACE); key(KEY_R); tile({2, 6});
        check(game.phase == Phase::Title, "title rejects arena and gameplay keys");
        key(KEY_ENTER);
        check(game.phase == Phase::PlayerTurn && game.canMove() && timer == 0, "Enter starts a match");
        for (Vector2 p : {Vector2{71, 160}, {584, 160}, {104, 127}, {104, 640}, {700, 200}})
            click(static_cast<int>(p.x), static_cast<int>(p.y));
        tile(game.player.position); tile({2, 2}); tile({5, 6}); tile(game.enemies[0].position);
        check(game.player.position == Position{1, 6} && !game.moved && !game.attacked,
              "outside, own, wall, distant tile and distant target clicks preserve actions");
        tile({2, 6}); probe.update(game, 0);
        check(game.player.position == Position{2, 6} && probe.busy(), "reachable click starts movement");
        key(KEY_G); key(KEY_SPACE); click(1080, 405);
        check(game.phase == Phase::PlayerTurn && !game.guarding && !game.attacked, "busy animation blocks gameplay input");
        key(KEY_ESCAPE); probe.update(game, 0);
        check(game.phase == Phase::Title && !probe.busy(), "Escape interrupts movement and clears presentation");
        click(250, 585); tile({2, 6}); probe.update(game, 0);
        for (int i = 0; i < 40; ++i) probe.update(game, 0.03f);
        click(840, 405);
        check(game.guarding && game.moved && game.attacked, "Start and Guard buttons allow move then guard");
        tile({3, 6}); key(KEY_G);
        check(game.player.position == Position{2, 6} && game.guarding, "guard blocks further moves and stacking");
        key(KEY_SPACE);
        check(game.phase == Phase::EnemyTurn && game.guarding && timer == 0, "Space ends turn and preserves protection");
        key(KEY_G); tile({3, 6}); click(1080, 405);
        check(game.phase == Phase::EnemyTurn && game.enemyIndex == 0 && game.player.position == Position{2, 6},
              "enemy phase rejects player input");
        key(KEY_ESCAPE); key(KEY_ENTER); key(KEY_G);
        check(game.phase == Phase::PlayerTurn && game.guarding, "G guards without moving");
        check(prepareCombat(game), "combat fixture is reachable");
        settle();
        const int enemy = game.canAttack(0) ? 0 : 1;
        const int hp = game.enemies[enemy].hp;
        tile(game.enemies[enemy].position); probe.update(game, 0);
        check(game.enemies[enemy].hp == hp - 3 && game.attacked && probe.busy(), "adjacent enemy click attacks once");
        tile(game.enemies[enemy].position); key(KEY_SPACE);
        check(game.enemies[enemy].hp == hp - 3 && game.phase == Phase::PlayerTurn, "attack animation rejects repeated click and end turn");
        for (int i = 0; i < 40; ++i) probe.update(game, 0.03f);
        key(KEY_G); click(1080, 405);
        check(!game.guarding && game.phase == Phase::EnemyTurn, "spent attack rejects Guard and End button advances turn");
        for (bool won : {true, false}) {
            check(won ? demoVictory(game) : demoDefeat(game), won ? "winning match fixture" : "losing match fixture");
            settle();
            const auto phase = game.phase;
            tile({2, 6}); key(KEY_G); key(KEY_SPACE);
            check(game.phase == phase, "result blocks arena and gameplay keys");
            key(KEY_R);
            check(game.phase == Phase::PlayerTurn && game.player.hp == 12 && game.round == 1 && !game.guarding,
                  "R restarts from result");
            if (won) demoVictory(game); else demoDefeat(game);
            settle(); click(640, 455);
            check(game.phase == Phase::PlayerTurn && game.validInitialLayout(), "result restart button restores match");
        }
        game.tryMove({4, 6}); settle(); key(KEY_R);
        check(game.player.position == Position{4, 6} && game.moved, "R cannot reset an active match");
        demoVictory(game); settle(); key(KEY_ESCAPE);
        check(game.phase == Phase::Title, "Escape returns from result");
    } catch (const std::exception& e) {
        std::cerr << "FAIL input: " << e.what() << '\n';
        return false;
    }
    return true;
}
} // namespace

int main(int argc, char** argv) {
    bool smoke = false;
    double benchmarkSeconds = 0;
    std::filesystem::path captureDirectory;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--smoke-test") smoke = true;
        else if (arg == "--capture-dir" && i + 1 < argc) captureDirectory = argv[++i];
        else if (arg == "--benchmark-seconds" && i + 1 < argc) {
            try {
                const std::string value = argv[++i];
                std::size_t parsed = 0;
                benchmarkSeconds = std::stod(value, &parsed);
                if (parsed != value.size()) throw std::invalid_argument("trailing characters");
            }
            catch (...) { std::cerr << "Invalid benchmark duration\n"; return 1; }
            if (!std::isfinite(benchmarkSeconds) || benchmarkSeconds <= 0 || benchmarkSeconds > 3600) return 1;
        } else { std::cerr << "Unknown or incomplete option: " << arg << '\n'; return 1; }
    }
    const bool automated = smoke || benchmarkSeconds > 0;
    try {
        if (!captureDirectory.empty()) std::filesystem::create_directories(captureDirectory);
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    SetConfigFlags(FLAG_MSAA_4X_HINT | (automated ? FLAG_WINDOW_HIDDEN : 0));
    SetTraceLogLevel(automated ? LOG_INFO : LOG_WARNING);
    InitWindow(Width, Height, "Turnforge | Tactical Arena");
    if (!IsWindowReady()) return 1;
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    Characters characters;
    const std::string atlasPath = (std::filesystem::path(GetApplicationDirectory()) / "assets" / "characters.png").string();
    if (!characters.load(atlasPath)) {
        std::cerr << "Unable to load complete character atlas: " << atlasPath << '\n';
        if (!automated) {
            while (!WindowShouldClose() && !IsKeyPressed(KEY_ESCAPE)) {
                BeginDrawing(); ClearBackground(Background);
                centered("Aset karakter tidak ditemukan.", {0, 250, Width, 80}, 30);
                centered("Ekstrak seluruh paket game, lalu jalankan kembali.", {0, 340, Width, 60}, 20, Muted);
                EndDrawing();
            }
        }
        characters.unload(); CloseWindow(); return 1;
    }
    Game game;
    if (!game.validInitialLayout()) { characters.unload(); CloseWindow(); return 1; }
    characters.update(game, 0);
    float enemyTimer = 0;
    double maxEnemyMs = 0;
    std::vector<double> frameTimes;
    int frames = 0;
    int restarts = 0;
    bool verifiedWin = false;
    bool verifiedLoss = false;
    bool verifiedGuard = false;
    int guardHpBefore = 0;
    int automatedTurns = 0;
    const bool animationChecks = !automated || characters.selfCheck();
    const bool inputChecks = !automated || inputSelfCheck(characters);
    bool valid = animationChecks && inputChecks && tileAt({71.9f, 128}) == Position{-1, -1} &&
        tileAt({72, 128}) == Position{0, 0} && tileAt({583.9f, 639.9f}) == Position{7, 7} &&
        tileAt({584, 200}) == Position{-1, -1} && tileAt({100, 640}) == Position{-1, -1} &&
        tileAt({728, 400}) == Position{-1, -1};
    const double start = GetTime();
    auto capture = [&](const std::string& name) {
        if (!captureDirectory.empty()) {
            // Capture the actual framebuffer, without reapplying Windows desktop DPI scaling.
            Image screenshot{rlReadScreenPixels(GetRenderWidth(), GetRenderHeight()), GetRenderWidth(), GetRenderHeight(), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            valid = screenshot.data && ExportImage(screenshot, (captureDirectory / (name + ".png")).string().c_str()) && valid;
            UnloadImage(screenshot);
        }
    };
    while (!WindowShouldClose()) {
        if (automated) {
            if (frames == 6) game.start();
            if (frames == 15) { game.tryMove({2, 6}); game.endTurn(); enemyTimer = 0; }
            if (frames == 40) {
                valid = prepareCombat(game) && valid;
            }
            if (frames == 54) {
                bool applied = false;
                for (int e = 0; e < 2; ++e) if (game.canAttack(e)) { applied = game.tryAttack(e); break; }
                valid = applied && valid;
            }
            if (frames == 75) { verifiedWin = demoVictory(game); valid = valid && verifiedWin; }
            if (frames == 120) { verifiedLoss = demoDefeat(game); valid = valid && verifiedLoss; }
            if (frames >= 160 && frames < 170) {
                game.start(); enemyTimer = 0; ++restarts;
                valid = valid && game.validInitialLayout() && game.round == 1 && !game.moved && !game.attacked && game.log.size() == 1;
            }
            if (frames == 185) {
                valid = prepareCombat(game) && valid;
                guardHpBefore = game.player.hp;
                valid = game.tryGuard() && valid;
            }
            if (frames == 235) { valid = game.endTurn() && valid; enemyTimer = 0; }
            if (frames == 350) {
                verifiedGuard = game.phase == Phase::PlayerTurn && !game.guarding && game.player.hp == guardHpBefore - 2;
                valid = valid && verifiedGuard;
            }
            if (benchmarkSeconds > 0 && frames > 360) {
                if (game.phase == Phase::Won || game.phase == Phase::Lost) game.start();
                if (game.phase == Phase::PlayerTurn && frames % 30 == 0) {
                    if (++automatedTurns % 4 == 0) game.tryGuard();
                    else for (int e = 0; e < 2; ++e) if (game.canAttack(e)) { game.tryAttack(e); break; }
                    game.endTurn(); enemyTimer = 0;
                }
            }
        } else input(game, enemyTimer, characters);
        if (game.phase == Phase::EnemyTurn && !characters.busy()) {
            enemyTimer += GetFrameTime();
            if (enemyTimer >= 0.30f) {
                const auto before = std::chrono::steady_clock::now();
                game.stepEnemy();
                maxEnemyMs = std::max(maxEnemyMs, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - before).count());
                enemyTimer = 0;
            }
        } else if (game.phase != Phase::EnemyTurn) enemyTimer = 0;
        characters.update(game, GetFrameTime());
        BeginDrawing();
        draw(game, characters);
        EndDrawing();
        if (automated) {
            if (frames == 3) capture("01-title");
            if (frames == 10) capture("02-gameplay");
            if (frames == 20) capture("03-enemy-turn");
            if (frames == 18) capture("07-character-walk");
            if (frames == 58) capture("08-character-attack");
            if (frames == 105) capture("04-victory");
            if (frames == 150) capture("05-defeat");
            if (frames == 180) capture("06-restart");
            if (frames == 230) capture("09-guard");
            if (frames == 255) capture("10-guard-hit");
            const bool captureRecovery = frames == 4 || frames == 11 || frames == 19 || frames == 21 || frames == 59 || frames == 106 || frames == 151 || frames == 181 || frames == 231 || frames == 256;
            if (frames > (benchmarkSeconds > 0 ? 360 : 5) && !captureRecovery)
                frameTimes.push_back(GetFrameTime() * 1000.0);
        }
        ++frames;
        if (automated && ((benchmarkSeconds <= 0 && frames >= 360) ||
                          (benchmarkSeconds > 0 && frames >= 360 && GetTime() - start >= benchmarkSeconds))) break;
    }
    if (automated) {
        // raylib 5.5 automation event 18 exercises its window-close state without desktop input.
        PlayAutomationEvent({0, 18, {0, 0, 0, 0}});
        const bool windowCloseCheck = WindowShouldClose();
        // A timed benchmark may end during a valid animation; the scripted reset check must end idle.
        valid = valid && windowCloseCheck && verifiedWin && verifiedLoss && verifiedGuard && restarts == 10 && frames >= 351 && characters.valid() &&
            (benchmarkSeconds > 0 || !characters.busy());
        const double elapsed = GetTime() - start;
        const double mean = frameTimes.empty() ? 0 : std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0) / frameTimes.size();
        std::sort(frameTimes.begin(), frameTimes.end());
        const double p95 = frameTimes.empty() ? 0 : frameTimes[static_cast<std::size_t>((frameTimes.size() - 1) * 0.95)];
        std::string json = "{\n  \"passed\": " + std::string(valid ? "true" : "false") +
            ",\n  \"frames\": " + std::to_string(frames) + ",\n  \"elapsed_seconds\": " + std::to_string(elapsed) +
            ",\n  \"mean_frame_ms\": " + std::to_string(mean) + ",\n  \"p95_frame_ms\": " + std::to_string(p95) +
            ",\n  \"max_enemy_decision_ms\": " + std::to_string(maxEnemyMs) +
            ",\n  \"verified_restarts\": " + std::to_string(restarts) +
            ",\n  \"character_animation_checks\": " + std::string(animationChecks ? "true" : "false") +
            ",\n  \"input_checks\": " + std::string(inputChecks ? "true" : "false") +
            ",\n  \"window_close_check\": " + std::string(windowCloseCheck ? "true" : "false") +
            ",\n  \"presentation_active_at_end\": " + std::string(characters.busy() ? "true" : "false") +
            ",\n  \"verified_win\": " + std::string(verifiedWin ? "true" : "false") +
            ",\n  \"verified_loss\": " + std::string(verifiedLoss ? "true" : "false") +
            ",\n  \"verified_guard\": " + std::string(verifiedGuard ? "true" : "false") + "\n}\n";
        std::cout << json;
        if (!captureDirectory.empty()) {
            std::ofstream output(captureDirectory / "verification.json");
            output << json;
            if (!output) valid = false;
        }
    }
    characters.unload();
    CloseWindow();
    return automated && !valid ? 1 : 0;
}
