/******************************************************************************
Copyright (C) 2019 by <rat.with.a.compiler@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <obs.hpp>
#include "CaptionSettingsWidget.h"
#include "../log.c"
#include <utils.h>
#include <QLineEdit>
#include <QFileDialog>
#include "../storage_utils.h"
#include "../caption_stream_helper.cpp"

static void setup_combobox_texts(QComboBox &comboBox,
                                 const vector<string> &items
) {
    while (comboBox.count())
        comboBox.removeItem(0);

    for (auto &a_item : items) {
        comboBox.addItem(QString::fromStdString(a_item));
    }
}

static int update_combobox_with_current_scene_collections(QComboBox &comboBox) {
    while (comboBox.count())
        comboBox.removeItem(0);

    char **names = obs_frontend_get_scene_collections();
    char **name = names;

    int cnt = 0;
    while (name && *name) {
        comboBox.addItem(QString(*name));
        name++;
        cnt++;
    }
    bfree(names);

    return cnt;
}

static int combobox_set_data_str(QComboBox &combo_box, const char *data, int default_index) {
    int index = combo_box.findData(data);
    if (index == -1)
        index = default_index;

    combo_box.setCurrentIndex(index);
    return index;

}

static int combobox_set_data_int(QComboBox &combo_box, const int data, int default_index) {
    int index = combo_box.findData(data);
    if (index == -1)
        index = default_index;

    combo_box.setCurrentIndex(index);
    return index;
}

CaptionSettingsWidget::CaptionSettingsWidget(const CaptionPluginSettings &latest_settings)
        : QWidget(),
          Ui_CaptionSettingsWidget(),
          current_settings(latest_settings) {
    setupUi(this);
    this->updateUi();
    QObject::connect(this->cancelPushButton, &QPushButton::clicked, this, &CaptionSettingsWidget::hide);
    QObject::connect(this->savePushButton, &QPushButton::clicked, this, &CaptionSettingsWidget::accept_current_settings);
}

void CaptionSettingsWidget::on_previewPushButton_clicked() {
    emit preview_requested();
}

void CaptionSettingsWidget::apply_ui_scene_collection_settings() {
    string current_scene_collection_name = scene_collection_name;
    SceneCollectionSettings scene_col_settings;
    CaptionSourceSettings &cap_source_settings = scene_col_settings.caption_source_settings;
    cap_source_settings.caption_source_name = sourcesComboBox->currentText().toStdString();
    current_settings.source_cap_settings.update_setting(current_scene_collection_name, scene_col_settings);
}

void CaptionSettingsWidget::update_scene_collection_ui(const string &use_scene_collection_name) {
    SourceCaptionerSettings &source_settings = current_settings.source_cap_settings;
    SceneCollectionSettings use_settings = default_SceneCollectionSettings();
    {
        use_settings = source_settings.get_scene_collection_settings(use_scene_collection_name);
    }

    // audio input sources
    auto audio_sources = get_audio_sources();
    audio_sources.insert(audio_sources.begin(), "");

    const QSignalBlocker blocker1(sourcesComboBox);
    setup_combobox_texts(*sourcesComboBox, audio_sources);

    sourcesComboBox->setCurrentText(QString::fromStdString(use_settings.caption_source_settings.caption_source_name));

}

void CaptionSettingsWidget::accept_current_settings() {
    SourceCaptionerSettings &source_settings = current_settings.source_cap_settings;
    current_settings.enabled = enabledCheckBox->isChecked();
    apply_ui_scene_collection_settings();
    emit settings_accepted(current_settings);
}


void CaptionSettingsWidget::updateUi() {
    update_scene_collection_ui(scene_collection_name);
    enabledCheckBox->setChecked(current_settings.enabled);
    previewPushButton->hide();
}

void CaptionSettingsWidget::set_settings(const CaptionPluginSettings &new_settings) {
    current_settings = new_settings;
    scene_collection_name = current_scene_collection_name();
    updateUi();
}

