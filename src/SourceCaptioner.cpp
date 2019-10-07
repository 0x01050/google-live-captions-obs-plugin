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

#include <memory>


#include "SourceCaptioner.h"
#include "log.c"


SourceCaptioner::SourceCaptioner(const SourceCaptionerSettings &settings, bool start) :
        QObject(),
        settings(settings),
        last_caption_at(std::chrono::steady_clock::now()),
        last_caption_cleared(true) {

    QObject::connect(&timer, &QTimer::timeout, this, &SourceCaptioner::clear_output_timer_cb);

    QObject::connect(this, &SourceCaptioner::received_caption_result,
                     this, &SourceCaptioner::process_caption_result, Qt::QueuedConnection);

    timer.start(1000);

    info_log("SourceCaptioner, source '%s'", settings.caption_source_settings.caption_source_name.c_str());
    if (start)
        start_caption_stream(settings);
}


void SourceCaptioner::stop_caption_stream(bool send_signal) {
    {
        std::lock_guard<recursive_mutex> lock(settings_change_mutex);
        audio_capture_session = nullptr;
        caption_result_handler = nullptr;
        continuous_captions = nullptr;
    }
    if (send_signal)
        not_not_captioning_status();
}

bool SourceCaptioner::start_caption_stream(const SourceCaptionerSettings &new_settings) {
    info_log("start_caption_stream");
    {
        std::lock_guard<recursive_mutex> lock(settings_change_mutex);

        audio_capture_session = nullptr;
        caption_result_handler = nullptr;

        bool caption_settings_equal = settings.stream_settings == new_settings.stream_settings;
        settings = new_settings;

        if (new_settings.caption_source_settings.caption_source_name.empty()) {
            warn_log("SourceCaptioner start_caption_stream, empty source given.");
            stop_caption_stream();
            return false;
        }

        OBSSource caption_source = obs_get_source_by_name(new_settings.caption_source_settings.caption_source_name.c_str());
        if (!caption_source) {
            warn_log("SourceCaptioner start_caption_stream, no caption source with name: '%s'",
                     new_settings.caption_source_settings.caption_source_name.c_str());
            stop_caption_stream();
            return false;
        }

        OBSSource mute_source;
        if (new_settings.caption_source_settings.mute_when == CAPTION_SOURCE_MUTE_TYPE_USE_OTHER_MUTE_SOURCE) {
            mute_source = obs_get_source_by_name(new_settings.caption_source_settings.mute_source_name.c_str());

            if (!mute_source) {
                warn_log("SourceCaptioner start_caption_stream, no mute source with name: '%s'",
                         new_settings.caption_source_settings.mute_source_name.c_str());
                stop_caption_stream();
                return false;
            }
        }

        debug_log("caption_settings_equal: %d, %d", caption_settings_equal, continuous_captions != nullptr);
        if (!continuous_captions || !caption_settings_equal) {
            try {
                auto caption_cb = std::bind(&SourceCaptioner::on_caption_text_callback, this, std::placeholders::_1, std::placeholders::_2);
                continuous_captions = std::make_unique<ContinuousCaptions>(new_settings.stream_settings);
                continuous_captions->on_caption_cb_handle.set(caption_cb, true);
            }
            catch (...) {
                warn_log("couldn't create ContinuousCaptions");
                stop_caption_stream();
                return false;
            }
        }
        caption_result_handler = std::make_unique<CaptionResultHandler>(new_settings.format_settings);

        try {
            resample_info resample_to = {16000, AUDIO_FORMAT_16BIT, SPEAKERS_MONO};
            audio_chunk_data_cb audio_cb = std::bind(&SourceCaptioner::on_audio_data_callback, this,
                                                     std::placeholders::_1, std::placeholders::_2);

            auto audio_status_cb = std::bind(&SourceCaptioner::on_audio_capture_status_change_callback, this, std::placeholders::_1);

            audio_capture_session = std::make_unique<AudioCaptureSession>(caption_source, mute_source, audio_cb, audio_status_cb,
                                                                          resample_to,
//                                                                      MUTED_SOURCE_DISCARD_WHEN_MUTED);
                                                                          MUTED_SOURCE_REPLACE_WITH_ZERO);
        }
        catch (std::string err) {
            warn_log("couldn't create AudioCaptureSession, %s", err.c_str());
            stop_caption_stream();
            return false;
        }
        catch (...) {
            warn_log("couldn't create AudioCaptureSession");
            stop_caption_stream();
            return false;
        }

        info_log("starting captioning source '%s'", new_settings.caption_source_settings.caption_source_name.c_str());
        return true;
    }
}

void SourceCaptioner::on_audio_capture_status_change_callback(const audio_source_capture_status status) {
    info_log("capture status change %d ", status);
    emit audio_capture_status_changed(status);
}

void SourceCaptioner::on_audio_data_callback(const uint8_t *data, const size_t size) {
//    info_log("audio data");
    if (continuous_captions) {
        continuous_captions->queue_audio_data((char *) data, size);
    }
    audio_chunk_count++;

}

