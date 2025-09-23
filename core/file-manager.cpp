#include "file-manager.hpp"
#include "streamup-common.hpp"
#include "plugin-manager.hpp"
#include "plugin-state.hpp"
#include "obs-wrappers.hpp"
#include "error-handler.hpp"
#include "string-utils.hpp"
#include "version-utils.hpp"
#include "path-utils.hpp"
#include <obs-module.h>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QApplication>
#include <list>
#include <map>

namespace StreamUP {
namespace FileManager {

//-------------------RESIZE AND SCALING FUNCTIONS-------------------
void ResizeAdvancedMaskFilter(obs_source_t *filter, float factor)
{
	if (!StreamUP::ErrorHandler::ValidateSource(filter, "ResizeAdvancedMaskFilter")) {
		return;
	}

	auto settings = OBSWrappers::OBSDataPtr(obs_source_get_settings(filter));
	if (!settings) {
		StreamUP::ErrorHandler::LogError("Failed to get settings for advanced mask filter", StreamUP::ErrorHandler::Category::Source);
		return;
	}

	for (size_t i = 0; i < ADVANCED_MASKS_SETTINGS_SIZE; i++) {
		if (obs_data_has_user_value(settings.get(), advanced_mask_settings[i]))
			obs_data_set_double(settings.get(), advanced_mask_settings[i],
					    obs_data_get_double(settings.get(), advanced_mask_settings[i]) * factor);
	}
	obs_source_update(filter, settings.get());
}

void ResizeMoveSetting(obs_data_t *obs_data, float factor)
{
	if (!obs_data) return;
	
	double x = obs_data_get_double(obs_data, "x");
	obs_data_set_double(obs_data, "x", x * factor);
	double y = obs_data_get_double(obs_data, "y");
	obs_data_set_double(obs_data, "y", y * factor);
	// Note: obs_data passed in is managed externally, don't release here
}

void ResizeMoveValueFilter(obs_source_t *filter, float factor)
{
	if (!StreamUP::ErrorHandler::ValidateSource(filter, "ResizeMoveValueFilter")) {
		return;
	}

	auto settings = OBSWrappers::OBSDataPtr(obs_source_get_settings(filter));
	if (!settings) {
		StreamUP::ErrorHandler::LogError("Failed to get settings for move value filter", StreamUP::ErrorHandler::Category::Source);
		return;
	}
	if (obs_data_get_int(settings.get(), "move_value_type") == 1) {
		for (size_t i = 0; i < ADVANCED_MASKS_SETTINGS_SIZE; i++) {
			if (obs_data_has_user_value(settings.get(), advanced_mask_settings[i]))
				obs_data_set_double(settings.get(), advanced_mask_settings[i],
						    obs_data_get_double(settings.get(), advanced_mask_settings[i]) * factor);
		}
	} else {
		const char *setting_name = obs_data_get_string(settings.get(), "setting_name");
		for (size_t i = 0; i < ADVANCED_MASKS_SETTINGS_SIZE; i++) {
			if (strcmp(setting_name, advanced_mask_settings[i]) == 0) {
				if (obs_data_has_user_value(settings.get(), "setting_float"))
					obs_data_set_double(settings.get(), "setting_float",
							    obs_data_get_double(settings.get(), "setting_float") * factor);
				if (obs_data_has_user_value(settings.get(), "setting_float_min"))
					obs_data_set_double(settings.get(), "setting_float_min",
							    obs_data_get_double(settings.get(), "setting_float_min") * factor);
				if (obs_data_has_user_value(settings.get(), "setting_float_max"))
					obs_data_set_double(settings.get(), "setting_float_max",
							    obs_data_get_double(settings.get(), "setting_float_max") * factor);
			}
		}
	}

	obs_source_update(filter, settings.get());
}

bool IsCloningSceneOrGroup(obs_source_t *source)
{
	if (!source)
		return false;

	const char *input_kind = obs_source_get_id(source);
	if (strcmp(input_kind, "source-clone") != 0)
		return false; // Not a source-clone, no need to check further.

	obs_data_t *source_settings = obs_source_get_settings(source);
	if (!source_settings)
		return false;

	if (obs_data_get_int(source_settings, "clone_type")) {
		obs_data_release(source_settings);
		return true;
	}

	const char *cloned_source_name = obs_data_get_string(source_settings, "clone");
	obs_source_t *cloned_source = obs_get_source_by_name(cloned_source_name);
	if (!cloned_source) {
		obs_data_release(source_settings);
		return false;
	}

	const char *cloned_source_kind = obs_source_get_unversioned_id(cloned_source);
	bool is_scene_or_group = (strcmp(cloned_source_kind, "scene") == 0 || strcmp(cloned_source_kind, "group") == 0);

	obs_source_release(cloned_source);
	obs_data_release(source_settings);

	return is_scene_or_group;
}

void ResizeMoveFilters(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);
	float factor = *((float *)param);

