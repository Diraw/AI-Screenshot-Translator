#include "App.h"

#include "ThemeUtils.h"

#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#include "TranslationManager.h"

void App::quitApp()
{
    // Quit hotkey should show a lightweight prompt before exiting.
    // Close prompt on any click/key, and quit shortly after even if user does nothing.
    static bool s_quitRequested = false;
    if (s_quitRequested)
    {
        qApp->quit();
        return;
    }
    s_quitRequested = true;

    QDialog *toast = new QDialog(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    toast->setAttribute(Qt::WA_DeleteOnClose);
    toast->setObjectName(QStringLiteral("quitToast"));
    toast->setFocusPolicy(Qt::StrongFocus);

    QLabel *label = new QLabel(TranslationManager::instance().tr("quit_toast"), toast);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);

    QVBoxLayout *layout = new QVBoxLayout(toast);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->addWidget(label);

    const bool isDark = ThemeUtils::isSystemDark();
    if (isDark)
    {
        toast->setStyleSheet(
            "#quitToast{background:rgba(25,25,25,230);border:1px solid rgba(255,255,255,40);border-radius:10px;}"
            "#quitToast QLabel{color:#ffffff;font-size:13px;}");
    }
    else
    {
        toast->setStyleSheet(
            "#quitToast{background:rgba(255,255,255,235);border:1px solid rgba(0,0,0,40);border-radius:10px;}"
            "#quitToast QLabel{color:#111111;font-size:13px;}");
    }

    toast->adjustSize();
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen)
    {
        const QRect area = screen->availableGeometry();
        toast->move(area.center() - QPoint(toast->width() / 2, toast->height() / 2));
    }

    class DismissOnAnyInputFilter : public QObject
    {
    public:
        explicit DismissOnAnyInputFilter(QDialog *dialog)
            : QObject(dialog), m_dialog(dialog)
        {
        }

    protected:
        bool eventFilter(QObject *obj, QEvent *event) override
        {
            Q_UNUSED(obj);
            if (!m_dialog)
                return false;

            switch (event->type())
            {
            case QEvent::KeyPress:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::Wheel:
                m_dialog->close();
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
                return false;
            default:
                return false;
            }
        }

    private:
        QPointer<QDialog> m_dialog;
    };

    // Close on any key/mouse anywhere in app.
    DismissOnAnyInputFilter *filter = new DismissOnAnyInputFilter(toast);
    qApp->installEventFilter(filter);

    toast->show();
    toast->raise();
    toast->activateWindow();

    // Ensure we still exit even if user does nothing.
    QTimer::singleShot(500, qApp, &QCoreApplication::quit);
}

void App::checkForThemeChange()
{
    bool currentDark = ThemeUtils::isSystemDark();
    if (currentDark != m_lastTopBarDark)
    {
        qDebug() << "Theme change detected: " << (currentDark ? "Dark" : "Light");
        m_lastTopBarDark = currentDark;
        updateAllWindowThemes(currentDark);
    }
}

void App::updateAllWindowThemes(bool isDark)
{
    if (m_summaryWindow)
    {
        m_summaryWindow->updateTheme(isDark);
    }

    if (m_activeConfigDialog)
    {
        m_activeConfigDialog->updateTheme(isDark);
    }

    for (auto w : m_activeWindows)
    {
        if (!w)
            continue;
        if (ResultWindow *rw = qobject_cast<ResultWindow *>(w.data()))
        {
            rw->updateTheme(isDark);
        }
    }
}
