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


#ifndef OBS_GOOGLE_LIVE_CAPTIONS_CAPTIONSETTINGSWIDGET_H
#define OBS_GOOGLE_LIVE_CAPTIONS_CAPTIONSETTINGSWIDGET_H


#include <QWidget>
#include <CaptionStream.h>
#include <src/ui/ui_CaptionSettingsWidget.h>

#include "../AudioCaptureSession.h"
#include "../log.c"
#include "../SourceCaptioner.h"
#include "../CaptionPluginSettings.h"

class CaptionSettingsWidget : public QWidget, Ui_CaptionSettingsWidget {
Q_OBJECT

    CaptionPluginSettings current_settings;
    string scene_collection_name;

    void accept_current_settings();

private slots:

    void on_previewPushButton_clicked();

    void apply_ui_scene_collection_settings();

    void update_scene_collection_ui(const string &use_scene_collection_name);

signals:

    void settings_accepted(CaptionPluginSettings new_settings);

    void preview_requested();

public:
    CaptionSettingsWidget(const CaptionPluginSettings &latest_settings);

    void set_settings(const CaptionPluginSettings &new_settings);

    void updateUi();
};


#endif //OBS_GOOGLE_LIVE_CAPTIONS_CAPTIONSETTINGSWIDGET_H
