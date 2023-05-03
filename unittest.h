void test(const bool verbose, const bool with_perft);
uint64_t perft(const Board & b, std::set<uint64_t> *const seen, const player_t p, const int depth, const int pass, const int verbose, const bool top);
