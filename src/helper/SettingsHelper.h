#pragma once

#include <QtCore/qobject.h>
#include <QSettings>
#include <QScopedPointer>
#include "../singleton.h"

class SettingsHelper : public QObject {

Q_OBJECT

private:
    explicit SettingsHelper(QObject *parent = nullptr);

public:
SINGLETON(SettingsHelper)

    ~SettingsHelper() override;

    void init(char *argv[]);

    // 夜间模式
    Q_INVOKABLE void saveDarkMode(int darkModel) {
        save("darkMode", darkModel);
    }

    Q_INVOKABLE int getDarkMode() {
        return get("darkMode", QVariant(0)).toInt();
    }

    // 表格里软件的详细信息
    Q_INVOKABLE void saveExeInfos(const QString &exeInfos) {
        save("exeInfos", exeInfos);
    }

    Q_INVOKABLE QString getExeInfos() {
        return get("exeInfos", QVariant("")).toString();
    }

    // 软件列表
    Q_INVOKABLE void saveExistingFilePath(const QString &existingFilePath) {
        save("existingFilePath", existingFilePath);
    }

    Q_INVOKABLE QString getExistingFilePath() {
        return get("existingFilePath", QVariant("")).toString();
    }

    // 记录软件打开次数
    Q_INVOKABLE void saveOpenCount(const int &openCount) {
        save("openCount", openCount);
    }

    Q_INVOKABLE int getOpenCount() {
        return get("openCount", QVariant("")).toInt();
    }

    // 开机自启动
    Q_INVOKABLE void saveAutoStart(bool autoStart) {
        save("autoStart", autoStart);
    }

    Q_INVOKABLE bool getAutoStart() {
        return get("autoStart", QVariant(0)).toBool();
    }

    // 语言跟随显示
    Q_INVOKABLE void saveLanguageFollowDisplay(bool enabled) {
        save("languageFollowDisplay", enabled);
    }

    Q_INVOKABLE bool getLanguageFollowDisplay() {
        return get("languageFollowDisplay", QVariant(true)).toBool();
    }

    // 语言跟随浮窗不透明度 (10 - 50), 数值越小越透明
    Q_INVOKABLE void saveLanguageOverlayOpacity(int opacity) {
        save("languageOverlayOpacity", opacity);
    }

    Q_INVOKABLE int getLanguageOverlayOpacity() {
        return get("languageOverlayOpacity", QVariant(30)).toInt();
    }

    // 语言跟随浮窗背景色: "auto" 为跟随明暗主题, 其余为 "#RRGGBB"
    Q_INVOKABLE void saveLanguageOverlayColor(const QString &color) {
        save("languageOverlayColor", color);
    }

    Q_INVOKABLE QString getLanguageOverlayColor() {
        return get("languageOverlayColor", QVariant("auto")).toString();
    }

    // 是否启用"开关浮窗"的全局快捷键
    Q_INVOKABLE void saveLanguageFollowHotkeyEnabled(bool enabled) {
        save("languageFollowHotkeyEnabled", enabled);
    }

    Q_INVOKABLE bool getLanguageFollowHotkeyEnabled() {
        return get("languageFollowHotkeyEnabled", QVariant(false)).toBool();
    }

    // 开关浮窗显示的全局快捷键 (如 "Ctrl+Shift+L")
    Q_INVOKABLE void saveLanguageFollowHotkey(const QString &sequence) {
        save("languageFollowHotkey", sequence);
    }

    Q_INVOKABLE QString getLanguageFollowHotkey() {
        return get("languageFollowHotkey", QVariant("Ctrl+Shift+L")).toString();
    }

private:
    void save(const QString &key, QVariant val);

    QVariant get(const QString &key, QVariant def = {});

private:
    QScopedPointer<QSettings> m_settings;
};
