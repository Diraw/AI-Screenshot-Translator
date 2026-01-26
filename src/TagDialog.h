#ifndef TAGDIALOG_H
#define TAGDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <functional>

class QEvent;
class QKeyEvent;

class TagDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TagDialog(const QStringList &allTags,
                       const QStringList &currentTags,
                       QWidget *parent = nullptr,
                       std::function<bool()> escapeInterceptor = {});

    QStringList getSelectedTags() const;

signals:
    void tagsUpdated(QStringList tags);

private slots:
    void onOkClicked();
    void onCancelClicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QListWidget *m_tagList;
    QLineEdit *m_newTagInput;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;

    QStringList m_allTags;
    QStringList m_currentTags;

    std::function<bool()> m_escapeInterceptor;
};

#endif // TAGDIALOG_H
