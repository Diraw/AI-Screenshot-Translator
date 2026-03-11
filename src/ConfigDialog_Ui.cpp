#include "ConfigDialog.h"

#include "ThemeUtils.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QTimer>

ConfigDialog::ConfigDialog(ConfigManager *configManager, HistoryManager *historyManager, QWidget *parent)
    : QDialog(parent), m_configManager(configManager), m_historyManager(historyManager)
{
    setWindowTitle("Settings");

    m_testNam = new QNetworkAccessManager(this);

    updateTheme(ThemeUtils::isSystemDark());
    setupDialogUi();

    updateProfileList();
    loadFromConfig();
    setupProfilesWatcher();
    retranslateUi();
}

void ConfigDialog::refreshStoragePathPlaceholder()
{
    if (!m_storagePathEdit)
        return;

    m_storagePathEdit->setPlaceholderText(QDir::toNativeSeparators(ConfigManager::defaultStoragePath()));
}

bool ConfigDialog::validateStoragePathInput(const QString &pathText, QString *resolvedPath)
{
    const QString resolved = ConfigManager::resolveStoragePath(pathText);
    QString errorMessage;
    if (!ConfigManager::ensureWritableDirectory(resolved, &errorMessage))
    {
        QMessageBox::warning(
            this, "Storage Path Not Writable",
            QString("The selected storage directory is not writable:\n%1\n\nReason: %2\n\nPlease choose another directory.")
                .arg(QDir::toNativeSeparators(resolved),
                     errorMessage.isEmpty() ? QString("Write access denied.") : errorMessage));
        return false;
    }

    if (resolvedPath)
        *resolvedPath = resolved;
    return true;
}

void ConfigDialog::browseForStoragePath()
{
    const QString currentText = m_storagePathEdit ? m_storagePathEdit->text().trimmed() : QString();
    const QString initialDir = ConfigManager::resolveStoragePath(currentText);
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Storage Directory", initialDir);
    if (dir.isEmpty())
        return;

    QString resolvedPath;
    if (!validateStoragePathInput(dir, &resolvedPath))
        return;

    if (m_storagePathEdit)
        m_storagePathEdit->setText(QDir::toNativeSeparators(resolvedPath));
}

void ConfigDialog::setupDialogUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    setupProfileSection(mainLayout);
    setupTabs(mainLayout);

    setupActionButtons(mainLayout);

    // Vertical resize behavior: keep profile/actions fixed, only tabs area gets extra height.
    mainLayout->setStretch(0, 0);
    mainLayout->setStretch(1, 1);
    mainLayout->setStretch(2, 0);

    // Keep dialog baseline size aligned with the General tab; other tabs should adapt via scroll areas.
    QTimer::singleShot(0, this, [this]()
                       {
        if (!m_tabWidget || !m_generalTab)
            return;

        const bool hasSavedGeometry = m_configManager && !m_configManager->getConfig().configWindowGeometry.isEmpty();

        if (!hasSavedGeometry)
        {
            const int previousIndex = m_tabWidget->currentIndex();
            const int generalIndex = m_tabWidget->indexOf(m_generalTab);
            if (generalIndex >= 0)
                m_tabWidget->setCurrentIndex(generalIndex);

            adjustSize();
            const QSize baselineSize = size();

            if (previousIndex >= 0)
                m_tabWidget->setCurrentIndex(previousIndex);

            resize(baselineSize);
            // Keep width baseline, but do not lock window minimum height.
            setMinimumWidth(baselineSize.width());
        }

        // Release prompt fixed-height seed so it can expand when the window grows.
        if (m_promptEdit)
        {
            m_promptEdit->setMinimumHeight(30);
            m_promptEdit->setMaximumHeight(QWIDGETSIZE_MAX);
        } });
}

void ConfigDialog::setupTabs(QVBoxLayout *mainLayout)
{
    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);

    setupGeneralTab();
    setupTranslationTab();
    setupArchiveTab();
    setupOtherTab();
    setupAdvancedApiTab();
}
