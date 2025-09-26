#include "../include/generic_form_dialog.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QDateTime>  // 시간 타입 기본값을 위해 추가

GenericFormDialog::GenericFormDialog(XmlTypeCatalog* catalog, const QString& typeName, QWidget* parent)
    : QDialog(parent), catalog_(catalog), typeName_(typeName), formRoot_(nullptr) {
    
    setWindowTitle(QString("Form: %1").arg(typeName));
    resize(400, 300);
    
    // 먼저 안전성 검증
    if (!catalog) {
        qWarning() << "GenericFormDialog: catalog is null!";
        showErrorForm("Invalid catalog pointer");
        return;
    }
    
    if (typeName.isEmpty()) {
        qWarning() << "GenericFormDialog: typeName is empty!";
        showErrorForm("Empty type name");
        return;
    }
    
    if (!catalog->hasType(typeName)) {
        qWarning() << "GenericFormDialog: type not found:" << typeName;
        showErrorForm(QString("Type '%1' not found. Enter JSON manually:").arg(typeName));
        return;
    }
    
    // 생성자에서는 UI만 설정하고, buildForm은 지연 호출
    setupNormalForm();
}

void GenericFormDialog::showErrorForm(const QString& message) {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(message));
    auto* te = new QTextEdit(); 
    te->setObjectName("rawJson");
    layout->addWidget(te);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    connect(bb,&QDialogButtonBox::accepted,this,&QDialog::accept);
    connect(bb,&QDialogButtonBox::rejected,this,&QDialog::reject);
    layout->addWidget(bb);
}

void GenericFormDialog::setupNormalForm() {
    // 정상적인 폼 설정 - buildForm은 실제 필요할 때 호출
    auto* layout = new QVBoxLayout(this);
    
    // 플레이스홀더 라벨
    auto* loadingLabel = new QLabel("Loading form...", this);
    layout->addWidget(loadingLabel);
    
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
        connect(bb, &QDialogButtonBox::accepted, this, [this]() {
            // OK 버튼 누를 때 결과 캐싱
            if (formRoot_ && catalog_ && catalog_->hasType(typeName_)) {
                const XTypeSchema& schema = catalog_->getType(typeName_);
                cachedResult_ = collect(formRoot_, schema);
            } else {
                cachedResult_ = QJsonObject{};
            }
            accept();
        });
    connect(bb,&QDialogButtonBox::rejected,this,&QDialog::reject);
    layout->addWidget(bb);
    
    // 폼 빌드를 지연 실행 (이벤트 루프 이후) - 람다 사용으로 안전성 확보
    QTimer::singleShot(0, [this]() {
        buildFormDelayed();
    });
}

