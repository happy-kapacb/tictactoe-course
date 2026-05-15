#include "my_player.hpp"
#include <cstdlib>

namespace ttt::my_player {

    static const struct {
        int dx;
        int dy;
    } directions[] = { {1, 0}, {0, 1}, {1, 1}, {1, -1} };

    static bool on_field(int x, int y) {
        return x >= 0 && x < 20 && y >= 0 && y < 20;
    }

    struct LightState {
        Sign grid[20][20];
        Sign current_player;
        int move_no;

        void from_state(const State& s) {
            for (int y = 0; y < 20; ++y)
                for (int x = 0; x < 20; ++x)
                    grid[y][x] = s.get_value(x, y);
            current_player = s.get_current_player();
            move_no = s.get_move_no();
        }

        Sign get(int x, int y) const {
            if (!on_field(x, y)) return Sign::WALL;
            return grid[y][x];
        }

        void apply_move(int x, int y) {
            grid[y][x] = current_player;
            current_player = (current_player == Sign::X) ? Sign::O : Sign::X;
            ++move_no;
        }
    };

    static int score_one(const Sign str[5], Sign my_sign) {
        int my = 0, opp = 0, none = 0;
        for (int i = 0; i < 5; ++i) {
            Sign s = str[i];
            if (s == Sign::WALL) return 0;
            if (s == my_sign) ++my;
            else if (s == Sign::NONE) ++none;
            else ++opp;
        }
        if (my > 0 && opp > 0) return 0;
        // Начальные веса (потом изменим)
        if (my == 5) return 10000000;
        if (my == 4 && none == 1) return 50000;
        if (my == 3 && none == 2) return 1000;
        if (my == 2 && none == 3) return 100;
        if (opp == 5) return -10000000;
        if (opp == 4 && none == 1) return -60000;
        if (opp == 3 && none == 2) return -3000;
        if (opp == 2 && none == 3) return -50;
        return 0;
    }

    static int score_all_light(const LightState& ls, Sign my_sign) {
        int total = 0;
        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) {
                for (int dir = 0; dir < 4; ++dir) {
                    Sign window[5];
                    bool valid = true;
                    for (int i = 0; i < 5; ++i) {
                        int nx = x + directions[dir].dx * i;
                        int ny = y + directions[dir].dy * i;
                        if (!on_field(nx, ny)) { valid = false; break; }
                        window[i] = ls.grid[ny][nx];
                    }
                    if (valid) total += score_one(window, my_sign);
                }
            }
        }
        return total;
    }

    static bool is_winning(const LightState& ls, Sign sign) {
        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) {
                if (ls.grid[y][x] != sign) continue;
                for (int dir = 0; dir < 4; ++dir) {
                    int cnt = 1;
                    for (int step = 1; step < 5; ++step) {
                        int nx = x + directions[dir].dx * step;
                        int ny = y + directions[dir].dy * step;
                        if (!on_field(nx, ny) || ls.grid[ny][nx] != sign) break;
                        ++cnt;
                    }
                    if (cnt >= 5) return true;
                }
            }
        }
        return false;
    }

    static int minimax(const LightState& ls, int depth, bool maximizing, Sign my_sign) {
        if (depth == 0) return score_all_light(ls, my_sign);

        Point moves[400];
        int move_cnt = 0;
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x)
                if (ls.grid[y][x] == Sign::NONE) {
                    moves[move_cnt].x = x;
                    moves[move_cnt].y = y;
                    ++move_cnt;
                }
        if (move_cnt == 0) return score_all_light(ls, my_sign);

        if (maximizing) {
            int best = -1000000000;
            for (int i = 0; i < move_cnt; ++i) {
                LightState child = ls;
                child.apply_move(moves[i].x, moves[i].y);
                int val = minimax(child, depth - 1, false, my_sign);
                if (val > best) best = val;
            }
            return best;
        } else {
            int best = 1000000000;
            for (int i = 0; i < move_cnt; ++i) {
                LightState child = ls;
                child.apply_move(moves[i].x, moves[i].y);
                int val = minimax(child, depth - 1, true, my_sign);
                if (val < best) best = val;
            }
            return best;
        }
    }

    static Point away_from_walls(const State& state) {
        int best_x = -1, best_y = -1, best_walls = 100000;
        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) {
                if (state.get_value(x, y) != Sign::NONE) continue;
                int walls = 0;
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx) {
                        int nx = x + dx, ny = y + dy;
                        if (on_field(nx, ny) && state.get_value(nx, ny) == Sign::WALL) ++walls;
                    }
                int dist = abs(x - 10) + abs(y - 10);
                if (walls < best_walls || (walls == best_walls && dist < abs(best_x - 10) + abs(best_y - 10))) {
                    best_walls = walls;
                    best_x = x; best_y = y;
                }
            }
        }
        if (best_x != -1) return {best_x, best_y};
        return {10, 10};
    }

    void MyPlayer::set_sign(Sign sign) { m_sign = sign; }
    const char* MyPlayer::get_name() const { return m_name; }

    Point MyPlayer::make_move(const State& state) {
        if (state.get_move_no() == 0)
            return away_from_walls(state);

        LightState root;
        root.from_state(state);

        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x) {
                if (root.grid[y][x] != Sign::NONE) continue;
                LightState test = root;
                test.apply_move(x, y);
                if (is_winning(test, m_sign))
                    return {x, y};
            }

        Sign opp = (m_sign == Sign::X) ? Sign::O : Sign::X;
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x) {
                if (root.grid[y][x] != Sign::NONE) continue;
                LightState test = root;
                test.grid[y][x] = opp;
                if (is_winning(test, opp))
                    return {x, y};
            }

        Point best = {0,0};
        int best_score = -1000000000;
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x) {
                if (root.grid[y][x] != Sign::NONE) continue;
                LightState child = root;
                child.apply_move(x, y);
                int score = minimax(child, 2, false, m_sign);
                if (score > best_score) {
                    best_score = score;
                    best = {x, y};
                }
            }
        return best;
    }


}; // namespace ttt::my_player