#include <cstdint>
#include <vector>
#include <numeric>
#include <iostream>
#include <thread>
#include <atomic>

#include <x86intrin.h>

namespace uarch
{
    namespace
    {
        constexpr auto sample_size = 10'000'000;

        void prefetch_read(const void *pointer)
        {
            __builtin_prefetch(pointer);
        }

        void prefetch_write(const void *pointer)
        {
            __builtin_prefetch(pointer, 1);
        }

        static inline std::uint64_t start_timed()
        {
            unsigned cycles_low, cycles_high;

            asm volatile(
                "CPUID\n\t"
                "RDTSC\n\t"
                "mov %%edx, %0\n\t"
                "mov %%eax, %1\n\t"
                : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");

            return ((std::uint64_t)cycles_high << 32) | cycles_low;
        }

        /**
         * CITE:
         * http://www.intel.com/content/www/us/en/embedded/training/ia-32-ia-64-benchmark-code-execution-paper.html
         */
        static inline std::uint64_t end_timed()
        {
            unsigned cycles_low, cycles_high;

            asm volatile(
                "RDTSCP\n\t"
                "mov %%edx, %0\n\t"
                "mov %%eax, %1\n\t"
                "CPUID\n\t"
                : "=r"(cycles_high), "=r"(cycles_low)::"%rax", "%rbx", "%rcx", "%rdx");

            return ((std::uint64_t)cycles_high << 32) | cycles_low;
        }

        struct xorwow_state
        {
            uint32_t x[5];
            uint32_t counter;
        };

        /* The state array must be initialized to not be all zero in the first four words */
        uint32_t xorwow(struct xorwow_state *state)
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

        struct alignas(64) cache_line
        {
            std::uint8_t padding[64];
        };

        auto __attribute__((noinline)) do_prefetch_saturation(bool use_xorwow)
        {
            std::vector<cache_line> cache_lines(1 << 20);
            xorwow_state state{};
            state.x[0] = 0xdeadbeef;

            std::vector<cache_line *> adresses(sample_size);
            for (auto &address : adresses)
            {
                if (use_xorwow)
                {
                    const auto offset = xorwow(&state) % cache_lines.size();
                    address = cache_lines.data() + offset;
                }
                else
                {
                    address = cache_lines.data();
                }
            }

            const auto start = start_timed();
            for (int i{}; i < sample_size; ++i)
                prefetch_read(adresses[i]);

            const auto end = end_timed();
            const auto average = static_cast<double>(end - start) / sample_size;

            return average;
        }

    }
}

int main()
{
    using namespace uarch;

    const auto n_cores = std::thread::hardware_concurrency();
    std::vector<std::thread> threads(n_cores - 1);
    std::vector<std::uint64_t> cold_times(n_cores);
    std::atomic_bool start{};
    std::atomic_uint waiting{};

    for (int i{}; i < n_cores - 1; ++i)
    {
        threads.at(i) = std::thread{[&cold = cold_times.at(i), &waiting, &start]
                                    {
                                        ++waiting;
                                        while (!start)
                                            _mm_pause();

                                        cold = do_prefetch_saturation(true);
                                    }};
    }

    while (waiting < n_cores - 1)
        _mm_pause();

    start = true;

    cold_times.back() = do_prefetch_saturation(true);

    for (auto &thread : threads)
        thread.join();

    const auto cold_average = std::accumulate(cold_times.begin(), cold_times.end(), 0) / n_cores;
    std::cout << cold_average << " cycles/op\n";
}
