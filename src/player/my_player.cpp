#include "my_player.hpp"
#include <cstdlib>
#include <ctime>

namespace ttt::my_player {

    static const int CANDIDATE_RADIUS = 4;
    static const int MAX_CANDIDATES = 50;
    static const int INNER_RADIUS = 2;
    static const int INNER_MAX_CANDIDATES = 20;

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

    static int quick_score(const LightState& ls, int x, int y, Sign my_sign) {
        int total = 0;
        for (int dir = 0; dir < 4; ++dir) {
            for (int start = -4; start <= 0; ++start) {
                Sign window[5];
                bool valid = true;
                for (int i = 0; i < 5; ++i) {
                    int nx = x + directions[dir].dx * (start + i);
                    int ny = y + directions[dir].dy * (start + i);
                    if (!on_field(nx, ny)) { valid = false; break; }
                    window[i] = (nx == x && ny == y) ? my_sign : ls.grid[ny][nx];
                }
                if (valid) total += score_one(window, my_sign);
            }
        }
        return total;
    }

    static void sort_candidates_light(Point* candidates, int count,
                                      const LightState& ls, Sign my_sign) {
        for (int i = 0; i < count - 1; ++i) {
            int best_idx = i;
            int best_val = quick_score(ls, candidates[i].x, candidates[i].y, my_sign);
            for (int j = i + 1; j < count; ++j) {
                int val = quick_score(ls, candidates[j].x, candidates[j].y, my_sign);
                if (val > best_val) {
                    best_val = val;
                    best_idx = j;
                }
            }
            if (best_idx != i) {
                Point tmp = candidates[i];
                candidates[i] = candidates[best_idx];
                candidates[best_idx] = tmp;
            }
        }
    }

    static void get_candidates_light(const LightState& ls, Point* candidates,
                                     int& count, Sign my_sign) {
        static bool near[20][20] = { false };
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x)
                near[y][x] = false;

        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) {
                Sign val = ls.grid[y][x];
                if (val == Sign::X || val == Sign::O) {
                    for (int dy = -CANDIDATE_RADIUS; dy <= CANDIDATE_RADIUS; ++dy) {
                        for (int dx = -CANDIDATE_RADIUS; dx <= CANDIDATE_RADIUS; ++dx) {
                            int nx = x + dx, ny = y + dy;
                            if (on_field(nx, ny) && ls.grid[ny][nx] == Sign::NONE) {
                                near[ny][nx] = true;
                            }
                        }
                    }
                }
            }
        }

        count = 0;
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x)
                if (near[y][x]) {
                    candidates[count].x = x;
                    candidates[count].y = y;
                    ++count;
                }

        sort_candidates_light(candidates, count, ls, my_sign);
        if (count > MAX_CANDIDATES) count = MAX_CANDIDATES;
        if (count == 0) {
            candidates[0] = {10, 10};
            count = 1;
        }
    }

    static void get_inner_candidates(const LightState& ls, Point* candidates, int& count) {
        static bool near[20][20] = {false};
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x)
                near[y][x] = false;

        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) {
                Sign val = ls.grid[y][x];
                if (val == Sign::X || val == Sign::O) {
                    for (int dy = -INNER_RADIUS; dy <= INNER_RADIUS; ++dy) {
                        for (int dx = -INNER_RADIUS; dx <= INNER_RADIUS; ++dx) {
                            int nx = x + dx, ny = y + dy;
                            if (on_field(nx, ny) && ls.grid[ny][nx] == Sign::NONE) {
                                near[ny][nx] = true;
                            }
                        }
                    }
                }
            }
        }

        count = 0;
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x)
                if (near[y][x]) {
                    candidates[count].x = x;
                    candidates[count].y = y;
                    ++count;
                    if (count >= INNER_MAX_CANDIDATES) return;
                }
        if (count == 0) {
            candidates[0] = {10, 10};
            count = 1;
        }
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

    static int minimax(const LightState& ls, int depth, bool maximizing,
                       Sign my_sign, int alpha, int beta, clock_t deadline, int& best_val_out) {
        if (clock() > deadline) {
            best_val_out = score_all_light(ls, my_sign);
            return best_val_out;
        }
        if (depth == 0) {
            best_val_out = score_all_light(ls, my_sign);
            return best_val_out;
        }

        Point candidates[400];
        int cand_count = 0;
        get_inner_candidates(ls, candidates, cand_count);
        if (cand_count == 0) {
            best_val_out = score_all_light(ls, my_sign);
            return best_val_out;
        }

        if (maximizing) {
            int max_val = -1000000000;
            for (int i = 0; i < cand_count; ++i) {
                LightState child = ls;
                child.apply_move(candidates[i].x, candidates[i].y);
                int dummy;
                int val = minimax(child, depth - 1, false, my_sign, alpha, beta, deadline, dummy);
                if (val > max_val) max_val = val;
                if (val > alpha) alpha = val;
                if (beta <= alpha) break;
            }
            best_val_out = max_val;
            return max_val;
        } else {
            int min_val = 1000000000;
            for (int i = 0; i < cand_count; ++i) {
                LightState child = ls;
                child.apply_move(candidates[i].x, candidates[i].y);
                int dummy;
                int val = minimax(child, depth - 1, true, my_sign, alpha, beta, deadline, dummy);
                if (val < min_val) min_val = val;
                if (val < beta) beta = val;
                if (beta <= alpha) break;
            }
            best_val_out = min_val;
            return min_val;
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

        Point top_moves[MAX_CANDIDATES];
        int top_cnt = 0;
        get_candidates_light(root, top_moves, top_cnt, m_sign);
        if (top_cnt == 0) return {10, 10};

        for (int i = 0; i < top_cnt; ++i) {
            LightState test = root;
            test.apply_move(top_moves[i].x, top_moves[i].y);
            if (is_winning(test, m_sign)) return top_moves[i];
        }

        Sign opp = (m_sign == Sign::X) ? Sign::O : Sign::X;
        for (int i = 0; i < top_cnt; ++i) {
            LightState test = root;
            test.grid[top_moves[i].y][top_moves[i].x] = opp;
            if (is_winning(test, opp)) return top_moves[i];
        }

        clock_t start_time = clock();
        clock_t deadline = start_time + (CLOCKS_PER_SEC * 80 / 1000);
        Point best = top_moves[0];
        int best_score = -1000000000;

        for (int depth = 2; depth <= 6; ++depth) {
            int current_best_score = -1000000000;
            Point current_best = top_moves[0];
            bool time_out = false;

            for (int i = 0; i < top_cnt; ++i) {
                if (clock() > deadline) { time_out = true; break; }
                LightState child = root;
                child.apply_move(top_moves[i].x, top_moves[i].y);
                int dummy;
                int score = minimax(child, depth - 1, false, m_sign,
                                    -1000000000, 1000000000, deadline, dummy);
                if (score > current_best_score) {
                    current_best_score = score;
                    current_best = top_moves[i];
                }
            }
            if (time_out) break;
            best_score = current_best_score;
            best = current_best;
        }
        return best;
    }

} // namespace ttt::my_player