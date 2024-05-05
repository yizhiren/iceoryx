// Copyright (c) 2019 - 2021 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2020 - 2021 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "iceoryx_posh/internal/runtime/ipc_runtime_interface_wrapper.hpp"
#include "iceoryx_hoofs/cxx/convert.hpp"
#include "iceoryx_hoofs/posix_wrapper/posix_access_rights.hpp"
#include "iceoryx_posh/version/version_info.hpp"

#include <thread>

namespace iox
{
namespace runtime
{
IpcRuntimeInterfaceWrapper::IpcRuntimeInterfaceWrapper(const RuntimeName_t& roudiName,
                                         const RuntimeName_t& runtimeName,
                                         const units::Duration roudiWaitingTimeout) noexcept
    : m_roudiName(roudiName)
    , m_runtimeName(runtimeName)
    , m_roudiReconnectTimeout(3_s)
    , m_max_keepalive_failure_try_count(3)
    , m_ipcRuntimeInterface(cxx::in_place, m_roudiName, m_runtimeName, roudiWaitingTimeout)
    , m_shmTopicSize(m_ipcRuntimeInterface->getShmTopicSize())
    , m_segmentId(m_ipcRuntimeInterface->getSegmentId())
    , m_keepalive_failure_count(0)
    , m_connectionValid(true)
{
    
}

void IpcRuntimeInterfaceWrapper::tryReconnect() noexcept
{
    bool roudi_ready = true;
    m_ipcRuntimeInterface.emplace(m_roudiName, m_runtimeName, m_roudiReconnectTimeout, [&](){
        roudi_ready = false;
    });

    m_connectionValid = roudi_ready;
}

bool IpcRuntimeInterfaceWrapper::sendKeepalive() noexcept
{
    if (!m_connectionValid) {
        tryReconnect();
        return false; 
    }

    bool keepalive_succ = m_ipcRuntimeInterface->sendKeepalive();

    if (keepalive_succ) {
        m_keepalive_failure_count = 0;
        m_connectionValid = true;
    } else {
        m_keepalive_failure_count ++;
        if (m_keepalive_failure_count >= 
            m_max_keepalive_failure_try_count) {
            m_connectionValid = false;
        }
    }

    if (!m_connectionValid) {
        tryReconnect();
        return false; 
    }

    return keepalive_succ;
}

rp::BaseRelativePointer::offset_t IpcRuntimeInterfaceWrapper::getSegmentManagerAddressOffset() const noexcept
{
    if (!m_connectionValid) { return false; }
    return m_ipcRuntimeInterface->getSegmentManagerAddressOffset();
}

bool IpcRuntimeInterfaceWrapper::sendRequestToRouDi(const IpcMessage& msg, IpcMessage& answer) noexcept
{
    if (!m_connectionValid) { return false; }
    return m_ipcRuntimeInterface->sendRequestToRouDi(msg, answer);
}

size_t IpcRuntimeInterfaceWrapper::getShmTopicSize() noexcept
{
    return m_shmTopicSize;
}

uint64_t IpcRuntimeInterfaceWrapper::getSegmentId() const noexcept
{
    return m_segmentId;
}
} // namespace runtime
} // namespace iox
