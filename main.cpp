#include <atomic>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#ifndef _MSC_VER
#include <x86intrin.h>
#define NOINLINE __attribute__((noinline))
#else
#include <intrin.h>
#define NOINLINE __declspec(noinline)
#endif

namespace uarch {
	namespace {
		constexpr auto sample_size = 1ull << 24;

		void prefetch_read(const void* pointer)
		{
#ifndef _MSC_VER
			__builtin_prefetch(pointer);
#else
			_m_prefetch(const_cast<void*>(pointer));
#endif
		}

		void prefetch_write(const void* pointer)
		{
#ifndef _MSC_VER
			__builtin_prefetch(pointer, 1);
#else
			_m_prefetchw(pointer);
#endif
		}

		static inline std::uint64_t start_timed()
		{
#ifndef _MSC_VER
			unsigned cycles_low, cycles_high;

			asm volatile("CPUID\n\t"
						 "RDTSC\n\t"
						 "mov %%edx, %0\n\t"
						 "mov %%eax, %1\n\t"
						 : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");

			return ((std::uint64_t)cycles_high << 32) | cycles_low;
#else
			int values[4];
			__cpuid(values, 0);
			return __rdtsc();
#endif
		}

		/**
		 * CITE:
		 * http://www.intel.com/content/www/us/en/embedded/training/ia-32-ia-64-benchmark-code-execution-paper.html
		 */
		static inline std::uint64_t end_timed()
		{
#ifndef _MSC_VER
			unsigned cycles_low, cycles_high;

			asm volatile("RDTSCP\n\t"
						 "mov %%edx, %0\n\t"
						 "mov %%eax, %1\n\t"
						 "CPUID\n\t"
						 : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");

			return ((std::uint64_t)cycles_high << 32) | cycles_low;
#else
			unsigned int aux_value;
			const auto timestamp = __rdtscp(&aux_value);
			int values[4];
			__cpuid(values, 0);
			return timestamp;
#endif
		}

		struct xorwow_state {
			uint32_t x[5];
			uint32_t counter;
		};

		/* The state array must be initialized to not be all zero in the first four words */
		uint32_t xorwow(struct xorwow_state* state)
		{
			/* Algorithm "xorwow" from p. 5 of Marsaglia, "Xorshift RNGs" */
			uint32_t t = state->x[4];

			uint32_t s = state->x[0]; /* Perform a contrived 32-bit shift. */
			state->x[4] = state->x[3];
			state->x[3] = state->x[2];
			state->x[2] = state->x[1];
			state->x[1] = s;

			t ^= t >> 2;
			t ^= t << 1;
			t ^= s ^ (s << 4);
			state->x[0] = t;
			state->counter += 362437;
			return t + state->counter;
		}

		struct alignas(64) cache_line {
			std::uint8_t padding[64];
		};

		void NOINLINE noopt(cache_line*) {}

		auto NOINLINE do_prefetch_saturation(uint32_t seed)
		{
			std::vector<cache_line> cache_lines(sample_size);
			xorwow_state state {};
			state.x[0] = seed;

			constexpr auto queue_len = 16;
			static_assert((queue_len & (queue_len - 1)) == 0);
			std::array<cache_line*, queue_len> queue {};
			std::size_t queue_head {};

			const auto start = start_timed();
			auto foo = 0;
			for (int i {}; i < sample_size; i += 4) {
				//const auto offset = i & (cache_lines.size() - 1);
				const auto a = &cache_lines[i];
				const auto b = &cache_lines[i+1];
				const auto c = &cache_lines[i+2];
				const auto d = &cache_lines[i+3];
				//prefetch_write(current);
				foo += a->padding[0] + b->padding[0] + c->padding[0] + d->padding[0];
				
				//queue[(queue_head++) & (queue_len - 1)] = current;
				//if (queue_head >= queue_len) {
				//	const auto last_line = queue[(queue_head - queue_len) & (queue_len - 1)];
				//	foo += last_line->padding[0];
				//}
			}

			const auto end = end_timed();
			const auto average = static_cast<double>(end - start) / sample_size;

			printf("%d", foo);

			return average;
		}

	}
}

int main()
{
	using namespace uarch;

	const auto n_cores = std::thread::hardware_concurrency();
	std::vector<std::thread> threads(n_cores - 1);
	std::vector<double> cold_times(n_cores);
	std::atomic_bool start {};
	std::atomic_uint waiting {};

	printf("threads: %d\n", n_cores);
	
	xorwow_state state {};
	state.x[0] = 0xdeadbeef;

	for (unsigned int i {}; i < n_cores - 1; ++i) {
		threads.at(i) = std::thread {[&cold = cold_times.at(i), &waiting, &start, seed = xorwow(&state)] {
			++waiting;
			while (!start)
				_mm_pause();

			cold = do_prefetch_saturation(seed);
		}};
	}

	while (waiting < n_cores - 1)
		_mm_pause();

	start = true;

	cold_times.back() = do_prefetch_saturation(xorwow(&state));

	for (auto& thread : threads)
		thread.join();

	const auto cold_average = std::accumulate(cold_times.begin(), cold_times.end(), 0.0) / n_cores;
	std::cout << cold_average << " cycles/op\n";
}
