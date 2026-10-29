#pragma once

#include <array>
#include <string>
#include <vector>

namespace turnforge {

constexpr int GridSize = 8;
constexpr int PlayerMaxHp = 12;
constexpr int EnemyMaxHp = 6;

struct Position {
    int x = 0;
    int y = 0;
};
bool operator==(Position a, Position b);
bool operator!=(Position a, Position b);
bool inBounds(Position p);
int distance(Position a, Position b);

enum class Phase { Title, PlayerTurn, EnemyTurn, Won, Lost };

struct Unit {
    Position position;
    int hp = 0;
    bool alive() const { return hp > 0; }
};

struct Game {
    std::array<bool, GridSize * GridSize> walls{};
    Unit player{{1, 6}, PlayerMaxHp};
    std::array<Unit, 2> enemies{{{{6, 1}, EnemyMaxHp}, {{6, 5}, EnemyMaxHp}}};
    Phase phase = Phase::Title;
    int round = 1;
    bool moved = false;
    bool attacked = false;
    bool guarding = false;
    int enemyIndex = 0;
    std::vector<std::string> log;
    std::string message;

    Game();
    void start();
    void toTitle();
    bool wallAt(Position p) const;
    bool occupied(Position p) const;
    int enemyAt(Position p) const;
    bool canMove() const;
    bool canAttack(int enemy) const;
    bool canGuard() const;
    std::vector<Position> path(Position from, Position to) const;
    std::array<int, GridSize * GridSize> movementDistances() const;
    bool tryMove(Position to);
    bool tryAttack(int enemy);
    bool tryGuard();
    bool endTurn();
    int activeEnemy() const;
    void stepEnemy();
    bool validInitialLayout() const;

private:
    void note(const std::string& text);
    bool reject(const std::string& text);
    void finishEnemyTurn();
};

} // namespace turnforge
