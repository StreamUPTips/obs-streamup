#include "hotkey-widget.hpp"
#include "ui-styles.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeySequence>
#include <QApplication>
#include <obs-data.h>
#include <obs-module.h>

namespace StreamUP {
namespace UI {

HotkeyWidget::HotkeyWidget(const QString& hotkeyName, QWidget* parent)
    : QWidget(parent)
    , m_hotkeyName(hotkeyName)
    , m_recording(false)
    , m_recordedKey(0)
    , m_recordedModifiers(Qt::NoModifier)
    , m_currentHotkeyData(nullptr)
{
    setFocusPolicy(Qt::StrongFocus);
    
    // Create layout
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    
    // Display label showing current hotkey
    m_displayLabel = new QLabel(obs_module_text("HotkeyWidget.None"));
    m_displayLabel->setStyleSheet(QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "padding: %3px %4px;"
        "background: %5;"
        "border: 1px solid %6;"
        "border-radius: %7px;"
        "min-width: 100px;"
        "}")
        .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
        .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL)
        .arg(StreamUP::UIStyles::Sizes::PADDING_SMALL)
        .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM)
        .arg(StreamUP::UIStyles::Colors::BACKGROUND_INPUT)
        .arg(StreamUP::UIStyles::Colors::BORDER_LIGHT)
        .arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS));
    m_displayLabel->setAlignment(Qt::AlignCenter);
    
    // Record button
    m_recordButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("HotkeyWidget.Set"), "info");
    m_recordButton->setFixedWidth(70);
    connect(m_recordButton, &QPushButton::clicked, this, &HotkeyWidget::OnRecordButtonClicked);
    
    // Clear button  
    m_clearButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("HotkeyWidget.Clear"), "neutral");
    m_clearButton->setFixedWidth(70);
    connect(m_clearButton, &QPushButton::clicked, this, &HotkeyWidget::OnClearButtonClicked);
    
    layout->addWidget(m_displayLabel, 1);
    layout->addWidget(m_recordButton);
    layout->addWidget(m_clearButton);
    
    // Install event filter to capture key events when recording
    installEventFilter(this);
    
    UpdateDisplay();
}

HotkeyWidget::~HotkeyWidget()
{
    if (m_currentHotkeyData) {
        obs_data_array_release(m_currentHotkeyData);
    }
}

void HotkeyWidget::SetHotkey(obs_data_array_t* hotkeyData)
{
    if (m_currentHotkeyData) {
        obs_data_array_release(m_currentHotkeyData);
        m_currentHotkeyData = nullptr;
    }
    
    if (hotkeyData && obs_data_array_count(hotkeyData) > 0) {
        m_currentHotkeyData = obs_data_array_create();
        // Copy each item from the source array
        size_t count = obs_data_array_count(hotkeyData);
        for (size_t i = 0; i < count; i++) {
            obs_data_t* item = obs_data_array_item(hotkeyData, i);
            obs_data_array_push_back(m_currentHotkeyData, item);
            obs_data_release(item);
        }
    }
    
    UpdateDisplay();
}

obs_data_array_t* HotkeyWidget::GetHotkey() const
{
    if (!m_currentHotkeyData) {
        return obs_data_array_create(); // Return empty array
    }
    
    obs_data_array_t* copy = obs_data_array_create();
    // Copy each item from the current array
    size_t count = obs_data_array_count(m_currentHotkeyData);
    for (size_t i = 0; i < count; i++) {
        obs_data_t* item = obs_data_array_item(m_currentHotkeyData, i);
        obs_data_array_push_back(copy, item);
        obs_data_release(item);
    }
    return copy;
}

void HotkeyWidget::ClearHotkey()
{
    if (m_currentHotkeyData) {
        obs_data_array_release(m_currentHotkeyData);
        m_currentHotkeyData = nullptr;
    }
    
    m_recordedKey = 0;
    m_recordedModifiers = Qt::NoModifier;
    
    UpdateDisplay();
    emit HotkeyChanged(m_hotkeyName, nullptr);
}

void HotkeyWidget::OnRecordButtonClicked()
{
    if (m_recording) {
        StopRecording();
    } else {
        StartRecording();
    }
}

void HotkeyWidget::OnClearButtonClicked()
{
    ClearHotkey();
}

