#include "MapBasedGlobalLockImpl.h"

#include <mutex>

namespace Afina {
namespace Backend {

bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
    std::unique_lock<std::recursive_mutex> __lock(mutex);

    if (_backend.find(key) != _backend.end()) {
        _backend[key] = pair_st_str(_stamp, value);
        _stamp++;

        __lock.unlock();
        return true;
    }

    if (_backend.size() == _max_size)
        Delete(FindOldKey());

    bool result = PutIfAbsent(key, value);

    __lock.unlock();

    return result;
}

bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
    std::unique_lock<std::recursive_mutex> __lock(mutex);

    auto inserted_value = std::pair<std::string, pair_st_str>(key, pair_st_str(_stamp, value));
    _stamp++;

    bool result = _backend.insert(inserted_value).second;

    __lock.unlock();
    return result;
}

bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
    std::unique_lock<std::recursive_mutex> __lock(mutex);

    if (_backend.find(key) == _backend.end()) {
        __lock.unlock();
        return false;
    }

    _backend[key] = pair_st_str(_stamp, value);
    _stamp++;

    __lock.unlock();
    return true;
}

bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
    std::unique_lock<std::recursive_mutex> __lock(mutex);

    auto key_del = _backend.find(key);
    if (key_del == _backend.end()) {
        __lock.unlock();
        return false;
    }

    _backend.erase(key_del);
    __lock.unlock();
    return true;
}

bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) {
    std::unique_lock<std::recursive_mutex> __lock(mutex);

    if (_backend.find(key) == _backend.end()) {
        __lock.unlock();
        return false;
    }

    value = _backend.at(key).second;
    __lock.unlock();
    return true;
}

std::string MapBasedGlobalLockImpl::FindOldKey() {
    size_t min = _stamp;
    std::string result_key;

    std::unique_lock<std::recursive_mutex> __lock(mutex);
    for (auto el : _backend) {
        if (el.second.first < min) {
            min = el.second.first;
            result_key = el.first;
        }
    }
    __lock.unlock();

    return result_key;
}

} // namespace Backend
} // namespace Afina