void SourceCaptioner::clear_output_timer_cb() {
//    info_log("clear timer checkkkkkkkkkkkkkkk");

    bool to_stream, to_recording;
    {
        std::lock_guard<recursive_mutex> lock(settings_change_mutex);
        if (!this->settings.format_settings.caption_timeout_enabled || this->last_caption_cleared)
            return;

        double secs_since_last_caption = std::chrono::duration_cast<std::chrono::duration<double >>(
                std::chrono::steady_clock::now() - this->last_caption_at).count();

        if (secs_since_last_caption <= this->settings.format_settings.caption_timeout_seconds)
            return;

        info_log("last caption line was sent %f secs ago, > %f, clearing",
                 secs_since_last_caption, this->settings.format_settings.caption_timeout_seconds);

        this->last_caption_cleared = true;
        to_stream = settings.streaming_output_enabled;
        to_recording = settings.recording_output_enabled;
    }

    auto clearance = CaptionOutput(std::make_shared<OutputCaptionResult>(CaptionResult(0, false, 0, "", "")), false, true);
    output_caption_text(clearance, to_stream, to_recording, true);
    emit caption_result_received(nullptr, false, true, "");
}


void SourceCaptioner::store_result(shared_ptr<OutputCaptionResult> output_result, bool interrupted) {
    if (!output_result)
        return;

    if (interrupted) {
        if (held_nonfinal_caption_result) {
            results_history.push_back(held_nonfinal_caption_result);
            debug_log("interrupt, saving latest nonfinal result to history, %s",
                      held_nonfinal_caption_result->clean_caption_text.c_str());
        }
    }

    held_nonfinal_caption_result = nullptr;
    if (output_result->caption_result.final) {
        results_history.push_back(output_result);
        debug_log("final, adding to history: %s", output_result->clean_caption_text.c_str());
    } else {
        held_nonfinal_caption_result = output_result;
    }

}


void SourceCaptioner::prepare_recent(string &recent_captions_output) {
    for (auto i = results_history.rbegin(); i != results_history.rend(); ++i) {
        if (!(*i))
            break;

        if (!(*i)->caption_result.final)
            break;

        if (recent_captions_output.size() + (*i)->clean_caption_text.size() >= MAX_HISTORY_VIEW_LENGTH)
            break;

        if ((*i)->clean_caption_text.empty())
            continue;

        if (recent_captions_output.empty()) {
            recent_captions_output.insert(0, 1, '.');
        } else {
            recent_captions_output.insert(0, ". ");
        }

        recent_captions_output.insert(0, (*i)->clean_caption_text);
    }

    if (held_nonfinal_caption_result) {
        if (!recent_captions_output.empty())
            recent_captions_output.push_back(' ');
        recent_captions_output.append("    >> ");
        recent_captions_output.append(held_nonfinal_caption_result->clean_caption_text);
    }
}

void SourceCaptioner::on_caption_text_callback(const CaptionResult &caption_result, bool interrupted) {
    // emit qt signal to avoid possible thread deadlock
    // this callback comes from the captioner thread, result processing needs settings_change_mutex, so does clearing captioner,
    // but that waits for the captioner callback to finish which might be waiting on the lock otherwise.

    emit received_caption_result(caption_result, interrupted);
}

void SourceCaptioner::process_caption_result(const CaptionResult caption_result, bool interrupted) {
    shared_ptr<OutputCaptionResult> output_result;
    string recent_caption_text;
    bool to_stream, to_recording;
    {
        std::lock_guard<recursive_mutex> lock(settings_change_mutex);

        if (!caption_result_handler) {
            warn_log("no caption_result_handler, shouldn't happen, there should be no AudioCaptureSession running");
            return;
        }

        output_result = caption_result_handler->prepare_caption_output(caption_result,
                                                                       true,
                                                                       settings.format_settings.caption_insert_newlines,
                                                                       results_history);
        if (!output_result)
            return;

        store_result(output_result, interrupted);

//        info_log("got caption '%s'", output_result->clean_caption_text.c_str());
//        info_log("output line '%s'", output_caption_line.c_str());

        prepare_recent(recent_caption_text);

        to_stream = settings.streaming_output_enabled;
        to_recording = settings.recording_output_enabled;
    }

    this->output_caption_text(CaptionOutput(output_result, interrupted, false), to_stream, to_recording, false);
    emit caption_result_received(output_result, interrupted, false, recent_caption_text);
}

void SourceCaptioner::output_caption_text(
        const CaptionOutput &output,
        bool to_stream,
        bool to_recoding,
        bool is_clearance) {

    bool sent_stream = false;
    if (to_stream) {
        sent_stream = streaming_output.enqueue(output);
    }

    bool sent_recording = false;
    if (to_recoding) {
        sent_recording = recording_output.enqueue(output);
    }

//    debug_log("queuing caption line , stream: %d, recording: %d, '%s'",
//              sent_stream, sent_recording, output.line.c_str());

    if (!is_clearance)
        caption_was_output();
}


void SourceCaptioner::not_not_captioning_status() {
    emit audio_capture_status_changed(-1);
}

void SourceCaptioner::caption_was_output() {
    this->last_caption_at = std::chrono::steady_clock::now();
    this->last_caption_cleared = false;
}


void SourceCaptioner::stream_started_event() {
    auto control = std::make_shared<CaptionOutputControl>();
    streaming_output.set_control(control);
    std::thread th(caption_output_writer_loop, control, true);
    th.detach();
}

void SourceCaptioner::stream_stopped_event() {
    streaming_output.clear();
}

void SourceCaptioner::recording_started_event() {
    auto control = std::make_shared<CaptionOutputControl>();
    recording_output.set_control(control);
    std::thread th(caption_output_writer_loop, control, false);
    th.detach();
}

void SourceCaptioner::recording_stopped_event() {
    recording_output.clear();
}


SourceCaptioner::~SourceCaptioner() {
    stream_stopped_event();
    recording_stopped_event();
    stop_caption_stream(false);
}