void HotkeyWidget::StartRecording()
{
    m_recording = true;
    m_recordedKey = 0;
    m_recordedModifiers = Qt::NoModifier;
    
    m_recordButton->setText(obs_module_text("HotkeyWidget.Cancel"));
    m_recordButton->setStyleSheet(StreamUP::UIStyles::CreateStyledButton("", "error")->styleSheet());
    m_displayLabel->setText(obs_module_text("HotkeyWidget.PressKeys"));
    m_displayLabel->setStyleSheet(m_displayLabel->styleSheet() + 
        QString("background: %1; border-color: %2;")
        .arg(StreamUP::UIStyles::Colors::WARNING)
        .arg(StreamUP::UIStyles::Colors::WARNING));
    
    setFocus();
    grabKeyboard();
}

void HotkeyWidget::StopRecording()
{
    if (!m_recording) return;
    
    m_recording = false;
    releaseKeyboard();
    
    m_recordButton->setText(obs_module_text("HotkeyWidget.Set"));
    m_recordButton->setStyleSheet(StreamUP::UIStyles::CreateStyledButton("", "info")->styleSheet());
    
    // Create hotkey data from recorded key combination
    if (m_recordedKey != 0) {
        if (m_currentHotkeyData) {
            obs_data_array_release(m_currentHotkeyData);
        }
        m_currentHotkeyData = CreateHotkeyData(m_recordedKey, m_recordedModifiers);
        emit HotkeyChanged(m_hotkeyName, m_currentHotkeyData);
    }
    
    UpdateDisplay();
}

void HotkeyWidget::UpdateDisplay()
{
    QString displayText = obs_module_text("HotkeyWidget.None");
    QString normalStyle = QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "padding: %3px %4px;"
        "background: %5;"
        "border: 1px solid %6;"
        "border-radius: %7px;"
        "min-width: 100px;"
        "}")
        .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
        .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL)
        .arg(StreamUP::UIStyles::Sizes::PADDING_SMALL)
        .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM)
        .arg(StreamUP::UIStyles::Colors::BACKGROUND_INPUT)
        .arg(StreamUP::UIStyles::Colors::BORDER_LIGHT)
        .arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS);
    
    if (m_currentHotkeyData && obs_data_array_count(m_currentHotkeyData) > 0) {
        // Parse OBS hotkey data format: {"key": "OBS_KEY_F1", "shift": true, "control": false, ...}
        obs_data_t* hotkeyBinding = obs_data_array_item(m_currentHotkeyData, 0);
        if (hotkeyBinding) {
            const char* keyStr = obs_data_get_string(hotkeyBinding, "key");
            bool shift = obs_data_get_bool(hotkeyBinding, "shift");
            bool control = obs_data_get_bool(hotkeyBinding, "control");
            bool alt = obs_data_get_bool(hotkeyBinding, "alt");
            bool command = obs_data_get_bool(hotkeyBinding, "command");
            
            QString modStr = "";
            if (control) {
                if (!modStr.isEmpty()) modStr += "+";
                modStr += obs_module_text("HotkeyWidget.Ctrl");
            }
            if (alt) {
                if (!modStr.isEmpty()) modStr += "+";
                modStr += obs_module_text("HotkeyWidget.Alt");
            }
            if (shift) {
                if (!modStr.isEmpty()) modStr += "+";
                modStr += obs_module_text("HotkeyWidget.Shift");
            }
            if (command) {
                if (!modStr.isEmpty()) modStr += "+";
                modStr += obs_module_text("HotkeyWidget.Cmd");
            }
            
            if (keyStr && strlen(keyStr) > 0) {
                // Convert OBS key name to display name
                QString keyName = QString(keyStr);
                if (keyName.startsWith("OBS_KEY_")) {
                    keyName = keyName.mid(8); // Remove "OBS_KEY_" prefix
                }
                
                displayText = modStr.isEmpty() ? keyName : modStr + "+" + keyName;
            }
            
            obs_data_release(hotkeyBinding);
        }
    }
    
    m_displayLabel->setText(displayText);
    m_displayLabel->setStyleSheet(normalStyle);
}

