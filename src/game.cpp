#include "game.h"

#include <algorithm>
#include <cstdlib>
#include <queue>
#include <utility>

namespace turnforge {
namespace {
constexpr std::array<Position, 4> Directions{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
int index(Position p) { return p.y * GridSize + p.x; }
Position position(int i) { return {i % GridSize, i / GridSize}; }

struct Search {
    std::array<int, GridSize * GridSize> distances;
    std::array<int, GridSize * GridSize> parents;
};

Search search(const Game& game, Position start) {
    Search result;
    result.distances.fill(-1);
    result.parents.fill(-1);
    if (!inBounds(start) || game.wallAt(start)) return result;
    std::queue<Position> queue;
    queue.push(start);
    result.distances[index(start)] = 0;
    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();
        for (auto d : Directions) {
            Position next{current.x + d.x, current.y + d.y};
            if (!inBounds(next) || game.wallAt(next) || game.occupied(next) ||
                result.distances[index(next)] != -1) continue;
            result.distances[index(next)] = result.distances[index(current)] + 1;
            result.parents[index(next)] = index(current);
            queue.push(next);
        }
    }
    return result;
}
} // namespace

bool operator==(Position a, Position b) { return a.x == b.x && a.y == b.y; }
bool operator!=(Position a, Position b) { return !(a == b); }
bool inBounds(Position p) { return p.x >= 0 && p.x < GridSize && p.y >= 0 && p.y < GridSize; }
int distance(Position a, Position b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

Game::Game() {
    for (Position p : {Position{2, 2}, Position{3, 2}, Position{3, 3}, Position{3, 4}})
        walls[index(p)] = true;
    message = "Siap menentukan langkah pertama?";
}

void Game::start() {
    *this = Game{};
    phase = Phase::PlayerTurn;
    note("Pertandingan dimulai. Giliranmu.");
}

void Game::toTitle() { *this = Game{}; }

bool Game::wallAt(Position p) const { return !inBounds(p) || walls[index(p)]; }
bool Game::occupied(Position p) const {
    if (player.alive() && player.position == p) return true;
    return enemyAt(p) != -1;
}
int Game::enemyAt(Position p) const {
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i)
        if (enemies[i].alive() && enemies[i].position == p) return i;
    return -1;
}
bool Game::canMove() const { return phase == Phase::PlayerTurn && !moved && !attacked; }
bool Game::canAttack(int enemy) const {
    return phase == Phase::PlayerTurn && !attacked && enemy >= 0 &&
        enemy < static_cast<int>(enemies.size()) && enemies[enemy].alive() &&
        distance(player.position, enemies[enemy].position) == 1;
}
bool Game::canGuard() const { return phase == Phase::PlayerTurn && !attacked; }

std::vector<Position> Game::path(Position from, Position to) const {
    if (!inBounds(from) || !inBounds(to)) return {};
    const auto result = search(*this, from);
    if (result.distances[index(to)] == -1) return {};
    std::vector<Position> route;
    for (int i = index(to); i != -1; i = result.parents[i]) route.push_back(position(i));
    std::reverse(route.begin(), route.end());
    return route;
}

std::array<int, GridSize * GridSize> Game::movementDistances() const {
    if (canMove()) return search(*this, player.position).distances;
    std::array<int, GridSize * GridSize> unavailable;
    unavailable.fill(-1);
    return unavailable;
}

void Game::note(const std::string& text) {
    message = text;
    log.push_back(text);
    if (log.size() > 5) log.erase(log.begin());
}
bool Game::reject(const std::string& text) {
    message = text;
    return false;
}

bool Game::tryMove(Position to) {
    if (phase != Phase::PlayerTurn) return reject("Tunggu giliranmu untuk bergerak.");
    if (!canMove()) return reject("Gerak sudah terpakai. Akhiri giliran.");
    if (!inBounds(to)) return reject("Tujuan berada di luar arena.");
    if (to == player.position) return reject("Pilih petak tujuan yang berbeda.");
    if (wallAt(to)) return reject("Tembok tidak dapat dilewati.");
    if (occupied(to)) return reject("Petak itu ditempati unit hidup.");
    const auto route = path(player.position, to);
    if (route.empty()) return reject("Tidak ada jalur menuju petak itu.");
    const auto steps = static_cast<int>(route.size()) - 1;
    if (steps > 3) return reject("Jarak gerak maksimal tiga langkah.");
    player.position = to;
    moved = true;
    note("Pemain bergerak " + std::to_string(steps) + " langkah.");
    return true;
}

bool Game::tryAttack(int enemy) {
    if (phase != Phase::PlayerTurn) return reject("Tunggu giliranmu untuk menyerang.");
    if (attacked) return reject("Serangan sudah terpakai.");
    if (!canAttack(enemy)) return reject("Serang musuh hidup tepat di sampingmu.");
    auto& target = enemies[enemy];
    target.hp = std::max(0, target.hp - 3);
    attacked = true;
    moved = true;
    note("Serang E" + std::to_string(enemy + 1) + ": -3 HP.");
    if (!target.alive()) note("E" + std::to_string(enemy + 1) + " dikalahkan.");
    if (std::none_of(enemies.begin(), enemies.end(), [](const Unit& u) { return u.alive(); })) {
        phase = Phase::Won;
        note("Arena dikuasai. Kamu menang!");
    }
    return true;
}

bool Game::endTurn() {
    if (phase != Phase::PlayerTurn) return reject("Giliran pemain belum aktif.");
    phase = Phase::EnemyTurn;
    enemyIndex = 0;
    note("Giliran musuh. Amati posisi mereka.");
    return true;
}

bool Game::tryGuard() {
    if (phase != Phase::PlayerTurn) return reject("Bertahan hanya pada giliran pemain.");
    if (!canGuard()) return reject("Jatah serangan sudah terpakai.");
    guarding = true;
    moved = true;
    attacked = true;
    note("Bertahan aktif: damage musuh 2 -> 1.");
    return true;
}

int Game::activeEnemy() const {
    if (phase != Phase::EnemyTurn) return -1;
    for (int i = enemyIndex; i < static_cast<int>(enemies.size()); ++i)
        if (enemies[i].alive()) return i;
    return -1;
}

void Game::finishEnemyTurn() {
    if (phase != Phase::EnemyTurn) return;
    phase = Phase::PlayerTurn;
    ++round;
    moved = false;
    attacked = false;
    guarding = false;
    note("Ronde " + std::to_string(round) + ". Giliranmu.");
}

void Game::stepEnemy() {
    if (phase != Phase::EnemyTurn) return;
    const int current = activeEnemy();
    if (current == -1) {
        finishEnemyTurn();
        return;
    }
    auto& enemy = enemies[current];
    const std::string name = current == 0 ? "Goblin" : "Bandit";
    if (distance(enemy.position, player.position) != 1) {
        std::vector<Position> best;
        std::array<std::vector<Position>, 4> candidates;
        for (std::size_t i = 0; i < Directions.size(); ++i) {
            const auto d = Directions[i];
            const Position target{player.position.x + d.x, player.position.y + d.y};
            auto& candidate = candidates[i];
            candidate = path(enemy.position, target);
            if (!candidate.empty() && (best.empty() || candidate.size() < best.size())) best = candidate;
        }
        bool flanking = false;
        if (current == 1 && enemies[0].alive() && !best.empty()) {
            const auto directGoal = best.back();
            const auto maxLength = best.size() + 2;
            int bestSpacing = -1;
            std::vector<Position> flank;
            for (const auto& candidate : candidates) {
                if (candidate.empty() || candidate.size() > maxLength) continue;
                const int dx = candidate.back().x - enemies[0].position.x;
                const int dy = candidate.back().y - enemies[0].position.y;
                // Squared spacing distinguishes the opposite side from a perpendicular side.
                const int spacing = dx * dx + dy * dy;
                if (spacing > bestSpacing || (spacing == bestSpacing && candidate.size() < flank.size())) {
                    bestSpacing = spacing;
                    flank = candidate;
                }
            }
            best = std::move(flank);
            flanking = !best.empty() && best.back() != directGoal;
        }
        if (best.size() > 1) {
            enemy.position = best[std::min<std::size_t>(2, best.size() - 1)];
            note(name + (flanking ? " mencari sisi lain." : " mendekat."));
        } else {
            note(name + " menunggu; jalur terhalang.");
        }
    }
    if (distance(enemy.position, player.position) == 1) {
        const int damage = guarding ? 1 : 2;
        player.hp = std::max(0, player.hp - damage);
        note(name + " menyerang: pemain -" + std::to_string(damage) + " HP.");
        if (!player.alive()) {
            guarding = false;
            phase = Phase::Lost;
            note("Unitmu tumbang. Coba strategi baru.");
            return;
        }
    }
    enemyIndex = current + 1;
    if (activeEnemy() == -1) finishEnemyTurn();
}

bool Game::validInitialLayout() const {
    if (!player.alive() || wallAt(player.position)) return false;
    for (std::size_t i = 0; i < enemies.size(); ++i) {
        const auto& enemy = enemies[i];
        if (!enemy.alive() || wallAt(enemy.position) || enemy.position == player.position) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (enemy.position == enemies[j].position) return false;
        bool reachable = false;
        for (auto d : Directions) {
            Position target{player.position.x + d.x, player.position.y + d.y};
            if (!path(enemy.position, target).empty()) reachable = true;
        }
        if (!reachable) return false;
    }
    return true;
}

} // namespace turnforge
