#ifndef STREAMUP_HOTKEY_WIDGET_HPP
#define STREAMUP_HOTKEY_WIDGET_HPP

#include <QWidget>
#include <QPushButton>
#include <QKeyEvent>
#include <QLabel>
#include <obs-data.h>

namespace StreamUP {
namespace UI {

/**
 * @brief Custom widget for capturing and displaying hotkey combinations
 * 
 * This widget provides an interface similar to OBS's native hotkey inputs,
 * allowing users to click and record key combinations for StreamUP hotkeys.
 */
class HotkeyWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HotkeyWidget(const QString& hotkeyName, QWidget* parent = nullptr);
    ~HotkeyWidget();

    /**
     * @brief Set the current hotkey combination from OBS data
     * @param hotkeyData OBS hotkey data array
     */
    void SetHotkey(obs_data_array_t* hotkeyData);

    /**
     * @brief Get the current hotkey combination as OBS data
     * @return obs_data_array_t* Current hotkey data (caller must release)
     */
    obs_data_array_t* GetHotkey() const;

    /**
     * @brief Clear the current hotkey assignment
     */
    void ClearHotkey();

signals:
    /**
     * @brief Emitted when the hotkey combination changes
     * @param hotkeyName The name of the hotkey that changed
     * @param hotkeyData The new hotkey data
     */
    void HotkeyChanged(const QString& hotkeyName, obs_data_array_t* hotkeyData);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void OnRecordButtonClicked();
    void OnClearButtonClicked();

private:
    void UpdateDisplay();
    void StartRecording();
    void StopRecording();
    QString FormatKeyCombo(int key, Qt::KeyboardModifiers modifiers);
    QString QtKeyToOBSKey(int qtKey);
    obs_data_array_t* CreateHotkeyData(int key, Qt::KeyboardModifiers modifiers);

    QString m_hotkeyName;
    QPushButton* m_recordButton;
    QPushButton* m_clearButton;
    QLabel* m_displayLabel;
    
    bool m_recording;
    int m_recordedKey;
    Qt::KeyboardModifiers m_recordedModifiers;
    obs_data_array_t* m_currentHotkeyData;
};

} // namespace UI
} // namespace StreamUP

#endif // STREAMUP_HOTKEY_WIDGET_HPP
