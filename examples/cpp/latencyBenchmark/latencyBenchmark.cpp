/** <!-------------------------------------------------------------------------->
*
*   @file latencyBenchmark.cpp
*
*   @brief Round-trip latency benchmark for the HSIL CoSim DDS transport.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Measures the round-trip latency of GroupedData samples
*       exchanged over the HSIL CoSim DDS transport. Supports two operating
*       modes:
*
*       SINGLE-MACHINE MODE (default, no --role argument)
*       --------------------------------------------------
*       Both the Pinger and Ponger run inside the same process. The Ponger
*       is started on a background thread. Each role uses its own independent
*       HSIL session (separate DDS participant) so that samples travel through
*       the full DDS serialisation / loopback / deserialisation path.
*
*           ./hsilLatencyBenchmark [--count N] [--size B]
*
*       TWO-MACHINE MODE (--role pinger | --role ponger)
*       ------------------------------------------------
*       Run one instance as the Pinger on machine A and one as the Ponger on
*       machine B. The two processes communicate over the DDS network. Start
*       the Ponger first so it is ready when the Pinger begins.
*
*           Machine B:  ./hsilLatencyBenchmark --role ponger
*           Machine A:  ./hsilLatencyBenchmark --role pinger  [--count N] [--size B]
*
*       COMMAND-LINE OPTIONS
*       --------------------
*           --role     pinger|ponger      Role in two-machine mode.
*           --count N  (default 1000)     Number of measured round trips.
*           --size  B  (default 16)       Payload size in bytes (8–65536).
*
*       STATISTICS REPORTED
*       -------------------
*       After all round trips the Pinger prints per-sample RTT samples and a
*       summary: min, max, mean, median and std-dev latency.
*
*   @copyright
*       Copyright 2026, dSPACE SE & Co. KG. All rights reserved.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

/*----------------------------------------------------------------------------*/
/* INCLUDES                                                                   */
/*----------------------------------------------------------------------------*/

#include <hsil/hsil.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <optional>
#include <mutex>
#include <condition_variable>

/*----------------------------------------------------------------------------*/
/* CONSTANTS                                                                  */
/*----------------------------------------------------------------------------*/

static constexpr uint32_t kDomainId = 42u;  // Arbitrary non-zero for this example application
static const char* kPingTopicName = "LatBench_Ping";
static const char* kPongTopicName = "LatBench_Pong";

static constexpr uint32_t kPingGroupId = 1u;
static constexpr uint32_t kPongGroupId = 2u;

/*----------------------------------------------------------------------------*/
/* COMMAND-LINE OPTIONS                                                       */
/*----------------------------------------------------------------------------*/

enum class Role { PingerPonger, Pinger, Ponger };

struct Options
{
    Role        role        = Role::PingerPonger;
    int         count       = 1000;
    size_t      payloadSize = 1024;
};

static void printUsage(const char* prog)
{
    std::fprintf(stderr,
        "Usage:\n"
        "  Single machine : %s [options]\n"
        "  Two machines   : %s --role ponger \n"
        "                   %s --role pinger [options]\n"
        "\n"
        "Options:\n"
        "  --role     pinger|ponger   Role (omit for single-machine mode)\n"
        "  --count    N               Measured round trips (default: 1000)\n"
        "  --size     B               Payload bytes, >= 1  (default: 1024)\n"
        "  --help                     Show this message\n",
        prog, prog, prog);
}

static bool parseArgs(int argc, char* argv[], Options& opts)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        auto nextVal = [&](const char* name) -> const char*
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "error: %s requires an argument.\n", name);
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h")
        {
            printUsage(argv[0]);
            std::exit(0);
        }
        else if (arg == "--role")
        {
            const char* v = nextVal("--role");
            if (!v) return false;
            if (std::strcmp(v, "pinger") == 0)      opts.role = Role::Pinger;
            else if (std::strcmp(v, "ponger") == 0) opts.role = Role::Ponger;
            else
            {
                std::fprintf(stderr, "error: --role must be 'pinger' or 'ponger', got '%s'.\n", v);
                return false;
            }
        }
        else if (arg == "--count")
        {
            const char* v = nextVal("--count");
            if (!v) return false;
            opts.count = std::atoi(v);
            if (opts.count < 1)
            {
                std::fprintf(stderr, "error: --count must be >= 1.\n");
                return false;
            }
        }
        else if (arg == "--size")
        {
            const char* v = nextVal("--size");
            if (!v) return false;
            const int sz = std::atoi(v);
            if (sz < static_cast<int>(1))
            {
                std::fprintf(stderr, "error: --size must be greater than 0.\n");
                return false;
            }
            opts.payloadSize = static_cast<size_t>(sz);
        }
        else
        {
            std::fprintf(stderr, "error: unknown option '%s'. Use --help.\n", arg.c_str());
            return false;
        }
    }
    return true;
}

