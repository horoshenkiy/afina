#include "MapBasedGlobalLockImpl.h"

#include <mutex>

namespace Afina {
namespace Backend {

bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
    if (_backend.find(key) != _backend.end()) {
        _backend[key] = pair_st_str(_stamp, value);
        _stamp++;

        return true;
    }

    if (_backend.size() == _max_size)
        Delete(FindOldKey());

    return PutIfAbsent(key, value);
}


bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
    auto inserted_value = std::pair<std::string, pair_st_str>(key, pair_st_str(_stamp, value));
    _stamp++;

    return _backend.insert(inserted_value).second;
}


bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
    if (_backend.find(key) == _backend.end())
        return false;

    _backend[key] = pair_st_str(_stamp, value);
    _stamp++;

    return true;
}


bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
    auto key_del = _backend.find(key);
    if (key_del == _backend.end())
        return false;

    _backend.erase(key_del);
    return true;
}

bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
    if (_backend.find(key) == _backend.end())
        return false;

    value = _backend.at(key).second;
    return true;
}

std::string MapBasedGlobalLockImpl::FindOldKey() {
    size_t min = _stamp;
    std::string result_key;

    for (auto el : _backend) {
        if (el.second.first < min) {
            min = el.second.first;
            result_key = el.first;
        }
    }

    return result_key;
}

} // namespace Backend
} // namespace Afina
