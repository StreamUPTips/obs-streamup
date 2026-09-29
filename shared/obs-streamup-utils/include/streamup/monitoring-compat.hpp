#pragma once

// Audio monitoring across OBS versions.
//
// OBS 33 replaced the three-way monitoring type (Off / Monitor Only / Monitor
// and Output) with a plain on/off switch. Mute and monitoring are independent
// there: a muted source that is monitored is what "Monitor Only" used to be.
// The old type functions still exist in 33 but are deprecated, log a warning
// on every call, and quietly turn Monitor Only into Monitor and Output, so the
// audio ends up on stream.
//
// We build against an older libobs so the same binary loads on 31, 32 and 33.
// The new functions are therefore looked up at runtime rather than linked:
// when they exist we use them, otherwise we fall back to the old type API.

#include <obs.h>
#include <util/platform.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

namespace StreamUP::MonitoringCompat {

namespace detail {

using set_enabled_fn = void (*)(obs_source_t *, bool);
using get_enabled_fn = bool (*)(const obs_source_t *);
using set_type_fn = void (*)(obs_source_t *, enum obs_monitoring_type);
using get_type_fn = enum obs_monitoring_type (*)(const obs_source_t *);

inline void *find_libobs_symbol(const char *name)
{
#ifdef _WIN32
	// obs.dll is already loaded, so this just hands back its handle. Kept for
	// the life of the process on purpose: libobs never unloads before us.
	static void *libobs = os_dlopen("obs");
	return libobs ? os_dlsym(libobs, name) : nullptr;
#else
	return dlsym(RTLD_DEFAULT, name);
#endif
}

// Both generations are looked up rather than linked, so the plugin keeps
// loading on an OBS that has removed the old functions, not just deprecated
// them (OBS 33 did exactly that to the gs_image_file API).
struct Api {
	set_enabled_fn set_enabled = nullptr;
	get_enabled_fn get_enabled = nullptr;
	set_type_fn set_type = nullptr;
	get_type_fn get_type = nullptr;
};

inline const Api &api()
{
	static const Api resolved = [] {
		Api a;
		a.set_enabled = reinterpret_cast<set_enabled_fn>(
			find_libobs_symbol("obs_source_set_monitoring_enabled"));
		a.get_enabled = reinterpret_cast<get_enabled_fn>(
			find_libobs_symbol("obs_source_get_monitoring_enabled"));
		if (!a.set_enabled || !a.get_enabled) {
			a.set_enabled = nullptr;
			a.get_enabled = nullptr;
		}
		a.set_type = reinterpret_cast<set_type_fn>(find_libobs_symbol("obs_source_set_monitoring_type"));
		a.get_type = reinterpret_cast<get_type_fn>(find_libobs_symbol("obs_source_get_monitoring_type"));
		return a;
	}();
	return resolved;
}

// Only used on OBS 32 and older, where the type functions are current.
inline obs_monitoring_type legacy_get(obs_source_t *source)
{
	return api().get_type ? api().get_type(source) : OBS_MONITORING_TYPE_NONE;
}

inline void legacy_set(obs_source_t *source, obs_monitoring_type type)
{
	if (api().set_type)
		api().set_type(source, type);
}

} // namespace detail

// True on OBS 33 and newer, where monitoring is a plain on/off switch and
// "Monitor Only" no longer exists as a separate mode.
inline bool has_monitoring_switch()
{
	return detail::api().set_enabled != nullptr;
}

// True when the running OBS still has a real "Monitor Only" mode.
inline bool supports_monitor_only()
{
	return !has_monitoring_switch();
}

inline bool is_enabled(obs_source_t *source)
{
	if (!source)
		return false;
	if (has_monitoring_switch())
		return detail::api().get_enabled(source);
	return detail::legacy_get(source) != OBS_MONITORING_TYPE_NONE;
}

inline void set_enabled(obs_source_t *source, bool enabled)
{
	if (!source)
		return;
	if (has_monitoring_switch()) {
		detail::api().set_enabled(source, enabled);
		return;
	}
	detail::legacy_set(source, enabled ? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT : OBS_MONITORING_TYPE_NONE);
}

// The monitoring state in the old three-way terms. On OBS 33 a monitored,
// muted source reads as Monitor Only, since that is what it does: you hear
// it, the stream and recording do not.
inline obs_monitoring_type get_type(obs_source_t *source)
{
	if (!source)
		return OBS_MONITORING_TYPE_NONE;
	if (!has_monitoring_switch())
		return detail::legacy_get(source);
	if (!is_enabled(source))
		return OBS_MONITORING_TYPE_NONE;
	return obs_source_muted(source) ? OBS_MONITORING_TYPE_MONITOR_ONLY : OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT;
}

// Apply a three-way monitoring type. On OBS 33 the two monitoring modes are
// expressed through mute, the mirror of get_type(): Monitor Only mutes the
// source and Monitor and Output unmutes it. Off only turns monitoring off.
inline void set_type(obs_source_t *source, obs_monitoring_type type)
{
	if (!source)
		return;
	if (!has_monitoring_switch()) {
		detail::legacy_set(source, type);
		return;
	}
	set_enabled(source, type != OBS_MONITORING_TYPE_NONE);
	if (type == OBS_MONITORING_TYPE_MONITOR_ONLY)
		obs_source_set_muted(source, true);
	else if (type == OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT)
		obs_source_set_muted(source, false);
}

// Listeners can keep using the "audio_monitoring" source signal: OBS 33 still
// sends it alongside its new "monitor" signal.

} // namespace StreamUP::MonitoringCompat