	const char *filter_id = obs_source_get_unversioned_id(child);

	if (strcmp(filter_id, "move_source_filter") == 0) {
		obs_data_t *settings = obs_source_get_settings(child);
		ResizeMoveSetting(obs_data_get_obj(settings, "pos"), factor);
		ResizeMoveSetting(obs_data_get_obj(settings, "bounds"), factor);
		const char *source_name = obs_data_get_string(settings, "source");
		obs_source_t *source = (source_name && strlen(source_name)) ? obs_get_source_by_name(source_name) : nullptr;
		// Skip resize if cloning a Scene or Group
		if (!obs_scene_from_source(source) && !obs_group_from_source(source) && !IsCloningSceneOrGroup(source)) {
			ResizeMoveSetting(obs_data_get_obj(settings, "scale"), factor);
		}
		obs_source_release(source);
		obs_data_set_string(settings, "transform_text", "");
		obs_data_release(settings);
	} else if (strcmp(filter_id, "advanced_masks_filter") == 0) {
		ResizeAdvancedMaskFilter(child, factor);
	} else if (strcmp(filter_id, "move_value_filter") == 0) {
		ResizeMoveValueFilter(child, factor);
	}
}

void ResizeSceneItems(obs_data_t *settings, float factor)
{
	obs_data_array_t *items = obs_data_get_array(settings, "items");
	size_t count = obs_data_array_count(items);

	if (obs_data_get_bool(settings, "custom_size")) {
		obs_data_set_int(settings, "cx", obs_data_get_int(settings, "cx") * factor);
		obs_data_set_int(settings, "cy", obs_data_get_int(settings, "cy") * factor);
	}

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item_data = obs_data_array_item(items, i);
		const char *name = obs_data_get_string(item_data, "name");
		obs_source_t *item_source = obs_get_source_by_name(name);

		vec2 vec2;
		obs_data_get_vec2(item_data, "pos", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "pos", &vec2);
		obs_data_get_vec2(item_data, "bounds", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "bounds", &vec2);

		// Skip resizing if it's a source-clone and cloning a Scene or Group
		if (item_source && (obs_scene_from_source(item_source) || obs_group_from_source(item_source) ||
				    IsCloningSceneOrGroup(item_source))) {
			obs_data_get_vec2(item_data, "scale_ref", &vec2);
			vec2.x *= factor;
			vec2.y *= factor;
			obs_data_set_vec2(item_data, "scale_ref", &vec2);
		} else {
			obs_data_get_vec2(item_data, "scale", &vec2);
			vec2.x *= factor;
			vec2.y *= factor;
			obs_data_set_vec2(item_data, "scale", &vec2);
		}

		obs_source_release(item_source);
		obs_data_release(item_data);
	}

	obs_data_array_release(items);
}

//-------------------PATH CONVERSION FUNCTIONS-------------------
void ConvertSettingPath(obs_data_t *settings, const char *setting_name, const QString &path, const char *sub_folder)
{
	const char *file = obs_data_get_string(settings, setting_name);
	if (!file || !strlen(file))
		return;
	if (QFile::exists(file))
		return;
	const QString file_name = QFileInfo(QT_UTF8(file)).fileName();
	QString filePath = path + "/Resources/" + sub_folder + "/" + file_name;
	if (QFile::exists(filePath)) {
		obs_data_set_string(settings, setting_name, QT_TO_UTF8(filePath));
		return;
	}
	filePath = path + "/" + sub_folder + "/" + file_name;
	if (QFile::exists(filePath)) {
		obs_data_set_string(settings, setting_name, QT_TO_UTF8(filePath));
	}
}

