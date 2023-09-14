#include "random.h"
#include "zobrist.h"
#include "io.h"

Zobrist::Zobrist(const int dim)
{
	setDim(dim);
}

Zobrist::~Zobrist()
{
}

void Zobrist::setDim(const int dim)
{
#ifdef SITUATIONAL_SUPERKO
	size_t newN = dim * dim * 2;
#else
	size_t newN = dim * dim;
#endif

	for(size_t i=rngs.size(); i<newN; i++)
		rngs.push_back(distribution(gen));
}

uint64_t Zobrist::get(const int nr, const bool black) const
{
	send(true, "Zobrist::get %d/%d (= %lu) byte %p", nr, black, rngs.at(nr), __builtin_return_address(0));
#ifdef SITUATIONAL_SUPERKO
	return rngs.at(nr * 2 + black);
#else
	return rngs.at(nr);  // TT & CGOS
#endif
}
