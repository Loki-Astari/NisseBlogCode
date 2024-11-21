    #include <vector>
    #include <iostream>
    #include <boost/coroutine2/coroutine.hpp>
    
    using CoRoutine     = boost::coroutines2::coroutine<std::size_t>::pull_type;
    using Yield         = boost::coroutines2::coroutine<std::size_t>::push_type;
    
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
    
    void primeGen(Yield& yield)
    {
        yield(2);
        yield(3);
    
        std::vector<std::size_t>    primes{2, 3};
        while (getNextPrime(primes)) {
            yield(primes.back());
        }
    }
    
    int main()
    {
        CoRoutine   primes(primeGen);
        for (int loop = 0; loop < 10; ++loop) {
            std::cerr << primes.get() << "\n";
        }
    }
    
