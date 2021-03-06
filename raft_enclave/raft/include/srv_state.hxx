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

#ifndef _SRV_STATE_HXX_
#define _SRV_STATE_HXX_

namespace cornerstone {
    class srv_state {
    public:
        explicit srv_state(ulong term = 0L, int voted_for = -1)
                : term_(term), voted_for_(voted_for) {}

    __nocopy__(srv_state)

    public:
        ulong get_term() const { return term_; }

        void set_term(ulong term) { term_ = term; }

        int get_voted_for() const { return voted_for_; }

        void set_voted_for(int voted_for) { voted_for_ = voted_for; }

        void inc_term() { term_ += 1; }

        bufptr serialize() const {
            bufptr buf = buffer::alloc(sz_ulong + sz_int);
            buf->put(term_);
            buf->put(voted_for_);
            buf->pos(0);
            return buf;
        }

        static ptr <srv_state> deserialize(buffer &buf) {
            ulong term = buf.get_ulong();
            int voted_for = buf.get_int();
            return cs_new<srv_state>(term, voted_for);
        }

    private:
        ulong term_;
        int voted_for_;
    };
}

#endif