QString HotkeyWidget::FormatKeyCombo(int key, Qt::KeyboardModifiers modifiers)
{
    QString result;
    
    // Add modifiers
    if (modifiers & Qt::ControlModifier) {
        if (!result.isEmpty()) result += "+";
        result += obs_module_text("HotkeyWidget.Ctrl");
    }
    if (modifiers & Qt::AltModifier) {
        if (!result.isEmpty()) result += "+";
        result += obs_module_text("HotkeyWidget.Alt");
    }
    if (modifiers & Qt::ShiftModifier) {
        if (!result.isEmpty()) result += "+";
        result += obs_module_text("HotkeyWidget.Shift");
    }
    if (modifiers & Qt::MetaModifier) {
        if (!result.isEmpty()) result += "+";
        result += obs_module_text("HotkeyWidget.Meta");
    }
    
    // Add the key
    if (key != 0) {
        QString keyName = QKeySequence(key).toString();
        if (!keyName.isEmpty()) {
            if (!result.isEmpty()) result += "+";
            result += keyName;
        }
    }
    
    return result.isEmpty() ? obs_module_text("HotkeyWidget.None") : result;
}

QString HotkeyWidget::QtKeyToOBSKey(int qtKey) 
{
    // Convert Qt key codes to OBS key names
    switch (qtKey) {
        // Function keys
        case Qt::Key_F1: return "OBS_KEY_F1";
        case Qt::Key_F2: return "OBS_KEY_F2";
        case Qt::Key_F3: return "OBS_KEY_F3";
        case Qt::Key_F4: return "OBS_KEY_F4";
        case Qt::Key_F5: return "OBS_KEY_F5";
        case Qt::Key_F6: return "OBS_KEY_F6";
        case Qt::Key_F7: return "OBS_KEY_F7";
        case Qt::Key_F8: return "OBS_KEY_F8";
        case Qt::Key_F9: return "OBS_KEY_F9";
        case Qt::Key_F10: return "OBS_KEY_F10";
        case Qt::Key_F11: return "OBS_KEY_F11";
        case Qt::Key_F12: return "OBS_KEY_F12";
        
        // Numbers
        case Qt::Key_0: return "OBS_KEY_0";
        case Qt::Key_1: return "OBS_KEY_1";
        case Qt::Key_2: return "OBS_KEY_2";
        case Qt::Key_3: return "OBS_KEY_3";
        case Qt::Key_4: return "OBS_KEY_4";
        case Qt::Key_5: return "OBS_KEY_5";
        case Qt::Key_6: return "OBS_KEY_6";
        case Qt::Key_7: return "OBS_KEY_7";
        case Qt::Key_8: return "OBS_KEY_8";
        case Qt::Key_9: return "OBS_KEY_9";
        
        // Letters
        case Qt::Key_A: return "OBS_KEY_A";
        case Qt::Key_B: return "OBS_KEY_B";
        case Qt::Key_C: return "OBS_KEY_C";
        case Qt::Key_D: return "OBS_KEY_D";
        case Qt::Key_E: return "OBS_KEY_E";
        case Qt::Key_F: return "OBS_KEY_F";
        case Qt::Key_G: return "OBS_KEY_G";
        case Qt::Key_H: return "OBS_KEY_H";
        case Qt::Key_I: return "OBS_KEY_I";
        case Qt::Key_J: return "OBS_KEY_J";
        case Qt::Key_K: return "OBS_KEY_K";
        case Qt::Key_L: return "OBS_KEY_L";
        case Qt::Key_M: return "OBS_KEY_M";
        case Qt::Key_N: return "OBS_KEY_N";
        case Qt::Key_O: return "OBS_KEY_O";
        case Qt::Key_P: return "OBS_KEY_P";
        case Qt::Key_Q: return "OBS_KEY_Q";
        case Qt::Key_R: return "OBS_KEY_R";
        case Qt::Key_S: return "OBS_KEY_S";
        case Qt::Key_T: return "OBS_KEY_T";
        case Qt::Key_U: return "OBS_KEY_U";
        case Qt::Key_V: return "OBS_KEY_V";
        case Qt::Key_W: return "OBS_KEY_W";
        case Qt::Key_X: return "OBS_KEY_X";
        case Qt::Key_Y: return "OBS_KEY_Y";
        case Qt::Key_Z: return "OBS_KEY_Z";
        
        // Special keys
        case Qt::Key_Space: return "OBS_KEY_SPACE";
        case Qt::Key_Return: case Qt::Key_Enter: return "OBS_KEY_RETURN";
        case Qt::Key_Escape: return "OBS_KEY_ESCAPE";
        case Qt::Key_Tab: return "OBS_KEY_TAB";
        case Qt::Key_Backspace: return "OBS_KEY_BACKSPACE";
        case Qt::Key_Delete: return "OBS_KEY_DELETE";
        case Qt::Key_Insert: return "OBS_KEY_INSERT";
        case Qt::Key_Home: return "OBS_KEY_HOME";
        case Qt::Key_End: return "OBS_KEY_END";
        case Qt::Key_PageUp: return "OBS_KEY_PAGEUP";
        case Qt::Key_PageDown: return "OBS_KEY_PAGEDOWN";
        
        // Arrow keys
        case Qt::Key_Left: return "OBS_KEY_LEFT";
        case Qt::Key_Right: return "OBS_KEY_RIGHT";
        case Qt::Key_Up: return "OBS_KEY_UP";
        case Qt::Key_Down: return "OBS_KEY_DOWN";
        
        // Numpad
        case Qt::Key_NumLock: return "OBS_KEY_NUMLOCK";
        case Qt::Key_division: return "OBS_KEY_NUMSLASH";
        case Qt::Key_multiply: return "OBS_KEY_NUMMULTIPLY";
        case Qt::Key_Minus: return "OBS_KEY_NUMMINUS";
        case Qt::Key_Plus: return "OBS_KEY_NUMPLUS";
        
        default:
            // For unmapped keys, try to use Qt's string representation
            QString keyStr = QKeySequence(qtKey).toString().toUpper();
            if (!keyStr.isEmpty()) {
                return "OBS_KEY_" + keyStr;
            }
            return QString::number(qtKey); // Fallback to key code
    }
}

