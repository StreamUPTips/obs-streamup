#include "product-requirements.hpp"
#include "plugin-state.hpp"
#include "path-utils.hpp"
#include "version-utils.hpp"

#include <streamup/debug-logger.hpp>

#include <obs-module.h>
#include <obs.h>
#include <util/bmem.h>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cstring>

namespace StreamUP {
namespace ProductRequirements {

namespace pm = StreamUP::PluginManager;

namespace {

constexpr const char *kPluginstallerUrl = "https://streamup.tips/product/pluginstaller";

// OBS's own types. They are never "missing a plugin": when one is not
// registered it is a platform thing (GDI+ text on macOS, WASAPI on Linux) that
// OBS itself reports, and refusing to install over it would block every Mac.
const char *const kBuiltIn[] = {
	"scene", "group", "color_source", "image_source", "slideshow", "ffmpeg_source", "browser_source",
	"text_gdiplus", "text_ft2_source", "vlc_source", "dshow_input", "wasapi_input_capture",
	"wasapi_output_capture", "wasapi_process_output_capture", "game_capture", "window_capture",
	"monitor_capture", "display_capture", "screen_capture", "av_capture_input", "macos-avcapture",
	"coreaudio_input_capture", "coreaudio_output_capture", "sck_audio_capture", "xshm_input",
	"xcomposite_input", "pipewire-desktop-capture-source", "pipewire-window-capture-source",
	"pipewire-screen-capture-source", "pulse_input_capture", "pulse_output_capture", "alsa_input_capture",
	"jack_output_capture", "v4l2_input", "decklink-input", "aja_source", "audio_line",
	// transitions
	"cut_transition", "fade_transition", "swipe_transition", "slide_transition", "wipe_transition",
	"fade_to_color_transition", "obs_stinger_transition", "luma_wipe_transition",
	// filters (obs-filters, nv-filters, obs-vst)
	"async_delay_filter", "basic_eq_filter", "chroma_key_filter", "clut_filter", "color_filter",
	"color_grade_filter", "color_key_filter", "compressor_filter", "crop_filter", "expander_filter",
	"gain_filter", "gpu_delay", "hdr_tonemap_filter", "invert_polarity_filter", "limiter_filter",
	"luma_key_filter", "mask_filter", "noise_gate_filter", "noise_suppress_filter", "scale_filter",
	"scroll_filter", "sdr_on_hdr_filter", "sharpness_filter", "upward_compressor_filter", "vst_filter",
	"nv_background_blur_filter", "nv_blur_filter", "nv_greenscreen_filter", "nvidia_audiofx_filter",
};

// Type ids to the plugin that registers them. "name" is the plugin's name in
// the StreamUP plugin list, which supplies its version and links; "website" is
// only used when the list does not know it. An id ending in '*' is a prefix.
// Checked against each plugin's registered types (see
// docs/plugin-requirements-scan.md for how this list was built).
struct TypeMapping {
	const char *id;
	const char *name;
	const char *website;
};

const TypeMapping kTypeMap[] = {
	{"move_*", "Move", "https://obsproject.com/forum/resources/move.913/"},
	{"audio_move_filter", "Move", "https://obsproject.com/forum/resources/move.913/"},
	{"nv_move_filter", "Move", "https://obsproject.com/forum/resources/move.913/"},
	{"scene_as_transition", "StreamUP Scene as Transition",
	 "https://obsproject.com/forum/resources/scene-as-transition.1704/"},
	{"shader_filter", "OBS-Shaderfilter", "https://obsproject.com/forum/resources/obs-shaderfilter.1736/"},
	{"shader_source", "OBS-Shaderfilter", "https://obsproject.com/forum/resources/obs-shaderfilter.1736/"},
	{"shader_transition", "OBS-Shaderfilter", "https://obsproject.com/forum/resources/obs-shaderfilter.1736/"},
	{"advanced_masks_filter*", "Advanced Masks", "https://obsproject.com/forum/resources/advanced-masks.1856/"},
	{"obs_composite_blur*", "Composite Blur", "https://obsproject.com/forum/resources/composite-blur.1780/"},
	{"obs_stroke_*", "Stroke Glow Shadow", "https://obsproject.com/forum/resources/stroke-glow-shadow.1800/"},
	{"obs_glow_*", "Stroke Glow Shadow", "https://obsproject.com/forum/resources/stroke-glow-shadow.1800/"},
	{"obs_shadow_*", "Stroke Glow Shadow", "https://obsproject.com/forum/resources/stroke-glow-shadow.1800/"},
	{"obs_retro_effects*", "Retro Effects", "https://obsproject.com/forum/resources/retro-effects.1972/"},
	{"pixel_art_plugin", "Pixel Art", "https://obsproject.com/forum/resources/pixel-art.1789/"},
	{"phandasm_waveform_source", "Waveform", "https://obsproject.com/forum/resources/waveform.1423/"},
	{"3d_effect_filter", "3D Effect", "https://obsproject.com/forum/resources/3d-effect.1692/"},
	{"dynamic_delay_filter", "Dynamic Delay", "https://obsproject.com/forum/resources/dynamic-delay.1035/"},
	{"freeze_filter", "Freeze Filter", "https://obsproject.com/forum/resources/freeze-filter.950/"},
	{"gradient_source", "Gradient Source", "https://obsproject.com/forum/resources/gradient-source.1172/"},
	// Downstream Keyer's output source really is spelt "ouput".
	{"ouput_source", "Downstream Keyer", "https://obsproject.com/forum/resources/downstream-keyer.1254/"},
	{"source-clone", "Source Clone", "https://obsproject.com/forum/resources/source-clone.1632/"},
	{"obs_noise_*", "Noise", "https://obsproject.com/forum/resources/noise.1916/"},
	{"time_warp_scan_filter", "Time Warp Scan", "https://obsproject.com/forum/resources/time-warp-scan.1167/"},
	{"source_record_filter", "Source Record", "https://obsproject.com/forum/resources/source-record.1285/"},
	{"draw_source", "Draw", "https://obsproject.com/forum/resources/draw.2081/"},
	{"input-overlay", "Input Overlay", "https://obsproject.com/forum/resources/input-overlay.552/"},
	{"input-history", "Input Overlay", "https://obsproject.com/forum/resources/input-overlay.552/"},
	{"obs_ghostscript", "PDF Source (Ghostscript)", "https://github.com/nleseul/obs-ghostscript"},
	{"streamfx-*", "StreamFX", "https://github.com/Xaymar/obs-StreamFX"},
	{"obs-stream-effects-*", "StreamFX", "https://github.com/Xaymar/obs-StreamFX"},
	{"streamup_adjustment_layer", "StreamUP", "https://streamup.tips/plugin"},
	{"streamup_protext_*", "StreamUP ProText", "https://streamup.tips/"},
	{"streamup_bbcode_*", "StreamUP ProText", "https://streamup.tips/"},
	{"streamup_chat", "StreamUP Chat", "https://streamup.tips/"},
	{"streamup_test_card", "StreamUP Test Card", "https://streamup.tips/"},
};

QString Text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

bool IsBuiltIn(const std::string &id)
{
	for (const char *b : kBuiltIn)
		if (id == b)
			return true;
	return false;
}

const TypeMapping *MappingFor(const std::string &id)
{
	for (const TypeMapping &m : kTypeMap) {
		const size_t n = std::strlen(m.id);
		if (n && m.id[n - 1] == '*') {
			if (id.compare(0, n - 1, m.id, n - 1) == 0)
				return &m;
		} else if (id == m.id) {
			return &m;
		}
	}
	return nullptr;
}

bool Registered(const std::string &id)
{
	return !id.empty() && obs_source_get_display_name(id.c_str()) != nullptr;
}

// Every source, filter and transition record anywhere in the file: sources,
// their filters, groups, scene item show/hide transitions, transitions.
void CollectTypes(const QJsonValue &node, std::set<std::string> &used, std::set<std::string> &unregistered)
{
	if (node.isArray()) {
		for (const QJsonValue v : node.toArray())
			CollectTypes(v, used, unregistered);
		return;
	}
	if (!node.isObject())
		return;
	const QJsonObject o = node.toObject();
	const QJsonValue id = o.value("id");
	const bool record = id.isString() && (o.contains("versioned_id") ||
					      (o.contains("settings") && o.contains("name")));
	if (record) {
		const std::string unversioned = id.toString().toStdString();
		const std::string versioned = o.value("versioned_id").toString().toStdString();
		if (!unversioned.empty()) {
			used.insert(unversioned);
			if (!Registered(unversioned) && !Registered(versioned))
				unregistered.insert(unversioned);
		}
	}
	for (auto it = o.begin(); it != o.end(); ++it)
		if (it.value().isArray() || it.value().isObject())
			CollectTypes(it.value(), used, unregistered);
}

std::string InstalledVersion(const char *logPath, const std::string &searchString)
{
	if (!logPath || searchString.empty() || pm::IsUpdateCheckSkipped(searchString))
		return std::string();
	return pm::SearchStringInFileForVersion(logPath, searchString.c_str());
}

void AddMissing(Result &r, pm::PluginRow row)
{
	for (const pm::PluginRow &m : r.missing)
		if (m.name == row.name)
			return;
	r.missing.push_back(std::move(row));
}

void AddOutdated(Result &r, pm::PluginRow row)
{
	for (const pm::PluginRow &m : r.outdated)
		if (m.name == row.name)
			return;
	r.outdated.push_back(std::move(row));
}

} // namespace

Result Check(const QString &path)
{
	Result r;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		r.readable = false;
		return r;
	}
	const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
	if (root.isEmpty()) {
		r.readable = false;
		return r;
	}
	const QJsonObject manifest = root.value("streamup_overlay").toObject();
	r.productName = manifest.value("product").toObject().value("name").toString(manifest.value("name").toString());
	if (r.productName.isEmpty())
		r.productName = QFileInfo(path).completeBaseName();

