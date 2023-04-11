#include <random>
#include <stdint.h>
#include <vector>


class Zobrist {
private:
	std::vector<std::uint64_t> rngs;

	std::uniform_int_distribution<std::uint64_t> distribution{ std::numeric_limits<std::uint64_t>::min(), std::numeric_limits<std::uint64_t>::max() };

public:
	Zobrist(const int dim);
	virtual ~Zobrist();

	void setDim(const int dim);

	uint64_t get(const int nr, const bool black) const;
};
