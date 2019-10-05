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


#ifndef OBS_GOOGLE_CAPTION_PLUGIN_CAPTIONSETTINGSWIDGET_H
#define OBS_GOOGLE_CAPTION_PLUGIN_CAPTIONSETTINGSWIDGET_H


#include <QWidget>
#include <CaptionStream.h>
#include <src/ui/ui_CaptionSettingsWidget_out.h>

#include "../AudioCaptureSession.h"
#include "../log.c"
#include "../SourceCaptioner.h"

class CaptionSettingsWidget : public QWidget, Ui_CaptionSettingsWidget {
Q_OBJECT

    CaptionerSettings latest_settings;

    void accept_current_settings();


private slots:

    void caption_when_index_change(int index);


signals:

    void settings_accepted(CaptionerSettings new_settings);


public:
    CaptionSettingsWidget(CaptionerSettings latest_settings);

    void set_settings(CaptionerSettings new_settings);

    void updateUi();

    void update_other_source_visibility(CaptionSourceMuteType mute_state);
};


#endif //OBS_GOOGLE_CAPTION_PLUGIN_CAPTIONSETTINGSWIDGET_H