	std::set<std::string> unregistered;
	CollectTypes(root.value("sources"), r.usedTypes, unregistered);
	CollectTypes(root.value("transitions"), r.usedTypes, unregistered);
	CollectTypes(root.value("groups"), r.usedTypes, unregistered);
	// A transition product makes its Scene as Transition itself, so the type
	// is never in the file. The action needs it all the same.
	for (const QJsonValue a : manifest.value("actions").toArray()) {
		if (a.toObject().value("type").toString() == "use_as_transition") {
			r.usedTypes.insert("scene_as_transition");
			if (!Registered("scene_as_transition"))
				unregistered.insert("scene_as_transition");
		}
	}

	const auto allPlugins = StreamUP::GetAllPlugins();
	char *logPath = StreamUP::PathUtils::GetOBSLogPath();
	const std::map<std::string, bool> disabledModules =
		logPath ? pm::SearchDisabledModulesInLogFile(logPath) : std::map<std::string, bool>();

	// A missing plugin that is installed but switched off needs a toggle,
	// not a download.
	auto disabledFor = [&](const std::string &name, bool *safeMode) {
		auto it = allPlugins.find(name);
		if (it == allPlugins.end() || it->second.moduleName.empty())
			return false;
		auto d = disabledModules.find(it->second.moduleName);
		if (d == disabledModules.end())
			return false;
		*safeMode = d->second;
		return true;
	};