/*----------------------------------------------------------------------------*/
/* TIMING                                                                     */
/*----------------------------------------------------------------------------*/

static uint64_t nowNs()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

/*----------------------------------------------------------------------------*/
/* PAYLOAD CODEC                                                              */
/*----------------------------------------------------------------------------*/

/** Encode timestamp (8 bytes) + sequence (4 bytes) at the start of buf. */
static void encodeHeader(uint8_t* buf, uint64_t ts, uint32_t seq)
{
    std::memcpy(buf,     &ts,  8);
    std::memcpy(buf + 8, &seq, 4);
}

/** Decode timestamp and sequence from the first 12 bytes of buf. */
static void decodeHeader(const uint8_t* buf, uint64_t& ts, uint32_t& seq)
{
    std::memcpy(&ts,  buf,     8);
    std::memcpy(&seq, buf + 8, 4);
}

/*----------------------------------------------------------------------------*/
/* STATISTICS                                                                  */
/*----------------------------------------------------------------------------*/

struct Stats
{
    double minNs   = 0.0;
    double maxNs   = 0.0;
    double meanNs  = 0.0;
    double medianNs= 0.0;
    double stddevNs= 0.0;
};

static Stats computeStats(std::vector<uint64_t>& samples)
{
    Stats s;
    if (samples.empty()) return s;

    std::sort(samples.begin(), samples.end());

    s.minNs    = static_cast<double>(samples.front());
    s.maxNs    = static_cast<double>(samples.back());

    const size_t n = samples.size();
    s.medianNs = (n % 2 == 0)
        ? static_cast<double>(samples[n / 2 - 1] + samples[n / 2]) / 2.0
        : static_cast<double>(samples[n / 2]);

    double sum = 0.0;
    for (uint64_t v : samples) sum += static_cast<double>(v);
    s.meanNs = sum / static_cast<double>(n);

    double var = 0.0;
    for (uint64_t v : samples)
    {
        const double d = static_cast<double>(v) - s.meanNs;
        var += d * d;
    }
    s.stddevNs = std::sqrt(var / static_cast<double>(n));

    return s;
}

static void printStats(const Stats& s, size_t count, size_t size)
{
    constexpr double kNsToUs = 1.0e-3;
    std::printf("\n--- Round-trip latency for %zu samples of %zu bytes each ---\n", count, size);
    std::printf("  Min         : %8.2f µs\n", s.minNs    * kNsToUs);
    std::printf("  Max         : %8.2f µs\n", s.maxNs    * kNsToUs);
    std::printf("  Mean        : %8.2f µs\n", s.meanNs   * kNsToUs);
    std::printf("  Median      : %8.2f µs\n", s.medianNs * kNsToUs);
    std::printf("  Std-dev     : %8.2f µs\n", s.stddevNs * kNsToUs);
}

class BinarySemaphore
{
    public:
            bool    acquire(std::optional<uint32_t> timeout = std::nullopt) {
                        std::unique_lock<std::mutex> uniqueLk{m_lock}; 
                        bool ReturnValue = false;
                        if (timeout)
                        {
                            ReturnValue = m_cv.wait_for(uniqueLk, std::chrono::milliseconds(timeout.value()), [&](){return m_isReady;});
                        }
                        else
                        {
                            m_cv.wait(uniqueLk, [&](){return m_isReady;});
                            ReturnValue = true;
                        }

                        if (ReturnValue)
                        {
                            m_isReady = false;
                        }
                        
                        return ReturnValue;
                    };

        void        release() {std::lock_guard<std::mutex> lk(m_lock); m_isReady = true; m_cv.notify_one();};

    private:
        bool        m_isReady{false};
        std::mutex  m_lock;
        std::condition_variable m_cv;
};

/*----------------------------------------------------------------------------*/
/* PONGER ROLE                                                                 */
/*----------------------------------------------------------------------------*/

struct PongerCtx
{
    HsilHandle         handle{nullptr};     /**< HSIL handle for the Ponger. */
    uint32_t           pongCount{0u};       /**< Number of pings received so far. */
    uint32_t           expectedPings{0u};   /**< Total pings expected set via cmd option */
    BinarySemaphore    semSyncAllPonged;    /**< Signal ponger that all pings have been ponged back for shutdown. */
};

static void onPingReceived(const char* /*topicName*/,
                            const HsilGroupedData* data,
                            void* userData)
{
    auto* ctx = static_cast<PongerCtx*>(userData);

    /* Send the pong. */
    hsil_publish_grouped(ctx->handle, kPongTopicName, data);

    /* Increment pong count. */
    ctx->pongCount++;

    /* Signal all pongs (including the extra warm-up pong) sent to deblock the ponger for shutdown. */
    if (ctx->pongCount == (ctx->expectedPings+1))
    {
        ctx->semSyncAllPonged.release();
    }
}

