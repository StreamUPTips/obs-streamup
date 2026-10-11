#include "scene-organiser-collection-data.hpp"
#include <streamup/debug-logger.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QString>

namespace StreamUP {
namespace SceneOrganiser {
namespace CollectionData {

namespace {

// The key under the collection file's "modules" object.
constexpr const char *kModuleKey = "streamup_scene_organiser";
constexpr int kFormatVersion = 1;

// Each canvas has its own dock and its own files. These names must match the
// ones SceneOrganiserDock and SceneTreeModel read and write.
struct Canvas {
    const char *blobKey;
    const char *treeFile;
    const char *configKey;
};

constexpr Canvas kCanvases[] = {
    {"normal", "scene_tree_normal.json", "scene_organiser_normal"},
    {"vertical", "scene_tree_vertical.json", "scene_organiser_vertical"},
};

// The per-collection files, saved as <configKey>_<collection>_<suffix>.
// quick_tabs.txt is the older tab format, still read when the JSON is missing.
constexpr const char *kFileSuffixes[] = {
    "lock_state.txt",
    "hidden_scenes.txt",
    "quick_tabs.json",
    "quick_tabs.txt",
    "expansion_state.txt",
};

QString ConfigDir()
{
    char *path = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!path)
        return QString();
    QString dir = QString::fromUtf8(path);
    bfree(path);
    return dir;
}

QString CurrentCollection()
{
    char *name = obs_frontend_get_current_scene_collection();
    if (!name)
        return QString();
    QString collection = QString::fromUtf8(name);
    bfree(name);
    return collection;
}

QString CollectionFilePath(const QString &dir, const Canvas &canvas, const QString &collection,
                           const char *suffix)
{
    return dir + "/" + canvas.configKey + "_" + collection + "_" + suffix;
}

// Copies this collection's saved layout for one canvas into a fresh object.
// Returns nullptr when there is nothing saved for it.
obs_data_t *GatherCanvas(const QString &dir, const Canvas &canvas, const QString &collection)
{
    obs_data_t *out = obs_data_create();
    bool any = false;

    const QByteArray collectionUtf8 = collection.toUtf8();
    const QString treePath = dir + "/" + canvas.treeFile;

    obs_data_t *treeRoot = obs_data_create_from_json_file(treePath.toUtf8().constData());
    if (treeRoot) {
        obs_data_array_t *tree = obs_data_get_array(treeRoot, collectionUtf8.constData());
        if (tree) {
            obs_data_set_array(out, "tree", tree);
            obs_data_array_release(tree);
            any = true;
        }
        obs_data_release(treeRoot);
    }

    obs_data_t *files = obs_data_create();
    for (const char *suffix : kFileSuffixes) {
        QFile file(CollectionFilePath(dir, canvas, collection, suffix));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        obs_data_set_string(files, suffix, file.readAll().constData());
        any = true;
    }
    obs_data_set_obj(out, "files", files);
    obs_data_release(files);

    if (!any) {
        obs_data_release(out);
        return nullptr;
    }
    return out;
}

// True when anything at all is saved locally for this canvas and collection.
// A canvas that has local data is left alone, so the copy in the collection
// file can never overwrite something newer.
bool HasLocalData(const QString &dir, const Canvas &canvas, const QString &collection)
{
    for (const char *suffix : kFileSuffixes) {
        if (QFile::exists(CollectionFilePath(dir, canvas, collection, suffix)))
            return true;
    }

    const QString treePath = dir + "/" + canvas.treeFile;
    obs_data_t *treeRoot = obs_data_create_from_json_file(treePath.toUtf8().constData());
    if (!treeRoot)
        return false;
    const bool hasTree = obs_data_has_user_value(treeRoot, collection.toUtf8().constData());
    obs_data_release(treeRoot);
    return hasTree;
}

void RestoreCanvas(const QString &dir, const Canvas &canvas, const QString &collection, obs_data_t *saved)
{
    int restoredFiles = 0;
    int treeItems = -1;

    obs_data_array_t *tree = obs_data_get_array(saved, "tree");
    if (tree) {
        const QString treePath = dir + "/" + canvas.treeFile;
        obs_data_t *treeRoot = obs_data_create_from_json_file(treePath.toUtf8().constData());
        if (!treeRoot)
            treeRoot = obs_data_create();

        obs_data_set_array(treeRoot, collection.toUtf8().constData(), tree);
        obs_data_save_json_safe(treeRoot, treePath.toUtf8().constData(), "tmp", "bak");
        treeItems = static_cast<int>(obs_data_array_count(tree));

        obs_data_release(treeRoot);
        obs_data_array_release(tree);
    }

    obs_data_t *files = obs_data_get_obj(saved, "files");
    if (files) {
        for (const char *suffix : kFileSuffixes) {
            if (!obs_data_has_user_value(files, suffix))
                continue;
            QSaveFile file(CollectionFilePath(dir, canvas, collection, suffix));
            if (!file.open(QIODevice::WriteOnly))
                continue;
            file.write(obs_data_get_string(files, suffix));
            if (file.commit())
                restoredFiles++;
        }
        obs_data_release(files);
    }

    blog(LOG_INFO,
         "[StreamUP] [SceneOrganiser] Restored the %s layout for collection '%s' from the collection file "
         "(%d top level items, %d settings files)",
         canvas.blobKey, collection.toUtf8().constData(), treeItems < 0 ? 0 : treeItems, restoredFiles);
}

void SaveToCollection(obs_data_t *moduleData)
{
    const QString collection = CurrentCollection();
    const QString dir = ConfigDir();
    if (collection.isEmpty() || dir.isEmpty())
        return;

    obs_data_t *blob = obs_data_create();
    bool any = false;

    for (const Canvas &canvas : kCanvases) {
        obs_data_t *canvasData = GatherCanvas(dir, canvas, collection);
        if (!canvasData)
            continue;
        obs_data_set_obj(blob, canvas.blobKey, canvasData);
        obs_data_release(canvasData);
        any = true;
    }

    // Nothing saved locally for this collection: leave whatever copy the
    // collection file already carries rather than wiping it.
    if (any) {
        obs_data_set_int(blob, "version", kFormatVersion);
        obs_data_set_obj(moduleData, kModuleKey, blob);
    }
    obs_data_release(blob);
}

void LoadFromCollection(obs_data_t *moduleData)
{
    obs_data_t *blob = obs_data_get_obj(moduleData, kModuleKey);
    if (!blob)
        return;

    // OBS points the current collection at the new one before loading it, so
    // this is the name the dock is about to read its files under.
    const QString collection = CurrentCollection();
    const QString dir = ConfigDir();

    if (!collection.isEmpty() && !dir.isEmpty() && QDir().mkpath(dir)) {
        for (const Canvas &canvas : kCanvases) {
            obs_data_t *saved = obs_data_get_obj(blob, canvas.blobKey);
            if (!saved)
                continue;

            if (HasLocalData(dir, canvas, collection)) {
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "CollectionData",
                    QString("Local %1 layout for '%2' found, the collection file copy is not needed")
                        .arg(canvas.blobKey, collection)
                        .toUtf8()
                        .constData());
            } else {
                RestoreCanvas(dir, canvas, collection, saved);
            }
            obs_data_release(saved);
        }
    }

    obs_data_release(blob);
}

void OnSaveLoad(obs_data_t *moduleData, bool saving, void *)
{
    if (!moduleData)
        return;

    if (saving)
        SaveToCollection(moduleData);
    else
        LoadFromCollection(moduleData);
}

} // namespace

void Register()
{
    obs_frontend_add_save_callback(OnSaveLoad, nullptr);
}

void Unregister()
{
    obs_frontend_remove_save_callback(OnSaveLoad, nullptr);
}

} // namespace CollectionData
} // namespace SceneOrganiser
} // namespace StreamUP
