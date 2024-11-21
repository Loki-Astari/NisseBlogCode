#include <vector>
#include <iostream>
#include <coroutine>

// The minimal object need to store co-routine state.
// This is the object returned from the co-routine (see primeGen() below).
struct PrimeCo
{
    struct promise_type;
    using HandleType = std::coroutine_handle<promise_type>;
    // The compiler uses the type "promise_type" to control the finite state
    // machine that it generates. It is required. This contains the minimal
    // implementation to work.
    struct promise_type
    {
        // Init co-routine object.
        PrimeCo             get_return_object()                 {return PrimeCo{HandleType::from_promise(*this)};}

        // co await handling Functions.
        std::suspend_always initial_suspend()       noexcept    {return {};}
        std::suspend_always final_suspend()         noexcept    {return {};}
        void                unhandled_exception()               {}

        // Called by co_yield.
        std::size_t     output;
        std::suspend_always yield_value(std::size_t value)      {output = value;return {};}
    };

    // Need to track the promise object.
    HandleType  handle;
    PrimeCo(HandleType handle)
        : handle{handle}
    {}
    ~PrimeCo()
    {
        handle.destroy();
    }

    // Get the next value.
    std::size_t get()
    {
        // Resume the co-routine
        if (!handle.done()) {
            handle.resume();
        }
        return handle.promise().output;
    }
};

bool getNextPrime(std::vector<std::size_t>& primes)
{
    std::size_t next = primes.back() + 2;
    for (;next >= 5; next += 2)
    {
        bool isPrime = true;
        for (std::size_t prime: primes)
        {
            if (prime * prime > next) {
                break;
            }
            if (next % prime == 0) {
                isPrime = false;
                break;
            }
        }
        if (isPrime) {
            primes.emplace_back(next);
            return true;
        }
    }
    return false;
}

// When the compiler see's a function
// that calls co_yield (or co_return or co_await) it knows
// it has a co-routine and will generate a finite state
// machine using the return type to store state (see above).
PrimeCo primeGen()
{
    co_yield 2;
    co_yield 3;

    std::vector<std::size_t>    primes{2, 3};
    while (getNextPrime(primes)) {
        co_yield primes.back();
    }
}

int main()
{
    auto primes = primeGen();
    for (int loop = 0; loop < 10; ++loop) {
        std::cerr << primes.get() << "\n";
    }
}