void ConvertFilterPaths(obs_data_t *filter_data, const QString &path)
{
	const char *id = obs_data_get_string(filter_data, "id");
	if (strcmp(id, "shader_filter") == 0) {
		obs_data_t *settings = obs_data_get_obj(filter_data, "settings");
		ConvertSettingPath(settings, "shader_file_name", path, "Shader Filters");
		obs_data_release(settings);
	} else if (strcmp(id, "mask_filter") == 0) {
		obs_data_t *settings = obs_data_get_obj(filter_data, "settings");
		ConvertSettingPath(settings, "image_path", path, "Image Masks");
		obs_data_release(settings);
	}
}

void ConvertSourcePaths(obs_data_t *source_data, const QString &path)
{
	const char *id = obs_data_get_string(source_data, "id");
	if (strcmp(id, "image_source") == 0) {
		obs_data_t *settings = obs_data_get_obj(source_data, "settings");
		ConvertSettingPath(settings, "file", path, "Image Sources");
		obs_data_release(settings);
	} else if (strcmp(id, "ffmpeg_source") == 0) {
		obs_data_t *settings = obs_data_get_obj(source_data, "settings");
		ConvertSettingPath(settings, "local_file", path, "Media Sources");
		obs_data_release(settings);
	}
	obs_data_array_t *filters = obs_data_get_array(source_data, "filters");
	if (!filters)
		return;
	const size_t count = obs_data_array_count(filters);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *filter_data = obs_data_array_item(filters, i);
		ConvertFilterPaths(filter_data, path);
		obs_data_release(filter_data);
	}
	obs_data_array_release(filters);
}

//-------------------SCENE LOADING FUNCTIONS-------------------
void MergeScenes(obs_source_t *s, obs_data_t *scene_settings)
{
	obs_source_save(s);
	obs_data_array_t *items = obs_data_get_array(scene_settings, "items");
	const size_t item_count = obs_data_array_count(items);
	obs_data_t *scene_settings_orig = obs_source_get_settings(s);
	obs_data_array_t *items_orig = obs_data_get_array(scene_settings_orig, "items");
	const size_t item_count_orig = obs_data_array_count(items_orig);
	for (size_t j = 0; j < item_count_orig; j++) {
		obs_data_t *item_data_orig = obs_data_array_item(items_orig, j);
		const char *name_orig = obs_data_get_string(item_data_orig, "name");
		bool found = false;
		for (size_t k = 0; k < item_count; k++) {
			obs_data_t *item_data = obs_data_array_item(items, k);
			const char *name = obs_data_get_string(item_data, "name");
			if (strcmp(name, name_orig) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			obs_data_array_push_back(items, item_data_orig);
		}
		obs_data_release(item_data_orig);
	}
	obs_data_array_release(items_orig);
	obs_data_release(scene_settings_orig);
	obs_data_array_release(items);
}

void MergeFilters(obs_source_t *s, obs_data_array_t *filters)
{
	size_t count = obs_data_array_count(filters);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *filter_data = obs_data_array_item(filters, i);
		const char *filter_name = obs_data_get_string(filter_data, "name");
		obs_source_t *filter = obs_source_get_filter_by_name(s, filter_name);
		if (filter) {
			obs_data_release(filter_data);
			obs_source_release(filter);
			continue;
		}
		filter = obs_load_private_source(filter_data);
		if (filter) {
			obs_source_filter_add(s, filter);
			obs_source_release(filter);
		}
		obs_data_release(filter_data);
	}

	obs_data_array_release(filters);
}

