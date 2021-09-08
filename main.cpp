#include <cstdint>
#include <vector>
#include <numeric>
#include <iostream>

#include <x86intrin.h>

namespace uarch
{
    namespace
    {
        constexpr auto sample_size = 10'000;

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

        auto __attribute__((noinline)) compute_timing_overhead()
        {
            std::vector<std::uint64_t> times(sample_size);
            for (int i{}; i < sample_size; ++i)
            {
                const auto start = start_timed();
                asm("");
                const auto end = end_timed();
                times.at(i) = end - start;
            }

            const auto sum = std::accumulate(times.begin(), times.end(), 0);
            const auto average = static_cast<double>(sum) / sample_size;

            return average;
        }

        auto __attribute__((noinline)) do_single_cycle_test()
        {
            std::vector<std::uint64_t> times(sample_size);
            for (int i{}; i < sample_size; ++i)
            {
                const auto start = start_timed();
                asm("nop");
                const auto end = end_timed();
                times.at(i) = end - start;
            }

            const auto sum = std::accumulate(times.begin(), times.end(), 0);
            const auto average = static_cast<double>(sum) / sample_size;

            return average;
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

        auto __attribute__((noinline)) time_prefetch1()
        {
            std::vector<std::uint64_t> times(sample_size);
            std::vector<char> bytes(1ull << 32);
            xorwow_state state{};
            state.x[0] = 0xdeadbeef;
            for (int i{}; i < sample_size; ++i)
            {
                const auto address = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto start = start_timed();
                prefetch_read(address);
                const auto end = end_timed();
                volatile auto foo = *address;
                times.at(i) = end - start;
            }

            const auto sum = std::accumulate(times.begin(), times.end(), 0);
            const auto average = static_cast<double>(sum) / sample_size;

            return average;
        }

        auto __attribute__((noinline)) time_prefetch2()
        {
            std::vector<std::uint64_t> times(sample_size);
            std::vector<char> bytes(1ull << 32);
            xorwow_state state{};
            state.x[0] = 0xdeadbeef;
            for (int i{}; i < sample_size; ++i)
            {
                const auto address1 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address2 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto start = start_timed();
                prefetch_read(address1);
                prefetch_read(address2);
                const auto end = end_timed();
                volatile auto foo = *address1;
                foo = *address2;
                times.at(i) = end - start;
            }

            const auto sum = std::accumulate(times.begin(), times.end(), 0);
            const auto average = static_cast<double>(sum) / sample_size;

            return average;
        }

        auto __attribute__((noinline)) time_prefetch3()
        {
            std::vector<std::uint64_t> times(sample_size);
            std::vector<char> bytes(1ull << 32);
            xorwow_state state{};
            state.x[0] = 0xdeadbeef;
            for (int i{}; i < sample_size; ++i)
            {
                const auto address1 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address2 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address3 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto start = start_timed();
                prefetch_read(address1);
                prefetch_read(address2);
                prefetch_read(address3);
                const auto end = end_timed();
                volatile auto foo = *address1;
                foo = *address2;
                foo = *address3;
                times.at(i) = end - start;
            }

            const auto sum = std::accumulate(times.begin(), times.end(), 0);
            const auto average = static_cast<double>(sum) / sample_size;

            return average;
        }

        auto __attribute__((noinline)) time_prefetch4()
        {
            std::vector<std::uint64_t> times(sample_size);
            std::vector<char> bytes(1ull << 32);
            xorwow_state state{};
            state.x[0] = 0xdeadbeef;
            for (int i{}; i < sample_size; ++i)
            {
                const auto address1 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address2 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address3 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address4 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto start = start_timed();
                prefetch_read(address1);
                prefetch_read(address2);
                prefetch_read(address3);
                prefetch_read(address4);
                const auto end = end_timed();
                volatile auto foo = *address1;
                foo = *address2;
                foo = *address3;
                foo = *address4;
                times.at(i) = end - start;
            }

            const auto sum = std::accumulate(times.begin(), times.end(), 0);
            const auto average = static_cast<double>(sum) / sample_size;

            return average;
        }

        auto __attribute__((noinline)) time_prefetch5()
        {
            std::vector<std::uint64_t> times(sample_size);
            std::vector<char> bytes(1ull << 32);
            xorwow_state state{};
            state.x[0] = 0xdeadbeef;
            for (int i{}; i < sample_size; ++i)
            {
                const auto address1 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address2 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address3 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address4 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address5 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto start = start_timed();
                prefetch_read(address1);
                prefetch_read(address2);
                prefetch_read(address3);
                prefetch_read(address4);
                prefetch_read(address5);
                const auto end = end_timed();
                volatile auto foo = *address1;
                foo = *address2;
                foo = *address3;
                foo = *address4;
                foo = *address5;
                times.at(i) = end - start;
            }

            const auto sum = std::accumulate(times.begin(), times.end(), 0);
            const auto average = static_cast<double>(sum) / sample_size;

            return average;
        }

        auto __attribute__((noinline)) time_prefetch6()
        {
            std::vector<std::uint64_t> times(sample_size);
            std::vector<char> bytes(1ull << 32);
            xorwow_state state{};
            state.x[0] = 0xdeadbeef;
            for (int i{}; i < sample_size; ++i)
            {
                const auto address1 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address2 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address3 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address4 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address5 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address6 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto start = start_timed();
                prefetch_read(address1);
                prefetch_read(address2);
                prefetch_read(address3);
                prefetch_read(address4);
                prefetch_read(address5);
                prefetch_read(address6);
                const auto end = end_timed();
                volatile auto foo = *address1;
                foo = *address2;
                foo = *address3;
                foo = *address4;
                foo = *address5;
                foo = *address6;
                times.at(i) = end - start;
            }

            const auto sum = std::accumulate(times.begin(), times.end(), 0);
            const auto average = static_cast<double>(sum) / sample_size;

            return average;
        }

        auto __attribute__((noinline)) time_prefetch7()
        {
            std::vector<std::uint64_t> times(sample_size);
            std::vector<char> bytes(1ull << 32);
            xorwow_state state{};
            state.x[0] = 0xdeadbeef;
            for (int i{}; i < sample_size; ++i)
            {
                const auto address1 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address2 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address3 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address4 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address5 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address6 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto address7 = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto start = start_timed();
                prefetch_read(address1);
                prefetch_read(address2);
                prefetch_read(address3);
                prefetch_read(address4);
                prefetch_read(address5);
                prefetch_read(address6);
                prefetch_read(address7);
                const auto end = end_timed();
                volatile auto foo = *address1;
                foo = *address2;
                foo = *address3;
                foo = *address4;
                foo = *address5;
                foo = *address6;
                foo = *address7;
                times.at(i) = end - start;
            }

            const auto sum = std::accumulate(times.begin(), times.end(), 0);
            const auto average = static_cast<double>(sum) / sample_size;

            return average;
        }

        auto __attribute__((noinline)) time_prefetch_with_dependency()
        {
            std::vector<std::uint64_t> times(sample_size);
            std::vector<char> bytes(1ull << 32);
            xorwow_state state{};
            state.x[0] = 0xdeadbeef;
            for (int i{}; i < sample_size; ++i)
            {
                const auto address = bytes.data() + (xorwow(&state) & ~(1ull << 32));
                const auto start = start_timed();
                prefetch_read(address);
                const auto value = *address;
                const auto end = end_timed();
                volatile auto foo = value;
                times.at(i) = end - start;
            }

            const auto sum = std::accumulate(times.begin(), times.end(), 0);
            const auto average = static_cast<double>(sum) / sample_size;

            return average;
        }

        struct alignas(64) cache_line
        {
            std::uint8_t padding[64];
        };

        auto __attribute__((noinline)) do_prefetch_saturation(bool should_touch, int bump_count)
        {
            std::vector<cache_line> cache_lines(1 << 26);
            xorwow_state state{};
            state.x[0] = 0xdeadbeef;

            std::vector<cache_line *> adresses(sample_size);
            for (auto &address : adresses)
            {
                const auto offset = xorwow(&state) % cache_lines.size();
                address = cache_lines.data() + offset;
            }

            if (should_touch)
            {
                for (auto &address : adresses)
                    const volatile auto foo = *address;
            }

            const auto start = start_timed();
            for (int i{}; i < sample_size; ++i)
            {
                prefetch_write(adresses[i]);
                const auto offset = i - 16;
                const auto elem = offset > 0 ? offset : 0;
                for (int j{}; j < bump_count; ++j)
                    adresses[elem]->padding[0]++;
            }

            const auto end = end_timed();
            const auto average = static_cast<double>(end - start) / sample_size;
            std::cout << "Averaged " << average << " cycles / prefetch iteration\n";
        }

        void do_prefetch_run_tests()
        {
            const auto timing_overhead = compute_timing_overhead();
            const auto nop_times = do_single_cycle_test();
            const auto prefetch1_times = time_prefetch1();
            const auto prefetch2_times = time_prefetch2();
            const auto prefetch3_times = time_prefetch3();
            const auto prefetch4_times = time_prefetch4();
            const auto prefetch5_times = time_prefetch5();
            const auto prefetch6_times = time_prefetch6();
            const auto prefetch7_times = time_prefetch7();
            const auto dependency_times = time_prefetch_with_dependency();
            std::cout << "Averaged " << timing_overhead << " cycles of timing overhead\n";
            std::cout << "Averaged " << nop_times << " cycles/nop\n";
            std::cout << "Averaged " << prefetch1_times << " cycles/1-prefetch\n";
            std::cout << "Averaged " << prefetch2_times << " cycles/2-prefetch\n";
            std::cout << "Averaged " << prefetch3_times << " cycles/3-prefetch\n";
            std::cout << "Averaged " << prefetch4_times << " cycles/4-prefetch\n";
            std::cout << "Averaged " << prefetch5_times << " cycles/5-prefetch\n";
            std::cout << "Averaged " << prefetch6_times << " cycles/6-prefetch\n";
            std::cout << "Averaged " << prefetch7_times << " cycles/7-prefetch\n";
            std::cout << prefetch2_times - prefetch1_times << ", " << prefetch3_times - prefetch2_times << ", " << prefetch4_times - prefetch3_times << ", " << prefetch5_times - prefetch4_times << ", " << prefetch6_times - prefetch5_times << " " << prefetch7_times - prefetch6_times << "\n";

            std::cout << "Averaged " << dependency_times << " cycles/prefetch-with-read\n";
        }
    }
}

int main()
{
    using namespace uarch;

    for (int i{}; i < 16; ++i)
    {
        // TODO: one core can't issue requests fast enough to saturate the controller
        std::cout << i << "\n";
        do_prefetch_saturation(false, i);
        do_prefetch_saturation(true, i);
    }
    //do_prefetch_run_tests();
}
