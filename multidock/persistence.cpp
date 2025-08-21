#include "persistence.hpp"
#include <obs-module.h>
#include <obs.h>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace StreamUP {
namespace MultiDock {


static QString GetConfigPath()
{
    // Use OBS module config path for better compatibility
    char* obsConfigPath = obs_module_config_path("multidock_config.json");
    QString configPath = QString::fromUtf8(obsConfigPath);
    bfree(obsConfigPath);
    
    // Ensure the directory exists
    QFileInfo fileInfo(configPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    blog(LOG_INFO, "[StreamUP MultiDock] Config path: %s", configPath.toUtf8().constData());
    return configPath;
}

static QJsonObject LoadConfig()
{
    QString configPath = GetConfigPath();
    QFile file(configPath);
    
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    
    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        blog(LOG_WARNING, "[StreamUP MultiDock] Failed to parse config file: %s", 
             error.errorString().toUtf8().constData());
        return QJsonObject();
    }
    
    return doc.object();
}

static void SaveConfig(const QJsonObject& config)
{
    QString configPath = GetConfigPath();
    QFile file(configPath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        blog(LOG_ERROR, "[StreamUP MultiDock] Failed to write config file: %s", 
             configPath.toUtf8().constData());
        return;
    }
    
    QJsonDocument doc(config);
    QByteArray jsonData = doc.toJson();
    qint64 written = file.write(jsonData);
    file.flush();
    
    blog(LOG_INFO, "[StreamUP MultiDock] Wrote %lld bytes to config file", written);
}

QList<MultiDockInfo> LoadMultiDockList()
{
    QList<MultiDockInfo> result;
    QJsonObject config = LoadConfig();
    
    blog(LOG_INFO, "[StreamUP MultiDock] Config object keys: %s", 
         QJsonDocument(config).toJson(QJsonDocument::Compact).constData());
    
    QJsonArray multiDockArray = config["multidocks"].toArray();
    blog(LOG_INFO, "[StreamUP MultiDock] MultiDock array size: %d", multiDockArray.size());
    
    for (const QJsonValue& value : multiDockArray) {
        QJsonObject obj = value.toObject();
        MultiDockInfo info;
        info.id = obj["id"].toString();
        info.name = obj["name"].toString();
        
        blog(LOG_INFO, "[StreamUP MultiDock] Loading MultiDock: id='%s', name='%s'", 
             info.id.toUtf8().constData(), info.name.toUtf8().constData());
        
        if (!info.id.isEmpty() && !info.name.isEmpty()) {
            result.append(info);
        }
    }
    
    blog(LOG_INFO, "[StreamUP MultiDock] Loaded %d MultiDocks from config", result.size());
    return result;
}

void SaveMultiDockList(const QList<MultiDockInfo>& multiDocks)
{
    QJsonObject config = LoadConfig();
    QJsonArray multiDockArray;
    
    blog(LOG_INFO, "[StreamUP MultiDock] Saving %d MultiDocks to config", multiDocks.size());
    
    for (const MultiDockInfo& info : multiDocks) {
        QJsonObject obj;
        obj["id"] = info.id;
        obj["name"] = info.name;
        multiDockArray.append(obj);
        
        blog(LOG_INFO, "[StreamUP MultiDock] Saving MultiDock: id='%s', name='%s'", 
             info.id.toUtf8().constData(), info.name.toUtf8().constData());
    }
    
    config["multidocks"] = multiDockArray;
    
    blog(LOG_INFO, "[StreamUP MultiDock] Final config to save: %s", 
         QJsonDocument(config).toJson(QJsonDocument::Compact).constData());
    
    SaveConfig(config);
    
    blog(LOG_INFO, "[StreamUP MultiDock] Saved %d MultiDocks to config", multiDocks.size());
}

bool LoadMultiDockState(const QString& id, QStringList& capturedDocks, QByteArray& layout)
{
    QJsonObject config = LoadConfig();
    QJsonObject states = config["states"].toObject();
    QJsonObject state = states[id].toObject();
    
    if (state.isEmpty()) {
        return false;
    }
    
    // Load captured docks
    QJsonArray docksArray = state["captured"].toArray();
    capturedDocks.clear();
    for (const QJsonValue& value : docksArray) {
        capturedDocks.append(value.toString());
    }
    
    // Load layout
    QString layoutBase64 = state["layout"].toString();
    layout = QByteArray::fromBase64(layoutBase64.toUtf8());
    
    blog(LOG_INFO, "[StreamUP MultiDock] Loaded state for MultiDock '%s': %d captured docks", 
         id.toUtf8().constData(), capturedDocks.size());
    
    return true;
}

void SaveMultiDockState(const QString& id, const QStringList& capturedDocks, const QByteArray& layout)
{
    QJsonObject config = LoadConfig();
    QJsonObject states = config["states"].toObject();
    QJsonObject state;
    
    // Save captured docks
    QJsonArray docksArray;
    for (const QString& dockId : capturedDocks) {
        docksArray.append(dockId);
    }
    state["captured"] = docksArray;
    
    // Save layout
    QString layoutBase64 = layout.toBase64();
    state["layout"] = layoutBase64;
    
    states[id] = state;
    config["states"] = states;
    SaveConfig(config);
    
    blog(LOG_INFO, "[StreamUP MultiDock] Saved state for MultiDock '%s': %d captured docks", 
         id.toUtf8().constData(), capturedDocks.size());
}

void RemoveMultiDockState(const QString& id)
{
    QJsonObject config = LoadConfig();
    QJsonObject states = config["states"].toObject();
    states.remove(id);
    config["states"] = states;
    SaveConfig(config);
    
    blog(LOG_INFO, "[StreamUP MultiDock] Removed state for MultiDock '%s'", id.toUtf8().constData());
}


} // namespace MultiDock
} // namespace StreamUP