void GenericFormDialog::buildFormDelayed() {
    if (!catalog_ || !catalog_->hasType(typeName_)) {
        qWarning() << "buildFormDelayed: Invalid state!";
        return;
    }
    
    try {
        const XTypeSchema& schema = catalog_->getType(typeName_);
        formRoot_ = buildForm(schema);
        
        if (formRoot_) {
            // 기존 레이아웃의 첫 번째 위젯(Loading label) 교체
            auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
            if (layout && layout->count() > 0) {
                // Loading label 제거
                QLayoutItem* item = layout->takeAt(0);
                if (item && item->widget()) {
                    item->widget()->deleteLater();
                    delete item;
                }
                // 실제 폼 삽입 (버튼 상자 앞에)
                layout->insertWidget(0, formRoot_);
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "buildFormDelayed exception:" << e.what();
        showErrorForm(QString("Form build error: %1").arg(e.what()));
    } catch (...) {
        qWarning() << "buildFormDelayed: Unknown exception";
        showErrorForm("Unknown form build error");
    }
}

QWidget* GenericFormDialog::buildForm(const XTypeSchema& schema) {
    if (schema.fields.isEmpty()) {
        // 빈 스키마 처리
        auto* emptyForm = new QWidget();
        auto* layout = new QVBoxLayout(emptyForm);
        layout->addWidget(new QLabel("No fields to display"));
        return emptyForm;
    }
    
    auto* form = new QWidget();
    // Attribute-Value 형식으로 UI 구성 (요구사항 3)
    auto* layout = new QVBoxLayout(form);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    
    // 폼 제목 추가
    auto* titleLabel = new QLabel(QString("<b>%1 Attributes</b>").arg(schema.name));
    titleLabel->setStyleSheet("font-size: 14px; color: #2c3e50; padding: 5px;");
    layout->addWidget(titleLabel);
    
    try {
        // 안전한 인덱스 기반 반복으로 변경
        for (int fieldIndex = 0; fieldIndex < schema.fields.size(); ++fieldIndex) {
            const auto& field = schema.fields.at(fieldIndex);
            if (field.name.isNull() && field.kind.isNull()) {
                qWarning() << "Invalid field detected at index" << fieldIndex;
                continue;
            }
            // 최종 타입 해석
            QString typeToResolve = field.kind == "sequence" && !field.sequenceElementType.isEmpty() ? field.sequenceElementType : (!field.nestedType.isEmpty() ? field.nestedType : field.kind);
            XTypeSchema resolvedSchema = catalog_ ? catalog_->resolveType(typeToResolve) : XTypeSchema{};
            XField finalField = field;
            if (!resolvedSchema.fields.isEmpty()) {
                const XField& resolved = resolvedSchema.fields.first();
                // name은 원본 유지, 타입 정보만 덮어씀
                finalField.kind = resolved.kind;
                finalField.nestedType = resolved.nestedType;
                finalField.sequenceElementType = resolved.sequenceElementType;
                finalField.enumVals = resolved.enumVals;
                finalField.isSequenceOfPrimitive = resolved.isSequenceOfPrimitive;
                finalField.isSequenceOfString = resolved.isSequenceOfString;
                finalField.maxLen = resolved.maxLen > 0 ? resolved.maxLen : finalField.maxLen;
                finalField.upperBound = resolved.upperBound > 0 ? resolved.upperBound : finalField.upperBound;
            }
            QString safeName = field.name.isEmpty() ? QString("field_%1").arg(fieldIndex) : field.name;
            auto* attrContainer = new QWidget(form);
            auto* attrLayout = new QHBoxLayout(attrContainer);
            attrLayout->setContentsMargins(5, 3, 5, 3);
            auto* attrLabel = new QLabel(safeName + ":", attrContainer);
            attrLabel->setMinimumWidth(120);
            attrLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            attrLabel->setStyleSheet("font-weight: bold; color: #34495e;");
            if (field.isKey) {
                attrLabel->setText(safeName + " [Key]:");
                attrLabel->setStyleSheet("font-weight: bold; color: #e74c3c;");
            }
            QWidget* valueWidget = makeFieldWidget(finalField, attrContainer);
            if (valueWidget) {
                valueWidget->setObjectName(safeName);
                attrLayout->addWidget(attrLabel);
                attrLayout->addWidget(valueWidget, 1);
                layout->addWidget(attrContainer);
            } else {
                auto* errorContainer = new QWidget(form);
                auto* errorLayout = new QHBoxLayout(errorContainer);
                auto* errorLabel = new QLabel(safeName + ":", errorContainer);
                errorLabel->setMinimumWidth(120);
                errorLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                auto* errorValue = new QLabel("Error creating widget", errorContainer);
                errorValue->setObjectName(safeName);
                errorValue->setStyleSheet("color: red;");
                errorLayout->addWidget(errorLabel);
                errorLayout->addWidget(errorValue, 1);
                layout->addWidget(errorContainer);
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in buildForm:" << e.what();
        // 예외 발생 시 안전한 대안 위젯 반환
        auto* errorWidget = new QWidget();
        auto* errorLayout = new QVBoxLayout(errorWidget);
        errorLayout->addWidget(new QLabel(QString("Error: %1").arg(e.what())));
        delete form; // 기존 form 정리
        return errorWidget;
    } catch (...) {
        qWarning() << "Unknown exception in buildForm";
        auto* errorWidget = new QWidget();
        auto* errorLayout = new QVBoxLayout(errorWidget);
        errorLayout->addWidget(new QLabel("Unknown error occurred"));
        delete form; // 기존 form 정리  
        return errorWidget;
    }
    
    return form;
}

QWidget* GenericFormDialog::makeFieldWidget(const XField& field, QWidget* parent) {
    // 안전한 문자열 처리
    QString safeKind = field.kind.isEmpty() ? "string" : field.kind;
    // 최종 타입 해석
    QString typeToResolve = safeKind == "sequence" && !field.sequenceElementType.isEmpty() ? field.sequenceElementType : (!field.nestedType.isEmpty() ? field.nestedType : safeKind);
    XTypeSchema finalSchema = catalog_ ? catalog_->resolveType(typeToResolve) : XTypeSchema{};
    XField finalField = (!finalSchema.fields.isEmpty()) ? finalSchema.fields.first() : field;
    finalField.name = field.name;
    try {
        QVariant defaultVal = getDefaultValue(finalField);
        if (safeKind == "bool") {
            auto* cb = new QCheckBox(parent);
            cb->setChecked(defaultVal.toString().toLower() == "true" || defaultVal.toBool());
            return cb;
        }

        if (safeKind == "enum") {
            auto* c = new QComboBox(parent);
            if (!field.enumVals.isEmpty()) {
                c->addItems(field.enumVals);
                int idx = c->findText(defaultVal.toString());
                c->setCurrentIndex(idx >= 0 ? idx : 0);
            } else {
                c->addItem("(no values)");
            }
            return c;
        }

        if (safeKind == "string") {
            auto* e = new QLineEdit(parent);
            e->setText(defaultVal.toString());
            if (field.maxLen > 0) e->setMaxLength(field.maxLen);
            return e;
        }

        if (safeKind.startsWith("int") || safeKind.startsWith("uint")) {
            auto* e = new QLineEdit(parent);
            e->setText(defaultVal.toString());
            e->setValidator(new QIntValidator(e));
            return e;
        }

        if (safeKind.startsWith("float") || safeKind.startsWith("double")) {
            auto* e = new QLineEdit(parent);
            e->setText(defaultVal.toString());
            e->setValidator(new QDoubleValidator(e));
            return e;
        }

        if (safeKind == "sequence") {
            // 배열의 각 요소가 구조체라면, 내부 필드별로 폼을 재귀적으로 생성
            if (!field.nestedType.isEmpty() && catalog_ && catalog_->hasType(field.nestedType)) {
                const XTypeSchema& nestedSchema = catalog_->getType(field.nestedType);
                if (!nestedSchema.fields.isEmpty()) {
                    auto* arrayContainer = new QWidget(parent);
                    auto* arrayLayout = new QVBoxLayout(arrayContainer);
                    arrayLayout->setContentsMargins(5, 5, 5, 5);
                    // 기본적으로 1개 구조체 폼만 생성, 동적 추가/삭제는 추후 확장 가능
                    QWidget* structForm = buildForm(nestedSchema);
                    if (structForm) {
                        structForm->setStyleSheet("margin-left: 10px; border-left: 2px solid #bdc3c7; padding-left: 8px;");
                        arrayLayout->addWidget(structForm);
                    }
                    return arrayContainer;
                }
            }
            // 기존 primitive/string 배열은 QLineEdit 유지
            auto* e = new QLineEdit(parent);
            e->setText(defaultVal.toString());
            if (field.isSequenceOfString) {
                e->setPlaceholderText("Enter string text");
                if (field.upperBound > 0) e->setMaxLength(field.upperBound);
            } else if (field.isSequenceOfPrimitive) {
                e->setPlaceholderText("item1,item2,item3 (primitive values)");
            } else {
                e->setPlaceholderText("item1;item2;item3");
            }
            return e;
        }

        if (safeKind == "struct" && !field.nestedType.isEmpty() && catalog_ && catalog_->hasType(field.nestedType)) {
            auto* nestedContainer = new QWidget(parent);
            auto* nestedLayout = new QVBoxLayout(nestedContainer);
            nestedLayout->setContentsMargins(10, 5, 5, 5);
            auto* nestedHeader = new QLabel(QString("▼ %1 (%2)").arg(field.name, field.nestedType));
            nestedHeader->setStyleSheet("font-weight: bold; color: #3498db; padding: 3px; background: #ecf0f1;");
            nestedLayout->addWidget(nestedHeader);
            QWidget* nestedForm = buildForm(catalog_->getType(field.nestedType));
            if (nestedForm) {
                nestedForm->setStyleSheet("margin-left: 15px; border-left: 2px solid #bdc3c7; padding-left: 10px;");
                nestedLayout->addWidget(nestedForm);
            }
            return nestedContainer;
        }

        // 기타 타입: 기본 LineEdit (부모 지정)
        auto* e = new QLineEdit(parent);
        e->setText(defaultVal.toString());
        return e;

    } catch (const std::exception& e) {
        qWarning() << "Exception in makeFieldWidget:" << e.what();
        auto* errorWidget = new QLabel(QString("Error: %1").arg(e.what()), parent);
        errorWidget->setStyleSheet("color: red;");
        return errorWidget;
    } catch (...) {
        qWarning() << "Unknown exception in makeFieldWidget";
        auto* errorWidget = new QLabel("Widget creation error", parent);
        errorWidget->setStyleSheet("color: red;");
        return errorWidget;
    }
}

QVariant GenericFormDialog::getDefaultValue(const XField& field) const {
    QString safeKind = field.kind.isEmpty() ? "string" : field.kind;
    QString safeName = field.name.isEmpty() ? "field" : field.name;
    // 최종 타입 해석
    QString typeToResolve = safeKind == "sequence" && !field.sequenceElementType.isEmpty() ? field.sequenceElementType : (!field.nestedType.isEmpty() ? field.nestedType : safeKind);
    XTypeSchema finalSchema = catalog_ ? catalog_->resolveType(typeToResolve) : XTypeSchema{};
    XField finalField = (!finalSchema.fields.isEmpty()) ? finalSchema.fields.first() : field;
    finalField.name = field.name;
    safeKind = finalField.kind.isEmpty() ? safeKind : finalField.kind;

    // 개발가이드 규칙 기반 기본값 생성
    static int identifierCounter = 1;
    qint64 currentEpoch = QDateTime::currentSecsSinceEpoch();

    auto removePrefixA = [](const QString& name) {
        if (name.startsWith("A_")) return name.mid(2);
        return name;
    };

    // 검증 로그: field 파라미터 값 출력
    qDebug() << "[getDefaultValue] field.name:" << finalField.name << ", field.kind:" << finalField.kind << ", field.nestedType:" << finalField.nestedType;

    std::function<QVariant(const QString&, const QString&, const QString&, const QStringList&, bool, bool, int, int)> getValueRec;
    getValueRec = [&](const QString& kind, const QString& nestedType, const QString& sequenceElementType, const QStringList& enumVals,
                      bool isSequenceOfString, bool isSequenceOfPrimitive, int maxLen, int upperBound) -> QVariant {
        qDebug() << "[getValueRec] kind:" << kind << ", nestedType:" << nestedType << ", sequenceElementType:" << sequenceElementType;
        // 식별자 타입
        if (nestedType.endsWith("T_IdentifierType")) {
            qDebug() << "[getValueRec] IdentifierType branch";
            QVariantMap obj;
            obj["A_resourceId"] = identifierCounter++;
            obj["A_instanceId"] = 1;
            return QVariant::fromValue(obj);
        }
        // 시간형
        if (nestedType.endsWith("T_DateTimeType")) {
            qDebug() << "[getValueRec] DateTimeType branch";
            QVariantMap obj;
            obj["A_second"] = currentEpoch;
            obj["A_nanoseconds"] = 0;
            return QVariant::fromValue(obj);
        }
        // 기간형
        if (nestedType.endsWith("T_DurationType")) {
            qDebug() << "[getValueRec] DurationType branch";
            QVariantMap obj;
            obj["A_seconds"] = 1;
            obj["A_nanoseconds"] = 0;
            return QVariant::fromValue(obj);
        }
        // 불리언
        if (kind == "bool") {
            qDebug() << "[getValueRec] bool branch";
            return false;
        }
        // 열거형
        if (kind == "enum" && !enumVals.isEmpty()) {
            qDebug() << "[getValueRec] enum branch";
            return enumVals.first();
        }
        // 수치
        if (kind.startsWith("int") || kind.startsWith("uint")) {
            qDebug() << "[getValueRec] int/uint branch";
            return 1;
        }
        if (kind.startsWith("float") || kind.startsWith("double")) {
            qDebug() << "[getValueRec] float/double branch";
            return 1.0;
        }
        // 문자열
        if (kind == "string") {
            qDebug() << "[getValueRec] string branch";
            // 기본값은 'A_' 접두사 제거한 필드명
            QString val = removePrefixA(safeName);
            if (maxLen > 0 && val.length() > maxLen) val = val.left(maxLen);
            return val;
        }
        // 시퀀스
        if (kind == "sequence") {
            qDebug() << "[getValueRec] sequence branch";
            QVariantList arr;
            if (isSequenceOfString) {
                QString val = removePrefixA(safeName);
                if (maxLen > 0 && val.length() > maxLen) val = val.left(maxLen);
                arr << val;
            } else if (isSequenceOfPrimitive) {
                QString elementType = sequenceElementType.isEmpty() ? "string" : sequenceElementType;
                if (elementType.contains("int")) arr << 1;
                else if (elementType.contains("float") || elementType.contains("double")) arr << 1.0;
                else if (elementType.contains("bool")) arr << false;
                else {
                    QString val = removePrefixA(safeName);
                    if (maxLen > 0 && val.length() > maxLen) val = val.left(maxLen);
                    arr << val;
                }
            } else if (!nestedType.isEmpty() && catalog_ && catalog_->hasType(nestedType)) {
                const XTypeSchema& nestedSchema = catalog_->getType(nestedType);
                if (!nestedSchema.fields.isEmpty()) {
                    QVariantMap obj;
                    for (const auto& nestedField : nestedSchema.fields) {
                        obj[nestedField.name] = getDefaultValue(nestedField);
                    }
                    arr << obj;
                }
            }
            return QVariant::fromValue(arr);
        }
        // struct 또는 미확정 타입: 내부 타입 재귀 분석
        // 분기 조건 보완: kind가 struct가 아니더라도 nestedType이 있고, 해당 타입이 struct라면 struct 분기 실행
        bool isStructType = (kind == "struct") || (!nestedType.isEmpty() && catalog_ && catalog_->hasType(nestedType) && catalog_->getType(nestedType).fields.size() > 0);
        if (isStructType && !nestedType.isEmpty() && catalog_ && catalog_->hasType(nestedType)) {
            qDebug() << "[getValueRec] struct branch";
            const XTypeSchema& nestedSchema = catalog_->getType(nestedType);
            if (!nestedSchema.fields.isEmpty()) {
                QVariantMap obj;
                for (const auto& nestedField : nestedSchema.fields) {
                    obj[nestedField.name] = getDefaultValue(nestedField);
                }
                return QVariant::fromValue(obj);
            }
        }
        // typedef/체인 해석: 타입 정보가 더 있으면 재귀적으로 따라감
        if (!nestedType.isEmpty() && catalog_ && catalog_->hasType(nestedType)) {
            qDebug() << "[getValueRec] typedef/chain branch";
            const XTypeSchema& schema = catalog_->getType(nestedType);
            if (!schema.fields.isEmpty()) {
                const XField& f = schema.fields.first();
                return getValueRec(f.kind, f.nestedType, f.sequenceElementType, f.enumVals,
                                   f.isSequenceOfString, f.isSequenceOfPrimitive, f.maxLen, f.upperBound);
            }
        }
        if (!sequenceElementType.isEmpty() && catalog_ && catalog_->hasType(sequenceElementType)) {
            qDebug() << "[getValueRec] sequenceElementType chain branch";
            const XTypeSchema& seqSchema = catalog_->getType(sequenceElementType);
            if (!seqSchema.fields.isEmpty()) {
                const XField& f = seqSchema.fields.first();
                return getValueRec(f.kind, f.nestedType, f.sequenceElementType, f.enumVals,
                                   f.isSequenceOfString, f.isSequenceOfPrimitive, f.maxLen, f.upperBound);
            }
        }
        // 기타 타입: 안전한 기본값
        qDebug() << "[getValueRec] fallback branch";
        QString val = removePrefixA(safeName);
        if (maxLen > 0 && val.length() > maxLen) val = val.left(maxLen);
        return val;
    };

    // 최초 필드 정보로 시작
    QVariant result = getValueRec(safeKind, field.nestedType, field.sequenceElementType, field.enumVals,
                      field.isSequenceOfString, field.isSequenceOfPrimitive, field.maxLen, field.upperBound);
    qDebug() << "[getDefaultValue] result for field" << field.name << ":" << result;
    return result;
}

QJsonObject GenericFormDialog::collect(QWidget* w, const XTypeSchema& schema) const {
    QJsonObject obj;
    
    for (const auto& field : schema.fields) {
        QString safeName = field.name.isEmpty() ? "field" : field.name;
        QString safeKind = field.kind.isEmpty() ? "string" : field.kind;
        
        // 새로운 UI 구조에서 위젯 찾기 (깊은 검색)
        QWidget* fieldWidget = findFieldWidget(w, safeName);
        if (!fieldWidget) {
            qWarning() << "Field widget not found:" << safeName;
            continue;
        }
        
        // 타입별 값 수집 및 JSON 변환
        if (safeKind == "bool") {
            if (auto* cb = qobject_cast<QCheckBox*>(fieldWidget)) {
                obj[safeName] = cb->isChecked();
            }
        } else if (safeKind == "enum") {
            if (auto* combo = qobject_cast<QComboBox*>(fieldWidget)) {
                obj[safeName] = combo->currentText();
            }
        } else if (safeKind == "string") {
            if (auto* le = qobject_cast<QLineEdit*>(fieldWidget)) {
                obj[safeName] = le->text();
            }
        } else if (safeKind.startsWith("int") || safeKind.startsWith("uint")) {
            if (auto* le = qobject_cast<QLineEdit*>(fieldWidget)) {
                obj[safeName] = le->text().toInt();
            }
        } else if (safeKind.startsWith("float") || safeKind.startsWith("double")) {
            if (auto* le = qobject_cast<QLineEdit*>(fieldWidget)) {
                obj[safeName] = le->text().toDouble();
            }
        } else if (safeKind.contains("time") || safeKind.contains("Time")) {
            if (auto* le = qobject_cast<QLineEdit*>(fieldWidget)) {
                obj[safeName] = le->text(); // ISO 형식 시간
            }
        } else if (safeKind == "sequence") {
            if (auto* le = qobject_cast<QLineEdit*>(fieldWidget)) {
                QString text = le->text().trimmed();
                obj[safeName] = parseSequenceValue(text, field);
            }
        } else if (safeKind == "struct" && !field.nestedType.isEmpty() && catalog_ && catalog_->hasType(field.nestedType)) {
            // 중첩 구조 처리 (재귀적 수집)
            const XTypeSchema& nestedSchema = catalog_->getType(field.nestedType);
            obj[safeName] = collect(fieldWidget, nestedSchema);
        } else {
            // 기타 타입 처리
            if (auto* le = qobject_cast<QLineEdit*>(fieldWidget)) {
                obj[safeName] = le->text();
            }
        }
    }
    
    return obj;
}

QJsonObject GenericFormDialog::toJson() const {
    // OK 버튼에서 미리 캐싱된 결과 반환
    return cachedResult_;
}

// 헬퍼 메서드: 깊은 위젯 검색
QWidget* GenericFormDialog::findFieldWidget(QWidget* parent, const QString& objectName) const {
    if (!parent) return nullptr;
    
    // 직접 검색
    QWidget* found = parent->findChild<QWidget*>(objectName, Qt::FindChildrenRecursively);
    if (found) return found;
    
    // 중첩 구조에서 더 깊이 검색
    QList<QWidget*> allWidgets = parent->findChildren<QWidget*>(QString(), Qt::FindChildrenRecursively);
    for (QWidget* widget : allWidgets) {
        if (widget->objectName() == objectName) {
            return widget;
        }
    }
    
    return nullptr;
}

// 헬퍼 메서드: 배열 값 파싱
QJsonValue GenericFormDialog::parseSequenceValue(const QString& text, const XField& field) const {
    if (text.isEmpty()) {
        return QJsonArray();
    }
    
    if (field.isSequenceOfString) {
        return text;  // 문자열 시퀀스는 단일 문자열로 처리
    } else if (field.isSequenceOfPrimitive) {
        QJsonArray arr;
        QStringList items = text.split(",", Qt::SkipEmptyParts);
        for (const QString& item : items) {
            QString trimmed = item.trimmed();
            // 타입별 변환
            if (field.sequenceElementType.contains("int")) {
                arr.append(trimmed.toInt());
            } else if (field.sequenceElementType.contains("float") || field.sequenceElementType.contains("double")) {
                arr.append(trimmed.toDouble());
            } else if (field.sequenceElementType.contains("bool")) {
                arr.append(trimmed.toLower() == "true");
            } else {
                arr.append(trimmed);
            }
        }
        return arr;
    } else {
        // JSON 배열로 파싱 시도
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && doc.isArray()) {
            return doc.array();
        }
        // 파싱 실패 시 빈 배열 반환
        return QJsonArray();
    }
}

bool GenericFormDialog::getFormResult(XmlTypeCatalog* catalog, const QString& typeName, QWidget* parent, QJsonObject& outResult) {
    GenericFormDialog dialog(catalog, typeName, parent);
    bool accepted = (dialog.exec() == QDialog::Accepted);
    if (accepted) {
        outResult = dialog.toJson();
    } else {
        outResult = QJsonObject{};
    }
    return accepted;
}