/**
 * @brief Run the Ponger until opts.count pings have been ponged back 
 *        (used both by the background thread in single-machine mode 
 *        and directly in --role ponger mode).
 */
static int runPonger(const Options& opts)
{
    PongerCtx ctx;

    /* Update total number of pings expected. */
    ctx.expectedPings = static_cast<uint32_t>(opts.count);

    if (hsil_create(kDomainId, nullptr, &ctx.handle) != HSIL_OK)
    {
        std::fprintf(stderr, "[Ponger] Failed to create HSIL session.\n");
        return EXIT_FAILURE;
    }

    if (hsil_create_grouped_publisher(ctx.handle, kPongTopicName, nullptr) != HSIL_OK)
    {
        std::fprintf(stderr, "[Ponger] Failed to create pong publisher.\n");
        hsil_destroy(ctx.handle);
        return EXIT_FAILURE;
    }

    if (hsil_subscribe_grouped(ctx.handle, kPingTopicName, nullptr,
                               HSIL_ALL_IDS, onPingReceived, &ctx) != HSIL_OK)
    {
        std::fprintf(stderr, "[Ponger] Failed to subscribe to ping topic.\n");
        hsil_destroy(ctx.handle);
        return EXIT_FAILURE;
    }

    std::printf("[Ponger] Ready on domain %d\n", kDomainId);

    /* Wait until signalled that all pings have been ponged back. */
    ctx.semSyncAllPonged.acquire();

    hsil_destroy(ctx.handle);
    
    return 0;
}

/*----------------------------------------------------------------------------*/
/* PINGER ROLE                                                                 */
/*----------------------------------------------------------------------------*/

struct PingerCtx
{
    uint64_t              recvTs{0};       /**< Pong arrival time captured inside the callback. */
    std::vector<uint64_t> rttNs;           /**< RTT for each measured ping (ns). */
    BinarySemaphore       semPongReceived; /**< Signal pinger that a pong has been received. */

};

static void onPongReceived(const char* /*topicName*/,
                            const HsilGroupedData* /*data*/,
                            void* userData)
{
    auto* ctx = static_cast<PingerCtx*>(userData);

    /* Capture the pong received time as early as possible. */
    const uint64_t ts = nowNs();
    ctx->recvTs    = ts;

    /* Signal pong received to calculate the rtt and continue with the next ping. */
    ctx->semPongReceived.release();
}

/**
 * @brief Block until the ping publisher has discovered the Ponger.
 *
 * Discovery is asynchronous, so a benchmark that starts publishing immediately
 * would lose its first pings and report a misleading warm-up latency. Polling
 * the publisher status instead of sleeping for a fixed duration makes the run
 * both faster and deterministic.
 *
 * @return true as soon as the Ponger is matched, false on timeout or error.
 */
