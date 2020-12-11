/**
 * benchmark.cc
 * Talib Pierson & Thalia Wright
 * November 2020
 * Perform an etc-style benchmark.
 */

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include "cache.hh"

#define ITERS 30000
#define THREADS 2

using millis = std::chrono::duration<double, std::milli>;


class Workload
{
private:
    std::random_device device;
    std::default_random_engine engine{device()};
    std::uniform_int_distribution<int> uniform;
    std::extreme_value_distribution<double> extreme{0, 0.625};
    std::extreme_value_distribution<double> extreme_long{0, 16};

    Cache::size_type maxmem;

    /// random signed long
    auto rand_ssize()
    {
        return static_cast<ssize_t>(this->extreme(this->engine));
    }

    /// random unsigned long
    auto rand_size()
    {
        return static_cast<size_t>(std::abs(this->rand_ssize()));
    }

    /// random signed long (higher values)
    auto rand_size_long()
    {
        return static_cast<size_t>(std::abs(this->extreme_long(this->engine)));
    }

    /// random std::string
    auto random_val()
    {
        std::string table =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789-_"; // Base64 table
        std::string val;
        size_t len = this->rand_size_long() + 1;
        for (size_t i = 0; i < len; ++i)
        {
            val += table[this->uniform(this->engine) % 64];
        }
        return val;
    }

    static auto set(const std::shared_ptr<Cache> &cachep, const key_type &key,
                    const std::string &data)
    {
        bool ret = false;
        std::chrono::duration<long, std::ratio<1, 1000000000>> duration{-1};

        try
        {
            Cache::byte_type *data_buf;
            data_buf = new Cache::byte_type[data.size() + 1];
            strncpy(data_buf, data.c_str(), data.length() + 1);
            Cache::val_type val{
                data_buf, static_cast<Cache::size_type>(data.length() + 1)};

            auto start = std::chrono::steady_clock::now();
            ret = cachep->set(key, val);
            auto end = std::chrono::steady_clock::now();

            duration = end - start;

            delete[] data_buf;
        }
        catch (const std::exception &e)
        {
            std::cerr << "set_data(): e:" << e.what() << std::endl;
        }

        if (!ret)
            std::cerr << "set(): ret: " << ret << std::endl;

        return duration;
    }

    static auto get(const std::shared_ptr<Cache> &cachep, const key_type &key)
    {
        Cache::val_type val{nullptr, 0};
        std::chrono::duration<long, std::ratio<1, 1000000000>> duration{-1};

        try
        {
            auto start = std::chrono::steady_clock::now();
            val = cachep->get(key);
            auto end = std::chrono::steady_clock::now();

            duration = end - start;

            delete[] val.data_;
        }
        catch (const std::exception &e)
        {
            std::cerr << "get(): e: " << e.what() << std::endl;
        }

        return duration;
    }

    static auto del(const std::shared_ptr<Cache> &cachep, const key_type &key)
    {
        std::chrono::duration<long, std::ratio<1, 1000000000>> duration{-1};

        try
        {
            auto start = std::chrono::steady_clock::now();
            cachep->del(key);
            auto end = std::chrono::steady_clock::now();

            duration = end - start;
        }
        catch (const std::exception &e)
        {
            std::cerr << "del(): e: " << e.what() << std::endl;
        }

        return duration;
    }

public:
    /// The constructor takes the required parameters to set up a cache
    Workload(const std::string &server, const std::string &port,
             Cache::size_type bytes)
    {
        cache = std::make_unique<Cache>(server, port);
        this->maxmem = bytes;
    }

    /// Let other people access the cache
    std::shared_ptr<Cache> cache;

    /**
     * Make a random request to the cache.
     * Requests in order of descending frequency:
     *  (1) get, (2) set, (3) del.
     * @param cachep pointer to the cache
     * @return the latency of the request in nanoseconds
     */
    auto benchmark_request()
    {
        switch (this->rand_size() % 3)
        {
        case 0:
            return get(this->cache, std::to_string(this->rand_ssize()));
        case 1:
            return set(this->cache, std::to_string(this->rand_ssize()),
                       this->random_val());
        default:
            return del(this->cache, std::to_string(this->rand_ssize()));
        }
    }

    ~Workload() = default;

}; // class Workload

/**
 * @param nreq          the number of iterations
 * @param &workload     the workload generation object
 * @param &duration_ref a reference to a variable top store the duration
 * @return a vector of measurements
 */
auto baseline_latencies(size_t nreq)
{

    thread_local auto workload = std::make_unique<Workload>("127.0.0.1", "42069", 16777216);

    std::vector<double> durations(nreq, 0);

    // "warm up" the cache before measuring
    for (size_t i = 0; i < 256; ++i)
    {
        workload->benchmark_request();
    }

    for (size_t i = 0; i < nreq; ++i)
    {
        auto start = std::chrono::high_resolution_clock::now();

        workload->benchmark_request();

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> seconds = end - start;
        double time = millis(seconds).count();
        durations[i] = time;        
    }
    auto end = std::chrono::high_resolution_clock::now();

    return durations;
}

/**
 * @param a pointer to the workload object
 * @return a struct containing the 95% latency and mean throughput
 */
std::pair<double, double> threaded_performance()
{
    std::pair<double, double> ret;

    size_t nreq = ITERS;
    size_t nreq_split = ITERS / THREADS;
    std::mutex mutex;

    std::vector<double> total_thread_output;

    total_thread_output.reserve(nreq);

    auto threadFunction = [&]() {
        std::vector<double> thread_output = baseline_latencies(nreq_split);
        std::unique_lock<std::mutex> lock (mutex);
        total_thread_output.insert(total_thread_output.end(), thread_output.begin(), thread_output.end());
    };  
    
    std::vector<std::thread> thread_vector;
    thread_vector.reserve(THREADS);

    // warming up the cache
    for (int i = 0; i < THREADS; i++)
    {
        thread_vector.push_back(std::thread(threadFunction));
    }


    // dealing with the threads
    for (int i = 0; i < THREADS; i++)
    {
        thread_vector[i].join();
    }

    auto latencies = total_thread_output;

    //sort out the latencies and then get the 95th percentile
    std::sort(latencies.begin(), latencies.end());
    double nineFive = std::ceil(95.0 / 100.0 * latencies.size()) - 1;
    double nineFiveVal = latencies[nineFive];
    ret.first = nineFiveVal;

    // Save the data to a file
    std::ofstream sheet{"latencies.tsv"};
    std::for_each(latencies.begin(), latencies.end(), [&](double val) { sheet << val << std::endl; });
    sheet.close();

    // calculate the mean_latency
    auto mean_latency = 0.0;
    std::for_each(latencies.begin(), latencies.end(), [&](double val) { mean_latency += val; });
    
    // calculate the mean_latency and the throughput 
    mean_latency /= latencies.size();
    ret.second = 1000 / mean_latency;

    return ret;
}

int main()
{

    auto p = threaded_performance();

    std::cout << "95% latency = " << p.first
              << " milliseconds" << std::endl;

    std::cout << "Throughput  = " << p.second
              << " requests per second" << std::endl;

    return EXIT_SUCCESS;
}
