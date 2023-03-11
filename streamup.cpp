#include "streamup.hpp"
#include <obs-module.h>
#include <QFileDialog>
#include "version.h"

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("streamup", "en-US")

void ResizeMoveSetting(obs_data_t *obs_data, float factor)
{
	double x = obs_data_get_double(obs_data, "x");
	obs_data_set_double(obs_data, "x", x * factor);
	double y = obs_data_get_double(obs_data, "y");
	obs_data_set_double(obs_data, "y", y * factor);
	obs_data_release(obs_data);
}

void ResizeMoveFilters(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);
	float factor = *((float *)param);
	if (strcmp(obs_source_get_unversioned_id(child),
		   "move_source_filter") != 0)
		return;
	obs_data_t *settings = obs_source_get_settings(child);
	ResizeMoveSetting(obs_data_get_obj(settings, "pos"), factor);
	ResizeMoveSetting(obs_data_get_obj(settings, "bounds"), factor);
	const char *source_name = obs_data_get_string(settings, "source");
	obs_source_t *source = (source_name && strlen(source_name))
				       ? obs_get_source_by_name(source_name)
				       : nullptr;
	if (!obs_scene_from_source(source) && !obs_group_from_source(source)) {
		ResizeMoveSetting(obs_data_get_obj(settings, "scale"), factor);
	}
	obs_source_release(source);
	obs_data_set_string(settings, "transform_text", "");
	obs_data_release(settings);
	return;
}

void ResizeSceneItems(obs_data_t *settings, float factor)
{
	obs_data_array_t *items = obs_data_get_array(settings, "items");
	size_t count = obs_data_array_count(items);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item_data = obs_data_array_item(items, i);
		vec2 vec2;
		obs_data_get_vec2(item_data, "pos", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "pos", &vec2);
		obs_data_get_vec2(item_data, "bounds", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "bounds", &vec2);
		const char *name = obs_data_get_string(item_data, "name");
		obs_source_t *item_source = obs_get_source_by_name(name);
		obs_source_release(item_source);
		if (item_source && (obs_scene_from_source(item_source) ||
				    obs_group_from_source(item_source))) {
			obs_data_release(item_data);
			continue;
		}
		obs_data_get_vec2(item_data, "scale", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "scale", &vec2);
		obs_data_release(item_data);
	}
	obs_data_array_release(items);
}

void ConvertSettingPath(obs_data_t *settings, const char *setting_name,
			const QString &path, const char *sub_folder)
{
	const char *file = obs_data_get_string(settings, setting_name);
	if (!file || !strlen(file))
		return;
	if (QFile::exists(file))
		return;
	const QString file_name = QFileInfo(QT_UTF8(file)).fileName();
	QString filePath = path + "/Resources/" + sub_folder + "/" + file_name;
	if (QFile::exists(filePath)) {
		obs_data_set_string(settings, setting_name,
				    QT_TO_UTF8(filePath));
		return;
	}
	filePath = path + "/" + sub_folder + "/" + file_name;
	if (QFile::exists(filePath)) {
		obs_data_set_string(settings, setting_name,
				    QT_TO_UTF8(filePath));
	}
}

void ConvertFilterPaths(obs_data_t *filter_data, const QString &path)
{
	const char *id = obs_data_get_string(filter_data, "id");
	if (strcmp(id, "shader_filter") == 0) {
		obs_data_t *settings =
			obs_data_get_obj(filter_data, "settings");
		ConvertSettingPath(settings, "shader_file_name", path,
				   "Shader Filters");
		obs_data_release(settings);
	} else if (strcmp(id, "mask_filter") == 0) {
		obs_data_t *settings =
			obs_data_get_obj(filter_data, "settings");
		ConvertSettingPath(settings, "image_path", path, "Image Masks");
		obs_data_release(settings);
	}
}

