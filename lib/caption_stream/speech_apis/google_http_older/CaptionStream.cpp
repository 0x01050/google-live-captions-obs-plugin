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

#include <utility>


#include <string>
#include <sstream>
#include "CaptionStream.h"
#include "utils.h"
#include "log.h"

#define BUFFER_SIZE 4096

#include <json11.cpp>
using namespace json11;

static CaptionResult *parse_caption_obj(const string &msg_obj) {
    bool is_final = false;
    double highest_stability = 0.0;
    string caption_text;
    int result_index;

    string err;
    Json json = Json::parse(msg_obj, err);
    if (!err.empty()) {
        throw err;
    }
    Json result_val = json["result"];
    if (!result_val.is_array())
        throw string("no result");

    if (!result_val.array_items().size())
        throw string("empty");

    Json result_index_val = json["result_index"];
    if (!result_index_val.is_number())
        throw string("no result index");

    result_index = result_index_val.int_value();

    for (auto &result_obj : result_val.array_items()) {
        string text = result_obj["alternative"][0]["transcript"].string_value();
        if (text.empty())
            continue;

        if (result_obj["final"].bool_value()) {
            is_final = true;
            caption_text = text;
            break;
        }

        Json stability_val = result_obj["stability"];
        if (!stability_val.is_number())
            continue;

        if (stability_val.number_value() >= highest_stability) {
            caption_text = text;
            highest_stability = stability_val.number_value();
        }
    }

    if (caption_text.empty())
        throw string("no caption text");

    return new CaptionResult(result_index, is_final, highest_stability, caption_text, msg_obj);
}


int read_until_contains(TcpConnection *connection, string &buffer, const char *required_contents) {
    int newline_pos = buffer.find(required_contents);
    if (newline_pos != string::npos)
        return newline_pos;

    char chunk[BUFFER_SIZE];
    do {
        int read_cnt = connection->receive_at_most(chunk, BUFFER_SIZE);
        if (read_cnt == -1)
            return -1;

        buffer.append(chunk, read_cnt);
    } while ((newline_pos = buffer.find(required_contents)) == string::npos);
    return newline_pos;
}


CaptionStream::CaptionStream(
        CaptionStreamSettings settings
) : upstream(TcpConnection(GOOGLE, PORTUP)),

    settings(settings),

    session_pair(random_string(15)),
    upstream_thread(nullptr) {
}


bool CaptionStream::start(std::shared_ptr<CaptionStream> self) {
    if (self.get() != this)
        return false;

    if (started)
        return false;

    started = true;
    upstream_thread = new thread(&CaptionStream::upstream_run, this, self);
    return true;
}


void CaptionStream::upstream_run(std::shared_ptr<CaptionStream> self) {
    _upstream_run(self);
    stop();
}

void CaptionStream::_upstream_run(std::shared_ptr<CaptionStream> self) {
    try {
        upstream.connect(settings.connect_timeout_ms);

    } catch (ConnectError &ex) {
        debug_log("upstream connect error, %s", ex.what());
        return;
    }

    upstream.set_timeout(settings.send_timeout_ms);

    const string crlf("\r\n");
    uint chunk_count = 0;
    while (true) {
        if (is_stopped())
            return;

        string *audio_chunk = dequeue_audio_data(settings.send_timeout_ms * 1000);
        if (audio_chunk == nullptr) {
            error_log("couldn't deque audio chunk in time");
            return;
        }

        if (!audio_chunk->empty()) {

            std::string request(*audio_chunk);

            if (!upstream.send_all(request.c_str(), request.size())) {
                error_log("couldn't send audio chunk");
                delete audio_chunk;
                return;
            }

            if (chunk_count % 1 == 0)
                debug_log("sent audio chunk %d, %lu bytes", chunk_count, audio_chunk->size());

        } else {
            error_log("got 0 size audio chunk. ignoring");
        }

        delete audio_chunk;
        chunk_count++;
    }
}

bool CaptionStream::is_stopped() {
    return stopped;
}

bool CaptionStream::queue_audio_data(const char *audio_data, const uint data_size) {
    if (is_stopped())
        return false;

    string *str = new string(audio_data, data_size);

    const int over_limit_cnt = audio_queue.size_approx() - settings.max_queue_depth;
    if (settings.max_queue_depth) {
        int cleared_cnt = 0;
        while (audio_queue.size_approx() > settings.max_queue_depth) {
            string *item;
            if (audio_queue.try_dequeue(item)) {
                delete item;
                cleared_cnt++;
            }
        }
        if (cleared_cnt)
            debug_log("queue too big, dropped %d old items from queue %s", cleared_cnt, session_pair.c_str());
    }
    audio_queue.enqueue(str);
    return true;
}

string *CaptionStream::dequeue_audio_data(const std::int64_t timeout_us) {
    string *ret;
    if (audio_queue.wait_dequeue_timed(ret, timeout_us))
        return ret;

    return nullptr;
}


void CaptionStream::stop() {
    on_caption_cb_handle.clear();
    stopped = true;

    upstream.close();

    string *to_unblock_uploader = new string();
    audio_queue.enqueue(to_unblock_uploader);
}


CaptionStream::~CaptionStream() {
    if (!is_stopped())
        stop();

    int cleared = 0;
    {
        string *item;
        while (audio_queue.try_dequeue(item)) {
            delete item;
            cleared++;
        }
    }

    if (upstream_thread) {
        upstream_thread->detach();
        delete upstream_thread;
        upstream_thread = nullptr;
    }

}

bool CaptionStream::is_started() {
    return started;
}


