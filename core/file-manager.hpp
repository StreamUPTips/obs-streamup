#ifndef STREAMUP_FILE_MANAGER_HPP
#define STREAMUP_FILE_MANAGER_HPP

#include <obs.h>
#include <obs-data.h>
#include <obs-frontend-api.h>
#include <QString>

namespace StreamUP {
namespace FileManager {

//-------------------RESIZE AND SCALING FUNCTIONS-------------------
/**
 * Resize Advanced Mask filter settings by a factor
 * @param filter The filter source to resize
 * @param factor The scaling factor to apply
 */
void ResizeAdvancedMaskFilter(obs_source_t *filter, float factor);

/**
 * Resize move settings (position/bounds) by a factor
 * @param obs_data The data object containing position settings
 * @param factor The scaling factor to apply
 */
void ResizeMoveSetting(obs_data_t *obs_data, float factor);

/**
 * Resize Move Value filter settings by a factor
 * @param filter The Move Value filter source to resize
 * @param factor The scaling factor to apply
 */
void ResizeMoveValueFilter(obs_source_t *filter, float factor);

/**
 * Check if a source is cloning a scene or group
 * @param source The source to check
 * @return bool True if the source is cloning a scene or group
 */
bool IsCloningSceneOrGroup(obs_source_t *source);

/**
 * Resize move filters callback for scene items
 * @param parent The parent source
 * @param child The child source/filter
 * @param param The scaling factor as void*
 */
void ResizeMoveFilters(obs_source_t *parent, obs_source_t *child, void *param);

/**
 * Resize scene items by a scaling factor
 * @param settings The scene item settings
 * @param factor The scaling factor to apply
 */
void ResizeSceneItems(obs_data_t *settings, float factor);

//-------------------PATH CONVERSION FUNCTIONS-------------------
/**
 * Convert a setting path to be relative to the .StreamUP file location
 * @param settings The settings object containing the path
 * @param setting_name The name of the setting to convert
 * @param path The base path of the .StreamUP file
 * @param sub_folder The subfolder to look in for the file
 */
void ConvertSettingPath(obs_data_t *settings, const char *setting_name, const QString &path, const char *sub_folder);

/**
 * Convert filter paths to be relative to the .StreamUP file location
 * @param filter_data The filter data containing paths
 * @param path The base path of the .StreamUP file
 */
void ConvertFilterPaths(obs_data_t *filter_data, const QString &path);

/**
 * Convert source paths to be relative to the .StreamUP file location
 * @param source_data The source data containing paths
 * @param path The base path of the .StreamUP file
 */
void ConvertSourcePaths(obs_data_t *source_data, const QString &path);

//-------------------SCENE LOADING FUNCTIONS-------------------
/**
 * Merge scene settings with existing scene
 * @param source The scene source to merge with
 * @param scene_settings The scene settings to merge
 */
void MergeScenes(obs_source_t *source, obs_data_t *scene_settings);

/**
 * Merge filters with existing source
 * @param source The source to add filters to
 * @param filters The filters array to merge
 */
void MergeFilters(obs_source_t *source, obs_data_array_t *filters);

/**
 * Load sources from .StreamUP data
 * @param data The sources data array
 * @param path The base path of the .StreamUP file
 */
void LoadSources(obs_data_array_t *data, const QString &path);

/**
 * Load a complete scene from .StreamUP data
 * @param data The scene data
 * @param path The base path of the .StreamUP file
 */
void LoadScene(obs_data_t *data, const QString &path);

//-------------------MAIN LOADING FUNCTIONS-------------------
/**
 * Load a .StreamUP file from a specific path
 * @param file_path The path to the .StreamUP file
 * @param forceLoad Whether to skip plugin requirement checks
 * @return bool True if file was loaded successfully
 */
bool LoadStreamupFileFromPath(const QString &file_path, bool forceLoad = false);

/**
 * Show file dialog and load a .StreamUP file
 * @param forceLoad Whether to skip plugin requirement checks
 */
void LoadStreamupFile(bool forceLoad = false);

} // namespace FileManager
} // namespace StreamUP

#endif // STREAMUP_FILE_MANAGER_HPP