void LoadSources(obs_data_array_t *data, const QString &path)
{
	const size_t count = obs_data_array_count(data);
	std::list<obs_source_t *> ref_sources;
	std::list<obs_source_t *> load_sources;
	obs_source_t *cs = obs_frontend_get_current_scene();
	uint32_t w = obs_source_get_width(cs);
	float factor = (float)w / 1920.0f;
	obs_source_release(cs);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *sourceData = obs_data_array_item(data, i);
		const char *name = obs_data_get_string(sourceData, "name");

		bool new_source = true;
		obs_source_t *s = obs_get_source_by_name(name);
		if (!s) {
			ConvertSourcePaths(sourceData, path);
			s = obs_load_source(sourceData);
			load_sources.push_back(s);
		} else {
			new_source = false;
			obs_data_array_t *filters = obs_data_get_array(sourceData, "filters");
			if (filters) {
				MergeFilters(s, filters);
				obs_data_array_release(filters);
			}
		}
		if (s)
			ref_sources.push_back(s);
		obs_scene_t *scene = obs_scene_from_source(s);
		if (!scene)
			scene = obs_group_from_source(s);
		if (scene) {
			obs_data_t *scene_settings = obs_data_get_obj(sourceData, "settings");
			if (w != 1920) {
				ResizeSceneItems(scene_settings, factor);
				if (new_source)
					obs_source_enum_filters(s, ResizeMoveFilters, &factor);
			}
			if (!new_source) {
				obs_scene_enum_items(
					scene,
					[](obs_scene_t *, obs_sceneitem_t *item, void *d) {
						std::list<obs_source_t *> *sources = (std::list<obs_source_t *> *)d;
						obs_source_t *si = obs_sceneitem_get_source(item);
						si = obs_source_get_ref(si);
						if (si)
							sources->push_back(si);
						return true;
					},
					&ref_sources);
				MergeScenes(s, scene_settings);
			}
			obs_source_update(s, scene_settings);
			obs_data_release(scene_settings);
			load_sources.push_back(s);
		}
		obs_data_release(sourceData);
	}

	for (obs_source_t *source : load_sources)
		obs_source_load(source);

	for (obs_source_t *source : ref_sources)
		obs_source_release(source);
}

void LoadScene(obs_data_t *data, const QString &path)
{
	if (!data)
		return;
	obs_data_array_t *sourcesData = obs_data_get_array(data, "sources");
	if (!sourcesData)
		return;
	LoadSources(sourcesData, path);
	obs_data_array_release(sourcesData);
}

//-------------------MAIN LOADING FUNCTIONS-------------------
bool LoadStreamupFileFromPath(const QString &file_path, bool forceLoad)
{
	if (!forceLoad) {
		if (!StreamUP::PluginManager::CheckrequiredOBSPlugins(true)) {
			return false;
		}
	}

	// Validate file path first
	if (!StreamUP::ErrorHandler::ValidateFile(file_path)) {
		return false;
	}

	// Load the .streamup file from the file path
	auto data = OBSWrappers::MakeOBSDataFromJsonFile(QT_TO_UTF8(file_path));
	if (data) {
		StreamUP::ErrorHandler::LogInfo("Successfully loaded StreamUP file: " + file_path.toStdString(), StreamUP::ErrorHandler::Category::FileSystem);
		LoadScene(data.get(), QFileInfo(file_path).absolutePath());
		return true;
	} else {
		StreamUP::ErrorHandler::LogError("Failed to parse StreamUP file: " + file_path.toStdString(), StreamUP::ErrorHandler::Category::FileSystem);
	}

	return false;
}

void LoadStreamupFile(bool forceLoad)
{
	if (!forceLoad) {
		if (!StreamUP::PluginManager::CheckrequiredOBSPlugins(true)) {
			return;
		}
	}

	QString fileName =
		QFileDialog::getOpenFileName(nullptr, QT_UTF8(obs_module_text("UI.Button.Load")), QString(), "StreamUP File (*.streamup)");
	if (!fileName.isEmpty()) {
		LoadStreamupFileFromPath(fileName, forceLoad);
	}
}

void LoadStreamupFileWithWarning()
{
	// Use cached plugin status for instant response
	if (StreamUP::PluginManager::IsAllPluginsUpToDateCached()) {
		// Plugins are OK, directly open the Install Product dialog (file selector)
		QString fileName =
			QFileDialog::getOpenFileName(nullptr, QT_UTF8(obs_module_text("UI.Button.Load")), QString(), "StreamUP File (*.streamup)");
		if (!fileName.isEmpty()) {
			LoadStreamupFileFromPath(fileName, false);
		}
		return;
	}

	// Plugins have issues, show cached plugin issues dialog with continue callback
	StreamUP::PluginManager::ShowCachedPluginIssuesDialog([]() {
		// Continue anyway callback - load with force
		LoadStreamupFile(true);
	});
}

} // namespace FileManager
} // namespace StreamUP
