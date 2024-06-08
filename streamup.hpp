#ifndef STREAMUP_HPP
#define STREAMUP_HPP

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>

bool ToggleLockAllSources();
bool ToggleLockSourcesInCurrentScene();
bool RefreshBrowserSources(void *data, obs_source_t *source);
bool RefreshAudioMonitoring(void *data, obs_source_t *source);

#endif
