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
#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-properties.h>

#include "SourceCaptioner.h"

#include <QAction>
#include <QMessageBox>
#include <QMainWindow>

#include <media-io/audio-resampler.h>
#include <fstream>
#include <thread>

#include "ui/MainCaptionWidget.h"
#include "caption_stream_helper.cpp"
#include "CaptionPluginManager.h"
#include "ui/CaptionDock.h"

#include "log.c"

using namespace std;

MainCaptionWidget *main_caption_widget = nullptr;
CaptionPluginManager *plugin_manager = nullptr;

bool frontend_loading_finished = false;
bool ui_setup_done = false;

OBS_DECLARE_MODULE()


void finished_loading_event();

void stream_started_event();

void stream_stopped_event();

void recording_started_event();

void recording_stopped_event();

void setup_UI();

void closed_caption_tool_menu_clicked();

void obs_frontent_exiting();

void obs_frontent_scene_collection_changed();

static void obs_event(enum obs_frontend_event event, void *) {
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        finished_loading_event();
    } else if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
        stream_started_event();
    } else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
        stream_stopped_event();
    } else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
        recording_started_event();
    } else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPED) {
        recording_stopped_event();
    } else if (event == OBS_FRONTEND_EVENT_EXIT) {
        obs_frontent_exiting();
    } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
        obs_frontent_scene_collection_changed();
    } else if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED) {
        printf("studio mode!!!!!!!!!!!!!!!!!!!!\n");
    }
}


void closed_caption_tool_menu_clicked() {
    if (main_caption_widget) {
        main_caption_widget->show_settings_dialog();
    }
}

void setup_UI() {
    if (ui_setup_done)
        return;

    QAction *action = (QAction *) obs_frontend_add_tools_menu_qaction("Google Live Captions");
    action->connect(action, &QAction::triggered, &closed_caption_tool_menu_clicked);

    ui_setup_done = true;
}

void finished_loading_event() {
    frontend_loading_finished = true;
    if (main_caption_widget) {
        main_caption_widget->external_state_changed();
    }
}

void stream_started_event() {
    if (main_caption_widget)
        main_caption_widget->stream_started_event();
}

void stream_stopped_event() {
    if (main_caption_widget)
        main_caption_widget->stream_stopped_event();
}

void recording_started_event() {
    if (main_caption_widget)
        main_caption_widget->recording_started_event();
}

void recording_stopped_event() {
    if (main_caption_widget)
        main_caption_widget->recording_stopped_event();
}

void obs_frontent_scene_collection_changed() {
    if (main_caption_widget) {
        main_caption_widget->scene_collection_changed();
    }

}

void obs_frontent_exiting() {
    if (main_caption_widget) {
        delete main_caption_widget;
        main_caption_widget = nullptr;
    }

    if (plugin_manager) {
        save_CaptionPluginSettings_to_config(plugin_manager->plugin_settings);

        delete plugin_manager;
        plugin_manager = nullptr;
    }
}

static void save_or_load_event_callback_config(obs_data_t *_, bool saving, void *) {
    int tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
    info_log("google_s2t_caption_plugin save_or_load_event_callback %d, %d", saving, tid);

    if (saving) {
        if (plugin_manager) {
            save_CaptionPluginSettings_to_config(plugin_manager->plugin_settings);
        }
    } else {
        if (!plugin_manager && !main_caption_widget) {
            CaptionPluginSettings settings = load_CaptionPluginSettings_from_config();
            plugin_manager = new CaptionPluginManager(settings);
            main_caption_widget = new MainCaptionWidget(*plugin_manager);
            setup_UI();
        }
    }
}


static void save_or_load_event_callback(obs_data_t *save_data, bool saving, void *) {
    int tid = std::hash<std::thread::id>{}(std::this_thread::get_id());

    if (saving && plugin_manager) {
        save_CaptionPluginSettings(save_data, plugin_manager->plugin_settings);
    }

    if (!saving) {
        auto loaded_settings = load_CaptionPluginSettings(save_data);
        if (plugin_manager && main_caption_widget) {
            plugin_manager->update_settings(loaded_settings);
        } else if (plugin_manager || main_caption_widget) {
            error_log("only one of plugin_manager and main_caption_widget, wtf, %d %d",
                      plugin_manager != nullptr, main_caption_widget != nullptr);
        } else {
            plugin_manager = new CaptionPluginManager(loaded_settings);
            main_caption_widget = new MainCaptionWidget(*plugin_manager);
            setup_UI();
        }
    }

}


bool obs_module_load(void) {
    qRegisterMetaType<std::string>();
    qRegisterMetaType<shared_ptr<OutputCaptionResult>>();
    qRegisterMetaType<CaptionResult>();
    qRegisterMetaType<std::shared_ptr<SourceCaptionerStatus>>();

    obs_frontend_add_event_callback(obs_event, nullptr);
    obs_frontend_add_save_callback(save_or_load_event_callback, nullptr);
    return true;
}

void obs_module_post_load(void) {
}

void obs_module_unload(void) {
}