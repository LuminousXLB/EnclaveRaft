/**
*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  The ASF licenses
* this file to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef _LOG_ENTRY_HXX_
#define _LOG_ENTRY_HXX_

#include "log_val_type.hxx"

namespace cornerstone {
    class log_entry {
    public:
        log_entry(ulong term, bufptr &&buff, log_val_type value_type = log_val_type::app_log)
                : term_(term), value_type_(value_type), buff_(std::move(buff)) {
        }

    __nocopy__(log_entry)

    public:
        ulong get_term() const {
            return term_;
        }

        void set_term(ulong term) {
            term_ = term;
        }

        log_val_type get_val_type() const {
            return value_type_;
        }

        buffer &get_buf() const {
            // we accept nil buffer, but in that case, the get_buf() shouldn't be called, throw runtime exception instead of having segment fault (AV on Windows)
            if (!buff_) {
                throw std::runtime_error("get_buf cannot be called for a log_entry with nil buffer");
            }

            return *buff_;
        }

        bufptr serialize() {
            buff_->pos(0);
            bufptr buf = buffer::alloc(sizeof(ulong) + sizeof(char) + buff_->size());
            buf->put(term_);
            buf->put((static_cast<byte>(value_type_)));
            buf->put(*buff_);
            buf->pos(0);
            return buf;
        }

        static ptr<log_entry> deserialize(buffer &buf) {
            ulong term = buf.get_ulong();
            log_val_type t = static_cast<log_val_type>(buf.get_byte());
            bufptr data = buffer::copy(buf);
            return cs_new<log_entry>(term, std::move(data), t);
        }

        static ulong term_in_buffer(buffer &buf) {
            ulong term = buf.get_ulong();
            buf.pos(0); // reset the position
            return term;
        }

    private:
        ulong term_;
        log_val_type value_type_;
        bufptr buff_;
    };
}
#endif //_LOG_ENTRY_HXX_