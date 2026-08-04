#include <cstdlib>
#include <stdint.h>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <android-base/file.h>

static constexpr const char* FLASH_NODE = "/sys/class/camera/flash/rear_flash";
// SM5714 driver: 1001-1008 map to 50-225mA in 25mA steps (register offset 0x0-0x7),
// >= 1009 maps to 225mA (0x7).
static const int32_t kLevelToRawValue[] = {0, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008};

static std::mutex gStateMutex;
static int32_t gLevel = 0;  // last requested strength level, 0 = off
static int32_t gRaw = 0;    // last raw value written to the node, 0 = off

bool supportsTorchStrengthControlExt() { return true; }
int32_t getTorchDefaultStrengthLevelExt() { return 1; }
int32_t getTorchMaxStrengthLevelExt() { return 8; }

int32_t getTorchStrengthLevelExt() {
    std::lock_guard<std::mutex> lock(gStateMutex);
    return gLevel;
}

void setTorchStrengthLevelExt(int32_t torchStrength, bool enabled) {
    int level = enabled ? torchStrength : 0;
    if (level < 0) level = 0;
    if (level > 8) level = 8;
    int raw = kLevelToRawValue[level];

    {
        std::lock_guard<std::mutex> lock(gStateMutex);
        gLevel = level;
        gRaw = raw;
    }
    android::base::WriteStringToFile(std::to_string(raw), FLASH_NODE);

    if (raw > 0) {
        std::thread([level, raw]() {
            for (int attempt = 0; attempt < 3; ++attempt) {
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
                {
                    std::lock_guard<std::mutex> lock(gStateMutex);
                    if (gLevel != level || gRaw != raw) return;  // superseded
                }
                std::string value;
                if (android::base::ReadFileToString(FLASH_NODE, &value) &&
                        atoi(value.c_str()) == raw) {
                    return;  // already holds our value
                }
                android::base::WriteStringToFile(std::to_string(raw), FLASH_NODE);
            }
        }).detach();
    }
}
