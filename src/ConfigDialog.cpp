#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include "HistoryManager.h"

void ConfigDialog::onImportLegacyHistory()
{
    TranslationManager &tm = TranslationManager::instance();

    if (!m_historyManager)
    {
        QMessageBox::warning(this, tm.tr("history_io_title"), tm.tr("history_manager_unavailable"));
        return;
    }

    const QString defaultJsonPath = QDir(m_historyManager->getStoragePath()).filePath("history.json");
    const QString initialPath = QFile::exists(defaultJsonPath) ? defaultJsonPath : m_historyManager->getStoragePath();
    const QString jsonPath = QFileDialog::getOpenFileName(
        this,
        tm.tr("import_legacy_title"),
        initialPath,
        tm.tr("json_files") + ";;All Files (*)");
    if (jsonPath.trimmed().isEmpty())
        return;

    int importedCount = 0;
    QString error;
    const bool ok = m_historyManager->importLegacyJson(jsonPath, &importedCount, &error);
    if (!ok)
    {
        QMessageBox::warning(
            this,
            tm.tr("import_legacy_title"),
            error.isEmpty() ? tm.tr("import_legacy_failed") : error);
        return;
    }

    QMessageBox::information(
        this,
        tm.tr("import_legacy_title"),
        tm.tr("import_legacy_success").arg(importedCount));
}

void ConfigDialog::onExportHistory()
{
    TranslationManager &tm = TranslationManager::instance();

    if (!m_historyManager)
    {
        QMessageBox::warning(this, tm.tr("history_io_title"), tm.tr("history_manager_unavailable"));
        return;
    }

    const QString initialPath = QDir(m_historyManager->getStoragePath()).filePath("history_export.json");
    const QString jsonPath = QFileDialog::getSaveFileName(
        this,
        tm.tr("export_history_title"),
        initialPath,
        tm.tr("json_files") + ";;All Files (*)");
    if (jsonPath.trimmed().isEmpty())
        return;

    int exportedCount = 0;
    QString error;
    const bool ok = m_historyManager->exportHistoryJson(jsonPath, &exportedCount, &error);
    if (!ok)
    {
        QMessageBox::warning(
            this,
            tm.tr("export_history_title"),
            error.isEmpty() ? tm.tr("export_history_failed") : error);
        return;
    }

    QMessageBox::information(
        this,
        tm.tr("export_history_title"),
        tm.tr("export_history_success").arg(exportedCount));
}

ConfigDialog::~ConfigDialog() = default;

void ConfigDialog::setGlobalHotkeyConflictKeys(const QStringList &labelKeys, bool focusConflicts)
{
    m_globalHotkeyConflictKeys = labelKeys;
    m_globalHotkeyConflictKeys.removeDuplicates();
    applyGlobalHotkeyConflictIndicators(focusConflicts);
}

QLineEdit *ConfigDialog::globalHotkeyEditForKey(const QString &labelKey) const
{
    if (labelKey == QStringLiteral("lbl_shot_hotkey"))
        return m_hotkeyEdit;
    if (labelKey == QStringLiteral("lbl_sum_hotkey"))
        return m_summaryHotkeyEdit;
    if (labelKey == QStringLiteral("lbl_set_hotkey"))
        return m_settingsHotkeyEdit;
    if (labelKey == QStringLiteral("lbl_quit_hotkey"))
        return m_quitHotkeyEdit;
    return nullptr;
}

void ConfigDialog::applyGlobalHotkeyConflictIndicators(bool focusConflicts)
{
    const QStringList trackedKeys = {
        QStringLiteral("lbl_shot_hotkey"),
        QStringLiteral("lbl_sum_hotkey"),
        QStringLiteral("lbl_set_hotkey"),
        QStringLiteral("lbl_quit_hotkey"),
    };

    for (const QString &key : trackedKeys)
    {
        if (QLineEdit *edit = globalHotkeyEditForKey(key))
        {
            const bool conflicted = m_globalHotkeyConflictKeys.contains(key);
            edit->setStyleSheet(conflicted ? QStringLiteral("border: 2px solid #d94b4b; border-radius: 4px;")
                                           : QString());
        }
    }

    const int otherTabIndex = m_tabWidget ? m_tabWidget->indexOf(m_otherTab) : -1;
    if (otherTabIndex >= 0)
    {
        QString baseText = TranslationManager::instance().tr("tab_other");
        if (!m_globalHotkeyConflictKeys.isEmpty())
            baseText += QStringLiteral(" *");
        m_tabWidget->setTabText(otherTabIndex, baseText);
    }

    if (!focusConflicts || m_globalHotkeyConflictKeys.isEmpty() || !m_tabWidget)
        return;

    m_tabWidget->setCurrentWidget(m_otherTab);

    QLineEdit *target = nullptr;
    for (const QString &key : trackedKeys)
    {
        if (m_globalHotkeyConflictKeys.contains(key))
        {
            target = globalHotkeyEditForKey(key);
            if (target)
                break;
        }
    }

    if (!target)
        return;

    QPointer<QLineEdit> targetEdit = target;
    QTimer::singleShot(0, this, [targetEdit]()
                       {
        if (!targetEdit)
            return;
        targetEdit->setFocus(Qt::OtherFocusReason);
        targetEdit->selectAll(); });
}



