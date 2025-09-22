#include "generic_form_dialog.hpp"
#include <QtWidgets>

GenericFormDialog::GenericFormDialog(const QString& xmlDir, const QString& type, QWidget* parent)
: QDialog(parent), xmlDir_(xmlDir), type_(type)
{
    if(!loadTypeSchema(xmlDir_, type_, schema_)){
        auto* lay = new QVBoxLayout(this);
        auto* te = new QTextEdit(); te->setObjectName("rawJson");
        lay->addWidget(new QLabel("Schema not found. Enter JSON:"));
        lay->addWidget(te);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
        connect(bb,&QDialogButtonBox::accepted,this,&QDialog::accept);
        connect(bb,&QDialogButtonBox::rejected,this,&QDialog::reject);
        lay->addWidget(bb);
        return;
    }
    formRoot_ = build(schema_);
    auto* lay = new QVBoxLayout(this);
    lay->addWidget(formRoot_);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    connect(bb,&QDialogButtonBox::accepted,this,&QDialog::accept);
    connect(bb,&QDialogButtonBox::rejected,this,&QDialog::reject);
    lay->addWidget(bb);
}

QWidget* GenericFormDialog::makeWidget(const XField& f) const {
    if(f.kind=="bool")    return new QCheckBox();
    if(f.kind=="enum")    { auto* c=new QComboBox(); c->addItems(f.enumVals); return c; }
    if(f.kind=="string")  { auto* e=new QLineEdit(); if(f.maxLen>0) e->setMaxLength(f.maxLen); return e; }
    if(f.kind.startsWith("int")||f.kind.startsWith("uint")){
        auto* e=new QLineEdit();
        e->setValidator(new QIntValidator(e)); return e;
    }
    if(f.kind.startsWith("float")){
        auto* e=new QLineEdit();
        e->setValidator(new QDoubleValidator(e)); return e;
    }
    if(f.kind=="sequence"){
        auto* e=new QLineEdit(); e->setPlaceholderText("a;b;c");
        return e;
    }
    if(f.kind=="struct") {
        // 미리보기 + 편집 버튼
        auto* box = new QWidget();
        box->setObjectName(f.name);
        auto* v = new QVBoxLayout(box); v->setContentsMargins(0,0,0,0);

        auto* preview = new QPlainTextEdit();
        preview->setReadOnly(true);
        preview->setPlaceholderText("{ }");  // 보기용
        preview->setMinimumHeight(64);

        auto* btn = new QPushButton("Edit...");
        QObject::connect(
            btn, &QPushButton::clicked,
            this,  // context: 이 다이얼로그 자신
            [this, f, preview]() {
                const QString nested = f.nestedType.isEmpty() ? f.name : f.nestedType;
                XTypeSchema child;
                if (!loadTypeSchema(xmlDir_, nested, child)) return;

                GenericFormDialog sub(this->xmlDir_, nested, nullptr);  // 부모로 this 넘겨도 됨
                if (sub.exec() == QDialog::Accepted) {
                    const QJsonObject j = sub.toJson();
                    preview->setPlainText(QString::fromUtf8(QJsonDocument(j).toJson(QJsonDocument::Indented)));
                    preview->setProperty("json_compact", QJsonDocument(j).toJson(QJsonDocument::Compact));
                }
            });

        v->addWidget(preview);
        v->addWidget(btn, 0, Qt::AlignRight);
        return box;
    }
    // fallback
    auto* te=new QPlainTextEdit(); te->setPlaceholderText("{ \"nested\": ... }");
    te->setFixedHeight(80);
    return te;
}

QWidget* GenericFormDialog::build(const XTypeSchema& s) {
    auto* w = new QWidget();
    auto* form = new QFormLayout(w);
    for (const auto& f : s.fields) {
        QWidget* ed = makeWidget(f);
        ed->setObjectName(f.name);
        if (auto* le = qobject_cast<QLineEdit*>(ed); le && f.required) {
            le->setPlaceholderText("(required)");
        }
        form->addRow(new QLabel(f.name), ed);
    }
    return w;
}

void GenericFormDialog::collect(QWidget* w, const XTypeSchema& s, QJsonObject& out) const {
    for (const auto& f : s.fields) {
        QObject* obj = w->findChild<QObject*>(f.name);
        if(!obj) continue;
        if(f.kind=="bool"){
            out[f.name] = qobject_cast<QCheckBox*>(obj)->isChecked();
        } else if(f.kind=="enum"){
            out[f.name] = qobject_cast<QComboBox*>(obj)->currentText();
        } else if(f.kind=="string"){
            out[f.name] = qobject_cast<QLineEdit*>(obj)->text();
        } else if(f.kind.startsWith("int")||f.kind.startsWith("uint")){
            out[f.name] = qobject_cast<QLineEdit*>(obj)->text().toLongLong();
        } else if(f.kind.startsWith("float")){
            out[f.name] = qobject_cast<QLineEdit*>(obj)->text().toDouble();
        } else if(f.kind=="sequence"){
            const auto sraw = qobject_cast<QLineEdit*>(obj)->text();
            out[f.name] = QJsonArray::fromStringList(sraw.split(';', Qt::SkipEmptyParts));
        } else if(f.kind=="struct") {
            auto* box = qobject_cast<QWidget*>(obj);
            auto* prev = box ? box->findChild<QPlainTextEdit*>() : nullptr;
            QJsonObject nested;
            if (prev) {
                QByteArray bytes = prev->property("json_compact").toByteArray();
                if (!bytes.isEmpty()) {
                    auto doc = QJsonDocument::fromJson(bytes);
                    if (doc.isObject()) nested = doc.object();
                }
            }
            out[f.name] = nested; // 비어있으면 {} 전송
        } else {
            // fallback: 기존 방식
            QJsonObject nested;
            const auto txt = qobject_cast<QPlainTextEdit*>(obj)->toPlainText();
            auto doc = QJsonDocument::fromJson(txt.toUtf8());
            if(doc.isObject()) nested = doc.object();
            out[f.name] = nested;
        }
    }
}

QJsonObject GenericFormDialog::toJson() const {
    if (!formRoot_) {
        const auto* te = findChild<QTextEdit*>("rawJson");
        if(!te) return {};
        const auto doc = QJsonDocument::fromJson(te->toPlainText().toUtf8());
        return doc.isObject()? doc.object() : QJsonObject{};
    }
    QJsonObject obj; collect(formRoot_, schema_, obj);
    return obj;
}
