#include "ConfigDialog.h"

#include "ThemeUtils.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QTimer>

ConfigDialog::ConfigDialog(ConfigManager *configManager, QWidget *parent)
    : QDialog(parent), m_configManager(configManager)
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

    connect(m_tabWidget, &QTabWidget::currentChanged, [this]()
            {
        layout()->setSizeConstraint(QLayout::SetFixedSize);
        QTimer::singleShot(100, [this]()
                           { layout()->setSizeConstraint(QLayout::SetDefaultConstraint); }); });

    setupActionButtons(mainLayout);
}

void ConfigDialog::setupTabs(QVBoxLayout *mainLayout)
{
    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);

    setupGeneralTab();
    setupTranslationTab();
    setupArchiveTab();
    setupOtherTab();
}