obs_data_array_t* HotkeyWidget::CreateHotkeyData(int key, Qt::KeyboardModifiers modifiers)
{
    obs_data_array_t* hotkeyArray = obs_data_array_create();
    obs_data_t* hotkeyBinding = obs_data_create();
    
    // Convert Qt key to OBS key name
    QString obsKeyName = QtKeyToOBSKey(key);
    obs_data_set_string(hotkeyBinding, "key", obsKeyName.toUtf8().constData());
    
    // Set modifiers as booleans (OBS format)
    obs_data_set_bool(hotkeyBinding, "shift", (modifiers & Qt::ShiftModifier) != 0);
    obs_data_set_bool(hotkeyBinding, "control", (modifiers & Qt::ControlModifier) != 0);
    obs_data_set_bool(hotkeyBinding, "alt", (modifiers & Qt::AltModifier) != 0);
    obs_data_set_bool(hotkeyBinding, "command", (modifiers & Qt::MetaModifier) != 0);
    
    obs_data_array_push_back(hotkeyArray, hotkeyBinding);
    obs_data_release(hotkeyBinding);
    
    return hotkeyArray;
}

void HotkeyWidget::keyPressEvent(QKeyEvent* event)
{
    if (!m_recording) {
        QWidget::keyPressEvent(event);
        return;
    }
    
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();
    
    // Ignore modifier-only keys
    if (key == Qt::Key_Control || key == Qt::Key_Alt || 
        key == Qt::Key_Shift || key == Qt::Key_Meta) {
        return;
    }
    
    // Escape cancels recording
    if (key == Qt::Key_Escape) {
        StopRecording();
        return;
    }
    
    m_recordedKey = key;
    m_recordedModifiers = mods;
    
    // Update display immediately
    m_displayLabel->setText(FormatKeyCombo(key, mods));
    
    event->accept();
}

void HotkeyWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (!m_recording) {
        QWidget::keyReleaseEvent(event);
        return;
    }
    
    // Stop recording when key is released (if we have a valid key)
    if (m_recordedKey != 0) {
        StopRecording();
    }
    
    event->accept();
}

bool HotkeyWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (m_recording && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        keyPressEvent(keyEvent);
        return true;
    }
    
    return QWidget::eventFilter(obj, event);
}

} // namespace UI
} // namespace StreamUP

#include "hotkey-widget.moc"