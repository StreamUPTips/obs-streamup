#ifndef STREAMUP_OBS_WRAPPERS_HPP
#define STREAMUP_OBS_WRAPPERS_HPP

#include <memory>
#include <functional>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-data.h>

namespace StreamUP {
namespace OBSWrappers {

// Custom deleters for OBS objects
struct OBSDataDeleter {
    void operator()(obs_data_t* data) const {
        if (data) {
            obs_data_release(data);
        }
    }
};

struct OBSDataArrayDeleter {
    void operator()(obs_data_array_t* array) const {
        if (array) {
            obs_data_array_release(array);
        }
    }
};

struct OBSSourceDeleter {
    void operator()(obs_source_t* source) const {
        if (source) {
            obs_source_release(source);
        }
    }
};

// RAII wrapper types
using OBSDataPtr = std::unique_ptr<obs_data_t, OBSDataDeleter>;
using OBSDataArrayPtr = std::unique_ptr<obs_data_array_t, OBSDataArrayDeleter>;
using OBSSourcePtr = std::unique_ptr<obs_source_t, OBSSourceDeleter>;

// Factory functions for creating RAII wrappers
inline OBSDataPtr MakeOBSData() {
    return OBSDataPtr(obs_data_create());
}

inline OBSDataPtr MakeOBSDataFromJson(const char* json_string) {
    return OBSDataPtr(obs_data_create_from_json(json_string));
}

inline OBSDataPtr MakeOBSDataFromJsonFile(const char* json_file) {
    return OBSDataPtr(obs_data_create_from_json_file(json_file));
}

inline OBSDataArrayPtr MakeOBSDataArray() {
    return OBSDataArrayPtr(obs_data_array_create());
}

inline OBSSourcePtr MakeOBSSource(const char* name) {
    return OBSSourcePtr(obs_get_source_by_name(name));
}

inline OBSSourcePtr MakeOBSSourceRef(obs_source_t* source) {
    if (source) {
        obs_source_get_ref(source);
        return OBSSourcePtr(source);
    }
    return nullptr;
}

// Safe property access helpers
inline std::string GetStringProperty(obs_data_t* data, const char* name, const char* default_value = "") {
    if (!data || !name) return default_value;
    const char* value = obs_data_get_string(data, name);
    return value ? std::string(value) : std::string(default_value);
}

inline bool GetBoolProperty(obs_data_t* data, const char* name, bool default_value = false) {
    if (!data || !name) return default_value;
    return obs_data_get_bool(data, name);
}

inline long long GetIntProperty(obs_data_t* data, const char* name, long long default_value = 0) {
    if (!data || !name) return default_value;
    return obs_data_get_int(data, name);
}

inline double GetDoubleProperty(obs_data_t* data, const char* name, double default_value = 0.0) {
    if (!data || !name) return default_value;
    return obs_data_get_double(data, name);
}

// Safe object retrieval
inline OBSDataPtr GetObjectProperty(obs_data_t* data, const char* name) {
    if (!data || !name) return nullptr;
    return OBSDataPtr(obs_data_get_obj(data, name));
}

inline OBSDataArrayPtr GetArrayProperty(obs_data_t* data, const char* name) {
    if (!data || !name) return nullptr;
    return OBSDataArrayPtr(obs_data_get_array(data, name));
}

} // namespace OBSWrappers
} // namespace StreamUP

#endif // STREAMUP_OBS_WRAPPERS_HPP