static bool waitForPonger(HsilHandle handle, int timeoutSeconds)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);

    while (std::chrono::steady_clock::now() < deadline)
    {
        HsilEndpointStatus status{};
        const int rc = hsil_get_publisher_status(handle, kPingTopicName, &status);
        if (rc != HSIL_OK)
        {
            std::fprintf(stderr, "[Pinger] Failed to read publisher status (err=%d).\n", rc);
            return false;
        }

        if (status.matchedCount > 0)
        {
            return true;
        }

        if (status.qosMismatchCount > 0)
        {
            std::fprintf(stderr,
                         "[Pinger] A Ponger was found but rejected due to incompatible QoS.\n");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;
}

/**
 * @brief Run the Pinger: send opts.count pings and wait for each corresponding pong to calculate RTTs.
 *        Print the stats after all ping-pong(s) have been exchanged.
 */
static int runPinger(const Options& opts)
{
    HsilHandle handle = nullptr;
    if (hsil_create(kDomainId, nullptr, &handle) != HSIL_OK)
    {
        std::fprintf(stderr, "[Pinger] Failed to create HSIL session.\n");
        return EXIT_FAILURE;
    }

    PingerCtx ctx;
    ctx.rttNs.reserve(static_cast<size_t>(opts.count));

    if (hsil_subscribe_grouped(handle, kPongTopicName, nullptr,
                               HSIL_ALL_IDS, onPongReceived, &ctx) != HSIL_OK)
    {
        std::fprintf(stderr, "[Pinger] Failed to subscribe to pong topic.\n");
        hsil_destroy(handle);
        return EXIT_FAILURE;
    }

    if (hsil_create_grouped_publisher(handle, kPingTopicName, nullptr) != HSIL_OK)
    {
        std::fprintf(stderr, "[Pinger] Failed to create ping publisher.\n");
        hsil_destroy(handle);
        return EXIT_FAILURE;
    }

    /* Wait until the Ponger (local thread or remote process) has been
     * discovered, so no ping is sent into the void. */
    if (!waitForPonger(handle, 10))
    {
        std::fprintf(stderr, "[Pinger] No Ponger discovered; is it running on domain %d?\n",
                     kDomainId);
        hsil_destroy(handle);
        return EXIT_FAILURE;
    }

    std::printf("[Pinger] Domain %d | payload %zu bytes | measured pings %d\n",
                kDomainId, opts.payloadSize, opts.count);

    std::vector<uint8_t> payload(opts.payloadSize, 0u);
    const auto kPongTimeoutMs = 5000;

    /* Send one warm-up ping (i==0, RTT not recorded) followed by opts.count
     * measured pings. Pings are sent from the main thread — never from inside
     * a DDS callback — to avoid deadlocking the CycloneDDS reader thread in
     * single-machine mode. */
    for (int index = 0; index <= opts.count; ++index)
    {
        HsilGroupedData ping{};
        ping.id             = kPingGroupId;
        ping.sequenceNumber = static_cast<uint32_t>(index);
        const uint64_t sendTs = nowNs();
        ping.timestamp      = sendTs;
        ping.data           = payload.data();
        ping.dataSize       = opts.payloadSize;

        if (hsil_publish_grouped(handle, kPingTopicName, &ping) != HSIL_OK)
        {
            std::fprintf(stderr, "[Pinger] Failed to send ping seq=%d.\n", index);
            hsil_destroy(handle);
            return EXIT_FAILURE;
        }

        /* Wait for the matching pong with a timeout. */
        if (false == ctx.semPongReceived.acquire(kPongTimeoutMs))
        {
            std::fprintf(stderr, "[Pinger] Timeout waiting for pong seq=%d.%s\n",
                         index, index == 0 ? " Is the Ponger running?" : "");
            hsil_destroy(handle);
            return EXIT_FAILURE;
        }

        /* Calculate RTT for all pings except the warm-up (seq 0). */
        if (index > 0)
        {
            ctx.rttNs.push_back(ctx.recvTs - sendTs);
        }
    }

    hsil_destroy(handle);

    /* ------------------------------------------------------------------ */
    /* Print per-sample results then summary statistics                    */
    /* ------------------------------------------------------------------ */
    const size_t n = ctx.rttNs.size();

    if (n == 0)
    {
        std::fprintf(stderr, "[Pinger] No pongs received — is the Ponger running?\n");
        return EXIT_FAILURE;
    }

    if (static_cast<int>(n) < opts.count)
    {
        std::printf("\nWARNING: expected %d samples, got %zu "
                    "(%d pong(s) timed out or arrived during warm-up).\n",
                    opts.count, n, opts.count - static_cast<int>(n));
    }

    std::printf("\n--- Per-sample RTT (µs) ---\n");
    for (size_t k = 0; k < n; ++k)
    {
        std::printf("  [%4zu] RTT=%8.2f µs\n",
                    k + 1,
                    static_cast<double>(ctx.rttNs[k]) * 1e-3);
    }

    const Stats rttStats = computeStats(ctx.rttNs);
    printStats(rttStats, n, opts.payloadSize);

    return 0;
}

/*----------------------------------------------------------------------------*/
/* PONGER THREAD WRAPPER (single-machine mode)                                 */
/*----------------------------------------------------------------------------*/


static void pongerThreadFunc(const Options& opts)
{
    runPonger(opts);
}

/*----------------------------------------------------------------------------*/
/* ENTRY POINT                                                                 */
/*----------------------------------------------------------------------------*/

int main(int argc, char* argv[])
{
    Options opts;
    if (!parseArgs(argc, argv, opts))
    {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    std::printf("=== HSIL CoSim Round-Trip Latency Benchmark ===\n");

    /* ------------------------------------------------------------------ */
    /* Two-machine mode: run a single role and exit                        */
    /* ------------------------------------------------------------------ */
    if (opts.role == Role::Ponger)
    {
        std::printf("Mode: Two-machine (Ponger in this process)\n\n");
        return runPonger(opts);
    }
    if (opts.role == Role::Pinger)
    {
        std::printf("Mode: Two-machine (Pinger in this process)\n");
        std::printf("Make sure the Ponger is running on another machine before starting the Pinger.\n\n");
        return runPinger(opts);
    }

    /* ------------------------------------------------------------------ */
    /* Single-machine mode: start the Ponger in a background thread        */
    /* ------------------------------------------------------------------ */
    std::printf("Mode: single-machine (Ping-Pong in this process)\n\n");

    /* Run Ponger from a background thread. The Pinger waits for discovery
     * itself, so no fixed start-up delay is needed here. */
    std::thread pongerThread(pongerThreadFunc, opts);

    /* Run Pinger in the main thread. */
    const int pingerResult = runPinger(opts);

    pongerThread.join();

    return pingerResult;
}