	for (const std::string &id : unregistered) {
		if (IsBuiltIn(id)) {
			DebugLogger::LogInfoFormat("Requirements", "'%s' is an OBS type this OBS does not have, not blocking",
						   id.c_str());
			continue;
		}
		const TypeMapping *m = MappingFor(id);
		if (!m) {
			pm::PluginRow row;
			row.name = Text("Requirements.UnknownType").arg(QString::fromStdString(id)).toStdString();
			row.websiteUrl = kPluginstallerUrl;
			row.downloadUrl = kPluginstallerUrl;
			AddMissing(r, row);
			r.anyUnknown = true;
			continue;
		}
		bool safeMode = false;
		if (disabledFor(m->name, &safeMode)) {
			r.disabled[m->name] = safeMode;
			continue;
		}
		pm::PluginRow row = pm::RowForListedPlugin(m->name);
		if (row.websiteUrl.empty())
			row.websiteUrl = m->website;
		AddMissing(r, row);
	}

	// Version protection, for plugins this file uses and that are loaded.
	std::set<std::string> usedPlugins;
	for (const std::string &id : r.usedTypes)
		if (const TypeMapping *m = MappingFor(id); m && !unregistered.count(id))
			usedPlugins.insert(m->name);
	for (const std::string &name : usedPlugins) {
		auto it = allPlugins.find(name);
		if (it == allPlugins.end())
			continue;
		const std::string installed = InstalledVersion(logPath, it->second.searchString);
		if (!installed.empty() && installed != it->second.version &&
		    VersionUtils::IsVersionLessThan(installed, it->second.version))
			AddOutdated(r, pm::RowForListedPlugin(name, installed));
	}

