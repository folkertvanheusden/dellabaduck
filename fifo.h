// (C) 2022-2023 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
#pragma once

#include <optional>
#include <pthread.h>
#include <stdint.h>

template <typename T>
class fifo
{
private:
	T        *data       { nullptr };
	const int n_elements { 0       };

	int  read_pointer    { 0       };
	int  write_pointer   { 0       };
	bool full            { false   };

	bool interrupted     { false   };

	pthread_mutex_t lock;

	/* cond_push is signalled when a new item is pushed
	 * cond_pull is signalled when an item is removed
	 */
	pthread_cond_t cond_push;
	pthread_cond_t cond_pull;

	uint64_t    n_in     { 0       };

public:
	fifo(const int n_elements) : n_elements(n_elements)
	{
		data = new T[n_elements];

		pthread_mutex_init(&lock, NULL);

		pthread_cond_init(&cond_push, NULL);
		pthread_cond_init(&cond_pull, NULL);
	}

	~fifo()
	{
		delete [] data;
	}

	bool is_empty()
	{
		pthread_mutex_lock(&lock);

		bool empty = n_in == 0;

		pthread_mutex_unlock(&lock);

		return empty;
	}

	void interrupt()
	{
		pthread_mutex_lock(&lock);

		interrupted = true;

		pthread_cond_broadcast(&cond_push);

		pthread_mutex_unlock(&lock);
	}

	void put(const T & element)
	{
		pthread_mutex_lock(&lock);

		while(full)
			pthread_cond_wait(&cond_pull, &lock);

		data[write_pointer] = element;

		write_pointer++;
		write_pointer %= n_elements;

		n_in++;

		full = write_pointer == read_pointer;

		pthread_cond_signal(&cond_push);

		pthread_mutex_unlock(&lock);
	}

	bool try_put(const T & element)
	{
		bool have_put = false;

		pthread_mutex_lock(&lock);

		if (!full) {
			data[write_pointer] = element;

			write_pointer++;
			write_pointer %= n_elements;

			n_in++;

			full = write_pointer == read_pointer;

			have_put = true;

			pthread_cond_signal(&cond_push);
		}

		pthread_mutex_unlock(&lock);

		return have_put;
	}

	std::optional<T> get(const int ms)
	{
		struct timespec ts { 0, 0 };
		clock_gettime(CLOCK_REALTIME, &ts);

		ts.tv_nsec += (ms % 1000) * 1000000ll;
		ts.tv_sec += ms / 1000;

		if (ts.tv_nsec >= 1000000000ll) {
			ts.tv_nsec -= 1000000000ll;
			ts.tv_sec++;
		}

		pthread_mutex_lock(&lock);

		bool ok = true;

		while(read_pointer == write_pointer && !full && !interrupted) {
			if (pthread_cond_timedwait(&cond_push, &lock, &ts)) {
				ok = false;
				break;
			}
		}

		if (interrupted) {
			pthread_mutex_unlock(&lock);

			return { };
		}

		if (ok) {
			T copy = data[read_pointer];

			read_pointer++;
			read_pointer %= n_elements;

			n_in--;

			full = 0;

			pthread_cond_signal(&cond_pull);

			pthread_mutex_unlock(&lock);

			return copy;
		}

		pthread_mutex_unlock(&lock);

		return { };
	}

	std::optional<T> get()
	{
		pthread_mutex_lock(&lock);

		while(read_pointer == write_pointer && !full && !interrupted)
			pthread_cond_wait(&cond_push, &lock);

		if (interrupted) {
			pthread_mutex_unlock(&lock);

			return { };
		}

		T copy = data[read_pointer];

		read_pointer++;
		read_pointer %= n_elements;

		n_in--;

		full = 0;

		pthread_cond_signal(&cond_pull);

		pthread_mutex_unlock(&lock);

		return copy;
	}

	std::optional<T> try_get()
	{
		pthread_mutex_lock(&lock);

		if ((read_pointer == write_pointer && !full) || interrupted) {
			pthread_mutex_unlock(&lock);

			return { };
		}

		T copy = data[read_pointer];

		read_pointer++;
		read_pointer %= n_elements;

		n_in--;

		full = 0;

		pthread_cond_signal(&cond_pull);

		pthread_mutex_unlock(&lock);

		return copy;
	}
};
