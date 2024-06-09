#ifndef STREAMUP_HPP
#define STREAMUP_HPP

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>

bool ToggleLockAllSources(bool sendNotification);
bool ToggleLockSourcesInCurrentScene(bool sendNotification);
bool RefreshBrowserSources(void *data, obs_source_t *source);
bool RefreshAudioMonitoring(void *data, obs_source_t *source);
bool AreAllSourcesLockedInCurrentScene();
bool AreAllSourcesLockedInAllScenes();
bool CheckIfAnyUnlocked(obs_scene_t *scene);
bool CheckIfAnyUnlockedInAllScenes();

#endif