	// Plugins the product names itself: docks and tools that add no types.
	for (const QJsonValue v : manifest.value("requires").toArray()) {
		const QJsonObject q = v.toObject();
		const std::string name = q.value("name").toString().toStdString();
		if (name.empty())
			continue;
		auto listed = allPlugins.find(name);
		std::string search = q.value("search_string").toString().toStdString();
		std::string minVersion = q.value("min_version").toString().toStdString();
		if (listed != allPlugins.end()) {
			if (search.empty())
				search = listed->second.searchString;
			if (minVersion.empty())
				minVersion = listed->second.version;
		}
		if (search.empty()) {
			DebugLogger::LogWarningFormat("Requirements",
						      "'%s' is required but has no search string, so it cannot be checked",
						      name.c_str());
			continue;
		}
		pm::PluginRow row = pm::RowForListedPlugin(name);
		const std::string url = q.value("url").toString().toStdString();
		if (!url.empty()) {
			row.websiteUrl = url;
			if (listed == allPlugins.end())
				row.downloadUrl = url;
		}
		row.requiredVersion = minVersion;
		const std::string installed = InstalledVersion(logPath, search);
		if (installed.empty()) {
			bool safeMode = false;
			if (disabledFor(name, &safeMode))
				r.disabled[name] = safeMode;
			else
				AddMissing(r, row);
		} else if (!minVersion.empty() && installed != minVersion &&
			   VersionUtils::IsVersionLessThan(installed, minVersion)) {
			row.installedVersion = installed;
			AddOutdated(r, row);
		}
	}
	if (logPath)
		bfree(logPath);

	std::string types;
	for (const std::string &id : r.usedTypes)
		types += (types.empty() ? "" : ", ") + id;
	DebugLogger::LogInfoFormat("Requirements", "%s uses: %s. Missing %d, outdated %d, switched off %d",
				   r.productName.toUtf8().constData(), types.c_str(), int(r.missing.size()),
				   int(r.outdated.size()), int(r.disabled.size()));
	return r;
}

void ShowIssues(const Result &result, std::function<void()> continueCallback)
{
	pm::PluginIssues issues;
	issues.missing = result.missing;
	issues.outdated = result.outdated;
	issues.disabled = result.disabled;
	const QString product = result.productName.toHtmlEscaped();
	if (!result.missing.empty())
		issues.description = Text("Requirements.Missing").arg(product);
	else if (!result.outdated.empty())
		issues.description = Text("Requirements.Outdated").arg(product);
	issues.warning = Text("Requirements.WarningContinue");
	if (result.anyUnknown)
		issues.warning += " " + Text("Requirements.WarningUnknown");
	pm::ShowPluginIssues(issues, continueCallback, false);
}

void CheckThen(const QString &path, std::function<void()> onReady)
{
	const Result r = Check(path);
	if (!r.readable || r.ok()) {
		// An unreadable file is the loader's to report.
		if (onReady)
			onReady();
		return;
	}
	ShowIssues(r, onReady);
}

} // namespace ProductRequirements
} // namespace StreamUP