void ConvertSourcePaths(obs_data_t *source_data, const QString &path)
{
	const char *id = obs_data_get_string(source_data, "id");
	if (strcmp(id, "image_source") == 0) {
		obs_data_t *settings =
			obs_data_get_obj(source_data, "settings");
		ConvertSettingPath(settings, "file", path, "Image Sources");
		obs_data_release(settings);
	} else if (strcmp(id, "ffmpeg_source") == 0) {
		obs_data_t *settings =
			obs_data_get_obj(source_data, "settings");
		ConvertSettingPath(settings, "local_file", path,
				   "Media Sources");
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

static void merge_scenes(obs_source_t *s, obs_data_t *scene_settings)
{
	obs_source_save(s);
	obs_data_array_t *items = obs_data_get_array(scene_settings, "items");
	const size_t item_count = obs_data_array_count(items);
	obs_data_t *scene_settings_orig = obs_source_get_settings(s);
	obs_data_array_t *items_orig =
		obs_data_get_array(scene_settings_orig, "items");
	const size_t item_count_orig = obs_data_array_count(items_orig);
	for (size_t j = 0; j < item_count_orig; j++) {
		obs_data_t *item_data_orig = obs_data_array_item(items_orig, j);
		const char *name_orig =
			obs_data_get_string(item_data_orig, "name");
		bool found = false;
		for (size_t k = 0; k < item_count; k++) {
			obs_data_t *item_data = obs_data_array_item(items, k);
			const char *name =
				obs_data_get_string(item_data, "name");
			if (strcmp(name, name_orig) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			obs_data_array_push_back(items, item_data_orig);
		}
	}
	obs_data_array_release(items_orig);
	obs_data_release(scene_settings_orig);
	obs_data_array_release(items);
}

static void merge_filters(obs_source_t *s, obs_data_array_t *filters)
{
	size_t count = obs_data_array_count(filters);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *filter_data = obs_data_array_item(filters, i);
		const char *filter_name = obs_data_get_string(filter_data, "name");
		obs_source_t *filter = obs_source_get_filter_by_name(s,
								  filter_name);
		if (filter) {
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

static void LoadSources(obs_data_array_t *data, QString path)
{
	const size_t count = obs_data_array_count(data);
	std::vector<obs_source_t *> sources;
	sources.reserve(count);
	uint32_t w = obs_source_get_width(obs_frontend_get_current_scene());
	float factor = (float)w / 1920.0f;
	for (size_t i = 0; i < count; i++) {
		obs_data_t *sourceData = obs_data_array_item(data, i);
		const char *name = obs_data_get_string(sourceData, "name");

		bool new_source = true;
		obs_source_t *s = obs_get_source_by_name(name);
		if (!s) {
			ConvertSourcePaths(sourceData, path);
			s = obs_load_source(sourceData);
		} else {
			new_source = false;
			obs_data_array_t *filters =
				obs_data_get_array(sourceData, "filters");
			if (filters) {
				merge_filters(s, filters);
				obs_data_array_release(filters);
			}
		}
		if (s)
			sources.push_back(s);
		obs_scene_t *scene = obs_scene_from_source(s);
		if (!scene)
			scene = obs_group_from_source(s);
		if (scene) {
			obs_data_t *scene_settings =
				obs_data_get_obj(sourceData, "settings");
			if (w != 1920) {
				ResizeSceneItems(scene_settings, factor);
				if (new_source)
					obs_source_enum_filters(
						s, ResizeMoveFilters, &factor);
			}
			if (!new_source) {
				merge_scenes(s, scene_settings);
			}
			obs_source_update(s, scene_settings);
			obs_data_release(scene_settings);
		}
		obs_data_release(sourceData);
	}

	for (obs_source_t *source : sources)
		obs_source_load(source);

	for (obs_source_t *source : sources)
		obs_source_release(source);
}

static void LoadScene(obs_data_t *data, QString path)
{
	if (!data)
		return;
	obs_data_array_t *sourcesData = obs_data_get_array(data, "sources");
	if (!sourcesData)
		return;
	LoadSources(sourcesData, path);
	obs_data_array_release(sourcesData);
}

void LoadStreamUpFile(void *private_data)
{
	UNUSED_PARAMETER(private_data);
	QString fileName = QFileDialog::getOpenFileName(
		nullptr, QT_UTF8(obs_module_text("Load")), QString(),
		"StreamUP File (*.streamup)");
	if (fileName.isEmpty())
		return;
	obs_data_t *data = obs_data_create_from_json_file(QT_TO_UTF8(fileName));
	LoadScene(data, QFileInfo(fileName).absolutePath());
	obs_data_release(data);
}

bool obs_module_load()
{
	blog(LOG_INFO, "[StreamUP] loaded version %s", PROJECT_VERSION);

	obs_frontend_add_tools_menu_item(obs_module_text("StreamUp"),
					 LoadStreamUpFile, nullptr);
	return true;
}

void obs_module_unload() {}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("StreamUP");
}
