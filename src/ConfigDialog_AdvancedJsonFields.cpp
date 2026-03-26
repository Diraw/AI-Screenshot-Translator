#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace
{
QString jsonValueToSingleLine(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isBool())
        return value.toBool() ? "true" : "false";
    if (value.isDouble())
        return QString::number(value.toDouble(), 'g', 15);
    if (value.isNull() || value.isUndefined())
        return "null";
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    return {};
}

void collectJsonLeafPaths(const QJsonValue &value, const QString &path,
                          QStringList &outPaths, QHash<QString, QString> &outPreview,
                          int depth = 0)
{
    constexpr int kMaxDepth = 8;
    constexpr int kMaxArrayItems = 24;
    constexpr int kMaxPreviewLen = 140;

    if (depth > kMaxDepth)
        return;

    if (value.isObject())
    {
        const QJsonObject obj = value.toObject();
        if (obj.isEmpty())
        {
            if (!path.isEmpty())
            {
                outPaths.append(path);
                outPreview.insert(path, "{}");
            }
            return;
        }

        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        {
            const QString childPath = path.isEmpty() ? it.key() : QString("%1.%2").arg(path, it.key());
            collectJsonLeafPaths(it.value(), childPath, outPaths, outPreview, depth + 1);
        }
        return;
    }

    if (value.isArray())
    {
        const QJsonArray arr = value.toArray();
        if (arr.isEmpty())
        {
            if (!path.isEmpty())
            {
                outPaths.append(path);
                outPreview.insert(path, "[]");
            }
            return;
        }

        const int limit = qMin(arr.size(), kMaxArrayItems);
        for (int i = 0; i < limit; ++i)
        {
            const QString childPath = path.isEmpty() ? QString("[%1]").arg(i) : QString("%1[%2]").arg(path).arg(i);
            collectJsonLeafPaths(arr.at(i), childPath, outPaths, outPreview, depth + 1);
        }
        return;
    }

    if (path.isEmpty())
        return;

    QString preview = jsonValueToSingleLine(value);
    preview.replace('\n', ' ');
    if (preview.size() > kMaxPreviewLen)
        preview = preview.left(kMaxPreviewLen - 3) + "...";
    outPaths.append(path);
    outPreview.insert(path, preview);
}

QStringList readDebugFieldsFromTemplateRoot(const QJsonObject &root)
{
    QStringList fields;
    const QJsonArray arr = root.value("debug_fields").toArray();
    for (const QJsonValue &v : arr)
    {
        const QString f = v.toString().trimmed();
        if (!f.isEmpty())
            fields.append(f);
    }
    fields.removeDuplicates();
    return fields;
}
} // namespace

void ConfigDialog::onPickAdvancedJsonFields()
{
    if (!m_hasLastAdvancedApiTestJson || m_lastAdvancedApiTestJson.isNull())
        return;

    TranslationManager &tm = TranslationManager::instance();

    QJsonObject templateRoot;
    QString parseErr;
    if (!parseAdvancedTemplateJson(templateRoot, parseErr))
    {
        if (m_advancedApiResultEdit)
            m_advancedApiResultEdit->appendPlainText(QString("\n%1").arg(tm.tr("adv_json_template_parse_failed").arg(parseErr)));
        return;
    }

    QStringList currentFields = readDebugFieldsFromTemplateRoot(templateRoot);
    QStringList candidatePaths;
    QHash<QString, QString> previews;
    if (m_lastAdvancedApiTestJson.isObject())
        collectJsonLeafPaths(m_lastAdvancedApiTestJson.object(), QString(), candidatePaths, previews);
    else if (m_lastAdvancedApiTestJson.isArray())
        collectJsonLeafPaths(m_lastAdvancedApiTestJson.array(), QString(), candidatePaths, previews);

    candidatePaths.removeDuplicates();
    if (candidatePaths.isEmpty())
    {
        if (m_advancedApiResultEdit)
            m_advancedApiResultEdit->appendPlainText(QString("\n%1").arg(tm.tr("adv_json_no_selectable_fields")));
        return;
    }

    QDialog picker(this);
    picker.setWindowTitle(tm.tr("adv_json_picker_title"));
    picker.resize(760, 520);

    auto *layout = new QVBoxLayout(&picker);
    auto *hint = new QLabel(tm.tr("adv_json_picker_hint"), &picker);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *list = new QListWidget(&picker);
    list->setSelectionMode(QAbstractItemView::MultiSelection);
    QSet<QString> currentSet;
    for (const QString &field : currentFields)
        currentSet.insert(field);

    for (const QString &path : candidatePaths)
    {
        const QString label = QString("%1 = %2").arg(path, previews.value(path));
        auto *item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, path);
        if (currentSet.contains(path))
            item->setSelected(true);
    }
    layout->addWidget(list, 1);

    auto *quickRow = new QHBoxLayout();
    auto *selectAllBtn = new QPushButton(tm.tr("btn_select_all"), &picker);
    auto *clearBtn = new QPushButton(tm.tr("btn_clear"), &picker);
    quickRow->addWidget(selectAllBtn, 0);
    quickRow->addWidget(clearBtn, 0);
    quickRow->addStretch(1);
    layout->addLayout(quickRow);
    connect(selectAllBtn, &QPushButton::clicked, list, [list]()
            { list->selectAll(); });
    connect(clearBtn, &QPushButton::clicked, list, [list]()
            { list->clearSelection(); });

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &picker);
    if (QPushButton *okBtn = box->button(QDialogButtonBox::Ok))
        okBtn->setText(tm.tr("ok"));
    if (QPushButton *cancelBtn = box->button(QDialogButtonBox::Cancel))
        cancelBtn->setText(tm.tr("cancel"));
    connect(box, &QDialogButtonBox::accepted, &picker, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &picker, &QDialog::reject);
    layout->addWidget(box);

    if (picker.exec() != QDialog::Accepted)
        return;

    QStringList selectedFields;
    const QList<QListWidgetItem *> pickedItems = list->selectedItems();
    for (QListWidgetItem *item : pickedItems)
    {
        const QString path = item->data(Qt::UserRole).toString().trimmed();
        if (!path.isEmpty())
            selectedFields.append(path);
    }
    selectedFields.removeDuplicates();

    QJsonArray debugFields;
    for (const QString &f : selectedFields)
        debugFields.append(f);
    templateRoot["debug_fields"] = debugFields;

    const QString templateText = QString::fromUtf8(QJsonDocument(templateRoot).toJson(QJsonDocument::Indented));
    if (m_advancedApiTemplateEdit)
    {
        QSignalBlocker blocker(m_advancedApiTemplateEdit);
        m_isSyncingAdvanced = true;
        m_advancedApiTemplateEdit->setPlainText(templateText);
        m_isSyncingAdvanced = false;
    }

    m_advancedTemplateDetached = true;
    updateAdvancedTemplateStatusLabel();

    if (m_advancedApiResultEdit)
        m_advancedApiResultEdit->appendPlainText(tm.tr("adv_json_selected_count").arg(selectedFields.size()));
}
