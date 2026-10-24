#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_set>

#include "SceTypes.hpp"

extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" int APS5_VABI getpid_nid_postfix();
extern "C" int APS5_VABI cpuset_getaffinity_nid_postfix(int level, int which, std::int64_t id, std::size_t size, void* mask);

namespace {

constexpr int LevelRoot = 1;
constexpr int LevelCpuset = 2;
constexpr int LevelWhich = 3;

constexpr int WhichThread = 1;
constexpr int WhichProcess = 2;
constexpr int WhichCpuset = 3;
constexpr int WhichIrq = 4;
constexpr int WhichJail = 5;
constexpr int WhichDomain = 6;

constexpr int GuestEsrch = 3;
constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;

constexpr int RootSetId = 0;
constexpr int DefaultSetId = 1;

std::mutex setMutex;
std::unordered_set<int> validSets{RootSetId, DefaultSetId};
int nextSetId = 2;
int currentProcessSetId = DefaultSetId;

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

} // namespace

extern "C" {

int APS5_VABI cpuset_nid_postfix(int* setid) {
    if (setid == nullptr) {
        return Fail(GuestEfault);
    }
    int id = 0;
    {
        std::lock_guard lock(setMutex);
        id = nextSetId++;
        validSets.insert(id);
        currentProcessSetId = id;
    }
    *setid = id;
    return 0;
}

int APS5_VABI cpuset_setid_nid_postfix(int which, std::int64_t id, int setid) {
    if (which != WhichProcess) {
        return Fail(GuestEinval);
    }
    if (id != -1 && id != getpid_nid_postfix()) {
        return Fail(GuestEsrch);
    }
    std::lock_guard lock(setMutex);
    if (setid == -1 || validSets.find(setid) == validSets.end()) {
        return Fail(GuestEsrch);
    }
    currentProcessSetId = setid;
    return 0;
}

int APS5_VABI cpuset_getid_nid_postfix(int level, int which, std::int64_t id, int* setid) {
    if (setid == nullptr) {
        return Fail(GuestEfault);
    }
    if (level < LevelRoot || level > LevelWhich) {
        return Fail(GuestEinval);
    }
    if (level == LevelWhich && which != WhichCpuset) {
        return Fail(GuestEinval);
    }
    if (which < WhichThread || which > WhichDomain || which == WhichIrq || which == WhichDomain) {
        return Fail(GuestEinval);
    }
    if (which == WhichJail) {
        return Fail(GuestEsrch);
    }
    int targetSet = 0;
    {
        std::lock_guard lock(setMutex);
        targetSet = currentProcessSetId;
    }
    if (which == WhichProcess) {
        if (id != -1 && id != getpid_nid_postfix()) {
            return Fail(GuestEsrch);
        }
    } else if (which == WhichThread) {
        if (id != -1) {
            std::uint64_t affinity = 0;
            if (cpuset_getaffinity_nid_postfix(LevelWhich, WhichThread, id, sizeof(affinity), &affinity) != 0) {
                return Fail(GuestEsrch);
            }
        }
    } else if (which == WhichCpuset) {
        if (id < std::numeric_limits<int>::min() || id > std::numeric_limits<int>::max()) {
            return Fail(GuestEsrch);
        }
        std::lock_guard lock(setMutex);
        if (id == -1) {
            targetSet = currentProcessSetId;
        } else {
            const auto it = validSets.find(static_cast<int>(id));
            if (it == validSets.end()) {
                return Fail(GuestEsrch);
            }
            targetSet = *it;
        }
    }
    if (level == LevelRoot) {
        *setid = RootSetId;
    } else {
        *setid = targetSet;
    }
    return 0;
}

}
