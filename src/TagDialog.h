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

class TagDialog : public QDialog {
    Q_OBJECT
public:
    explicit TagDialog(const QStringList& allTags, const QStringList& currentTags, QWidget *parent = nullptr);
    
    QStringList getSelectedTags() const;

signals:
    void tagsUpdated(QStringList tags);

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    QListWidget *m_tagList;
    QLineEdit *m_newTagInput;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    
    QStringList m_allTags;
    QStringList m_currentTags;
};

#endif // TAGDIALOG_H
