# 🌍 StreamUP Plugin Localization Guide

This guide explains how to contribute translations and maintain the localization system for the StreamUP plugin.

## 📁 File Structure

```
data/locale/
├── en-US.ini          # English (US) - Primary language
├── es-ES.ini          # Spanish (Spain) - Example translation
└── [other languages]  # Add new language files here
```

## 🚀 Quick Start for Translators

### Adding a New Language

1. **Copy the English template:**
   ```bash
   cp data/locale/en-US.ini data/locale/[LANGUAGE-CODE].ini
   ```
   
   Common language codes:
   - `fr-FR` - French (France)
   - `de-DE` - German (Germany)
   - `pt-BR` - Portuguese (Brazil)
   - `ja-JP` - Japanese
   - `ko-KR` - Korean
   - `zh-CN` - Chinese (Simplified)

2. **Edit the header:**
   ```ini
   # ===============================================================================
   # StreamUP Plugin Locale File - [Your Language Name]
   # ===============================================================================
   ```

3. **Translate the strings:**
   Only change the text within quotes, keep the keys the same:
   ```ini
   # English
   Cancel="Cancel"
   
   # Your translation
   Cancel="Cancelar"  # Spanish example
   ```

4. **Test your translation:**
   - Build the plugin
   - Change OBS language settings to your locale
   - Verify all text displays correctly

### Translation Guidelines

#### ✅ Do:
- Keep placeholder variables like `%1` in the same position
- Maintain consistent terminology throughout
- Test UI layouts with longer/shorter text
- Use formal tone unless informal is more natural in your language
- Keep technical terms like "StreamUP", "OBS", "MultiDock" untranslated

#### ❌ Don't:
- Change the key names (left side of =)
- Remove or modify escape sequences like `\n`
- Translate file extensions or technical identifiers
- Make translations significantly longer than the UI can handle

## 🔧 Developer Guide

### Adding New Translatable Strings

1. **Add to locale file:**
   ```ini
   # Add to appropriate section in en-US.ini
   MyFeature.ButtonName="My Button"
   MyFeature.Tooltip="This is a tooltip"
   ```

2. **Use in code:**
   ```cpp
   // C++
   QString buttonText = obs_module_text("MyFeature.ButtonName");
   button->setText(buttonText);
   button->setToolTip(obs_module_text("MyFeature.Tooltip"));
   ```

3. **Update all language files:**
   ```bash
   # Add the new keys to all existing locale files
   # Translators can update the values later
   ```

### Key Naming Conventions

The new locale system uses hierarchical naming with dots following these patterns:

```ini
# Core UI Elements
UI.Button.Cancel="Cancel"
UI.Label.Name="Name:"
UI.Message.Loading="Loading..."

# Application Info
App.Name="StreamUP"
App.Description="Description"

# Menu System
Menu.About="About"
Menu.VideoCapture.ActivateAll="Activate All Video Capture Devices"
Menu.Source.LockAllSources="Lock/Unlock All Sources"

# Features
Feature.SourceLock.All.Title="Lock / Unlock All Sources"
Feature.BrowserSources.Notification="Browser sources successfully refreshed"
Feature.VideoCapture.Activate.HowTo1="Press the activate button in the dock."

# System-Specific
WebSocket.Window.Title="StreamUP WebSocket Commands"
Hotkey.LockAllSources.Name="Lock/Unlock All Sources"
Settings.Window.Title="StreamUP • Settings"
Plugin.Status.AllUpToDate="All plugins are up to date"
Dock.Tool.LockAllSources.Title="Lock/Unlock All Sources"
MultiDock.Dialog.NewTitle="New MultiDock"
Toolbar.Config.Title="StreamUP Toolbar Configuration"
Theme.Window.Title="StreamUP Themes"
```

### Current Sections

The locale file is organized into these hierarchical sections:

- **CORE UI ELEMENTS** - Common buttons, labels, messages (`UI.*`, `App.*`)
- **MENU SYSTEM** - All menu items (`Menu.*`)
- **FEATURES** - Main plugin features (`Feature.*`)
  - Source Locking (`Feature.SourceLock.*`)
  - Browser Sources (`Feature.BrowserSources.*`)
  - Audio Monitoring (`Feature.AudioMonitoring.*`)
  - Video Capture (`Feature.VideoCapture.*`)
- **WEBSOCKET SYSTEM** - WebSocket commands and UI (`WebSocket.*`)
- **HOTKEY SYSTEM** - Hotkey names and descriptions (`Hotkey.*`)
- **PLUGIN MANAGEMENT** - Plugin status and dialogs (`Plugin.*`)
- **SETTINGS WINDOW** - Settings UI and configuration (`Settings.*`)
- **DOCK SYSTEM** - Dock tools and configuration (`Dock.*`)
- **MULTIDOCK SYSTEM** - MultiDock-specific strings (`MultiDock.*`)
- **TOOLBAR SYSTEM** - Toolbar-related strings (`Toolbar.*`)
- **THEME SYSTEM** - Theme-related strings (`Theme.*`)

## 🔍 Testing Translations

### Build and Test
```bash
# Build the plugin
mkdir build && cd build
cmake ..
make

# Install to OBS plugins directory
# Change OBS language in settings
# Test all UI components
```

### Validation Checklist
- [ ] All strings display correctly
- [ ] No text is cut off in UI
- [ ] Buttons and dialogs resize properly
- [ ] Special characters display correctly
- [ ] Plurals work correctly
- [ ] Error messages are clear

## 📋 Translation Status

| Language | Status | Contributor |
|----------|--------|-------------|
| English (en-US) | ✅ Complete | Official |
| Spanish (es-ES) | 🔄 Example | Official |

## 🤝 Contributing

1. **Fork the repository**
2. **Create your translation file** following the guide above
3. **Test thoroughly** with the actual plugin
4. **Submit a pull request** with:
   - Translation file
   - Your name/credits
   - Brief description of testing performed

### Translation Maintenance

When the plugin adds new features:
1. New strings are added to `en-US.ini`
2. Translators are notified via GitHub issues
3. Translation files are updated with the new keys
4. Translators can provide translations via pull requests

## 📞 Support

- **Issues:** Report translation bugs or request new languages
- **Questions:** Ask about localization in GitHub discussions
- **Coordination:** Use GitHub issues to coordinate translation efforts

## 🙏 Credits

Thank you to all translators who help make StreamUP accessible worldwide!

---
*This localization system uses OBS Studio's built-in text loading mechanism for seamless integration.*