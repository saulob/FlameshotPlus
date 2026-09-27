// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "visualseditor.h"
#include "config/buttonlistview.h"
#include "config/colorpickereditor.h"
#include "config/extendedslider.h"
#include "config/uicoloreditor.h"
#include "utils/confighandler.h"

#include <QCollator>
#include <QDirIterator>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <algorithm>

VisualsEditor::VisualsEditor(QWidget* parent)
  : QWidget(parent)
{
    m_layout = new QVBoxLayout();
    setLayout(m_layout);
    initWidgets();
}

void VisualsEditor::updateComponents()
{
    m_buttonList->updateComponents();
    m_colorEditor->updateComponents();
    int opacity = ConfigHandler().contrastOpacity();
    m_opacitySlider->setMapedValue(0, opacity, 255);
}

void VisualsEditor::initOpacitySlider()
{
    m_opacitySlider = new ExtendedSlider();
    m_opacitySlider->setFocusPolicy(Qt::NoFocus);
    m_opacitySlider->setOrientation(Qt::Horizontal);
    m_opacitySlider->setRange(0, 100);
    auto* localLayout = new QHBoxLayout();
    localLayout->addWidget(new QLabel(QStringLiteral("0%")));
    localLayout->addWidget(m_opacitySlider);
    localLayout->addWidget(new QLabel(QStringLiteral("100%")));

    auto* label = new QLabel();
    QString labelMsg = tr("Opacity of area outside selection:") + " %1%";
    ExtendedSlider* opacitySlider = m_opacitySlider;
    connect(m_opacitySlider,
            &ExtendedSlider::valueChanged,
            this,
            [labelMsg, label, opacitySlider](int val) {
                label->setText(labelMsg.arg(val));
                ConfigHandler().setContrastOpacity(
                  opacitySlider->mappedValue(0, 255));
            });
    m_layout->addWidget(label);
    m_layout->addLayout(localLayout);

    int opacity = ConfigHandler().contrastOpacity();
    m_opacitySlider->setMapedValue(0, opacity, 255);
}

void VisualsEditor::initWidgets()
{
    initTranslations();

    m_tabWidget = new QTabWidget();
    m_layout->addWidget(m_tabWidget);

    m_colorEditor = new UIcolorEditor();
    m_colorEditorTab = new QWidget();
    auto* colorEditorLayout = new QVBoxLayout(m_colorEditorTab);
    m_colorEditorTab->setLayout(colorEditorLayout);
    colorEditorLayout->addWidget(m_colorEditor);
    m_tabWidget->addTab(m_colorEditorTab, tr("UI Color Editor"));

    m_colorpickerEditor = new ColorPickerEditor();
    m_colorpickerEditorTab = new QWidget();
    auto* colorpickerEditorLayout = new QVBoxLayout(m_colorpickerEditorTab);
    colorpickerEditorLayout->addWidget(m_colorpickerEditor);
    m_tabWidget->addTab(m_colorpickerEditorTab, tr("Colorpicker Editor"));

    initOpacitySlider();

    auto* boxButtons = new QGroupBox();
    boxButtons->setTitle(tr("Button Selection"));
    auto* listLayout = new QVBoxLayout(boxButtons);
    m_buttonList = new ButtonListView();
    m_layout->addWidget(boxButtons);
    listLayout->addWidget(m_buttonList);

    auto* setAllButtons = new QPushButton(tr("Select All"));
    connect(setAllButtons,
            &QPushButton::clicked,
            m_buttonList,
            &ButtonListView::selectAll);
    listLayout->addWidget(setAllButtons);
}

void VisualsEditor::initTranslations()
{
    auto* localLayout = new QHBoxLayout();
    localLayout->addWidget(new QLabel(tr("UI language")));
    m_selectTranslation = new QComboBox(this);

    QStringList translations;
    QString tmpFilename;
    for (const QString& path : PathInfo::translationsPaths()) {
        QDirIterator it(path,
                        QStringList() << QStringLiteral("*.qm"),
                        QDir::NoDotAndDotDot | QDir::Files);
        while (it.hasNext()) {
            it.next();
            tmpFilename = it.fileName();

            if (tmpFilename.startsWith(
                  QStringLiteral("Internationalization_"))) {
                tmpFilename =
                  tmpFilename.remove(QStringLiteral("Internationalization_"))
                    .remove(QStringLiteral(".qm"));
                if (!translations.contains(tmpFilename)) {
                    translations << tmpFilename;
                }
            }
        }
    }
    // Pairs of display name and locale code, sorted by the display name
    QList<QPair<QString, QString>> languages;
    for (const QString& code : translations) {
        // Display the locale's native name; the code stays as item data
        const QLocale locale(code);
        const QString languageCode = code.section(QLatin1Char('_'), 0, 0);
        const QString territoryCode = code.section(QLatin1Char('_'), 1, 1);
        // Prefer the endonym shared by most locales of this language, so a
        // bare code like "en" is not shown as a regional variant
        QHash<QString, int> endonyms;
        for (const QLocale& l : QLocale::matchingLocales(
               locale.language(), locale.script(), QLocale::AnyTerritory)) {
            ++endonyms[l.nativeLanguageName()];
        }
        QString name = locale.nativeLanguageName();
        for (auto it = endonyms.cbegin(); it != endonyms.cend(); ++it) {
            if (it.value() > endonyms.value(name)) {
                name = it.key();
            }
        }
        if (name.isEmpty() ||
            locale.language() != QLocale::codeToLanguage(languageCode)) {
            name = code;
        } else {
            // Capitalize only the first character (may be a surrogate pair)
            const int first = name.at(0).isHighSurrogate() ? 2 : 1;
            name = locale.toUpper(name.left(first)) + name.mid(first);
            if (!territoryCode.isEmpty() &&
                locale.territory() == QLocale::codeToTerritory(territoryCode) &&
                !locale.nativeTerritoryName().isEmpty()) {
                name +=
                  QStringLiteral(" (%1)").arg(locale.nativeTerritoryName());
            }
        }
        languages.append({ name, code });
    }
    QCollator collator;
    std::sort(languages.begin(),
              languages.end(),
              [&collator](const auto& a, const auto& b) {
                  return collator.compare(a.first, b.first) < 0;
              });
    m_selectTranslation->addItem(tr("Automatic (System language)"),
                                 QStringLiteral("auto"));
    for (const auto& item : languages) {
        m_selectTranslation->addItem(item.first, item.second);
    }

    QString language = ConfigHandler().value("uiLanguage").toString();
    m_selectTranslation->setCurrentIndex(
      m_selectTranslation->findData(language));

    connect(
      m_selectTranslation, &QComboBox::activated, this, [this](int index) {
          const QString code = m_selectTranslation->itemData(index).toString();
          if (code == ConfigHandler().uiLanguage()) {
              return;
          }
          ConfigHandler().setUiLanguage(code);
          // TODO: Retranslate UI without restart
          QMessageBox::information(
            this,
            tr("Configuration"),
            tr("Flameshot must be restarted to apply these changes!"));
      });

    localLayout->addWidget(m_selectTranslation);
    localLayout->addStretch();
    m_layout->addLayout(localLayout);
}
