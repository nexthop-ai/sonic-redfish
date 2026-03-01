#include "bridge_app.hpp"
#include "logger.hpp"
#include <iostream>
#include <cstdlib>
#include <getopt.h>

using namespace sonic::dbus_bridge;

namespace
{
constexpr const char* VERSION = "1.0.0";
constexpr const char* DEFAULT_CONFIG_PATH = "/etc/sonic-dbus-bridge/config.yaml";

void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " [OPTIONS]\n\n"
              << "SONiC to D-Bus Inventory Bridge\n\n"
              << "Options:\n"
              << "  -c, --config PATH     Config file path (default: " << DEFAULT_CONFIG_PATH << ")\n"
              << "  -v, --version         Print version and exit\n"
              << "  -h, --help            Show this help message\n"
              << std::endl;
}

void printVersion()
{
    std::cout << "sonic-dbus-bridge version " << VERSION << std::endl;
}

} // anonymous namespace

int main(int argc, char* argv[])
{
    openlog("sonic-dbus-bridge", LOG_PID | LOG_NDELAY, LOG_DAEMON);
    std::string configPath = DEFAULT_CONFIG_PATH;

    // Parse command-line arguments
    static struct option longOptions[] = {
        {"config", required_argument, nullptr, 'c'},
        {"version", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:vh", longOptions, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'c':
                configPath = optarg;
                break;
            case 'v':
                printVersion();
                return EXIT_SUCCESS;
            case 'h':
                printUsage(argv[0]);
                return EXIT_SUCCESS;
            default:
                printUsage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    try
    {
        LOG_INFO("Starting sonic-dbus-bridge with config: %s", configPath.c_str());

        BridgeApp app(configPath);

        if (!app.initialize())
        {
            LOG_ERROR("Failed to initialize application");
            return EXIT_FAILURE;
        }

        LOG_INFO("Initialization complete, entering main loop...");
        return app.run();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Fatal error: %s", e.what());
        return EXIT_FAILURE;
    }
}

