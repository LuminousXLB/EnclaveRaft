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

#ifndef _CORNERSTONE_HXX_
#define _CORNERSTONE_HXX_

#include <cstdio>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include <memory>
#include <vector>
#include <list>
#include <string>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <exception>
#include <atomic>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include "utils/basic_types.hxx"
#include "utils/pp_util.hxx"
#include "utils/strfmt.hxx"
#include "utils/ptr.hxx"
#include "utils/logger.hxx"
#include "utils/buffer.hxx"

#include "state_machine_interface.hxx"

#include "context.hxx"
#include "raft_params.hxx"
#include "cluster_config.hxx"
#include "raft_server.hxx"
#include "srv_config.hxx"
#include "srv_state.hxx"
#include "srv_role.hxx"
#include "state_mgr_interface.hxx"
#include "peer.hxx"

#include "rpc/msg_base.hxx"
#include "rpc/msg_type.hxx"
#include "rpc/req_msg.hxx"
#include "rpc/resp_msg.hxx"

#include "contribution/log_entry.hxx"
#include "contribution/log_val_type.hxx"
#include "contribution/log_store_interface.hxx"
#include "contribution/snapshot.hxx"
#include "contribution/snapshot_sync_ctx.hxx"
#include "contribution/snapshot_sync_req.hxx"

#include "rpc/async_result.hxx"
#include "rpc/rpc_exception.hxx"
#include "rpc/rpc_listener_interface.hxx"
#include "rpc/rpc_client_interfice.hxx"
#include "rpc/rpc_client_factory_interface.hxx"

#include "async_task/delayed_task.hxx"
#include "async_task/timer_task.hxx"
#include "async_task/delayed_task_scheduler_interface.hxx"

//#include "asio_service.hxx"
//#include "fs_log_store.hxx"
#endif // _CORNERSTONE_HXX_
