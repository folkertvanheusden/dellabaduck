#include <algorithm>
#include <random>
#include <vector>


auto low_produce_seed()
{
	std::vector<unsigned int> random_data(std::mt19937::state_size);

	std::random_device source;
	std::generate(std::begin(random_data), std::end(random_data), [&](){return source();});

	return std::seed_seq(std::begin(random_data), std::end(random_data));
}

auto produce_seed()
{
//#ifndef NDEBUG
//	return 1;
//#else
	return low_produce_seed();
//#endif
}

thread_local auto mt_seed = produce_seed();
thread_local std::mt19937_64 gen { mt_seed };
#ifndef NDEBUG
thread_local auto mt_seed_low = low_produce_seed();
thread_local std::mt19937_64 gen_debug { mt_seed_low };
#endif
