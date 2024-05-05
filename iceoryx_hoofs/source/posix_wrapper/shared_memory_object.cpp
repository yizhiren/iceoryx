// Copyright (c) 2019 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 - 2022 by Apex.AI Inc. All rights reserved.
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

#include "iceoryx_hoofs/internal/posix_wrapper/shared_memory_object.hpp"
#include "iceoryx_hoofs/cxx/attributes.hpp"
#include "iceoryx_hoofs/cxx/helplets.hpp"
#include "iceoryx_hoofs/platform/fcntl.hpp"
#include "iceoryx_hoofs/platform/unistd.hpp"
#include "iceoryx_hoofs/posix_wrapper/signal_handler.hpp"

#include <bitset>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>

namespace iox
{
namespace posix
{
constexpr void* SharedMemoryObject::NO_ADDRESS_HINT;
constexpr uint64_t SIGBUS_ERROR_MESSAGE_LENGTH = 1024U + platform::IOX_MAX_SHM_NAME_LENGTH;

static char sigbusErrorMessage[SIGBUS_ERROR_MESSAGE_LENGTH];
static std::mutex sigbusHandlerMutex;

static void memsetSigbusHandler(int) noexcept
{
    auto result = write(STDERR_FILENO, sigbusErrorMessage, strnlen(sigbusErrorMessage, SIGBUS_ERROR_MESSAGE_LENGTH));
    IOX_DISCARD_RESULT(result);
    _exit(EXIT_FAILURE);
}

// TODO: reduce number of arguments by moving them to struct
// NOLINTNEXTLINE(readability-function-size)
SharedMemoryObject::SharedMemoryObject(const SharedMemory::Name_t& name,
                                       const uint64_t memorySizeInBytes,
                                       const AccessMode accessMode,
                                       const OpenMode openMode,
                                       const cxx::optional<const void*>& baseAddressHint,
                                       const cxx::perms permissions) noexcept
    : m_name(name)
    , m_memorySizeInBytes(cxx::align(memorySizeInBytes, Allocator::MEMORY_ALIGNMENT))
    , m_accessMode(accessMode)
    , m_openMode(openMode)
    , m_baseAddressHint((baseAddressHint) ? *baseAddressHint : NO_ADDRESS_HINT)
    , m_permissions(permissions)
{
    open();
    if (isInitialized())
    {
        std::clog << "SharedMemoryObject construct " << m_name
                  << " success, set shm base addr to " << m_memoryMap->getBaseAddress()
                  << std::endl;
        m_allocator.emplace(m_memoryMap->getBaseAddress(), m_memorySizeInBytes);
    }
}

void SharedMemoryObject::open() noexcept
{
    m_isInitialized = true;

    SharedMemoryBuilder()
        .name(m_name)
        .accessMode(m_accessMode)
        .openMode(m_openMode)
        .filePermissions(m_permissions)
        .size(m_memorySizeInBytes)
        .create()
        .and_then([this](auto& sharedMemory) { 
            if (m_sharedMemory) { m_sharedMemory->reset(); }
            m_sharedMemory.emplace(std::move(sharedMemory)); })
        .or_else([this](auto&) {
            std::cerr << "Unable to create SharedMemoryObject since we could not acquire a SharedMemory resource"
                      << std::endl;
            m_isInitialized = false;
            m_errorValue = SharedMemoryObjectError::SHARED_MEMORY_CREATION_FAILED;
        });

    if (m_isInitialized)
    {
        MemoryMapBuilder()
            .baseAddressHint(m_baseAddressHint)
            .length(m_memorySizeInBytes)
            .fileDescriptor(m_sharedMemory->getHandle())
            .accessMode(m_accessMode)
            .flags(MemoryMapFlags::SHARE_CHANGES)
            .offset(0)
            .create()
            .and_then([this](auto& memoryMap) { m_memoryMap.emplace(std::move(memoryMap)); })
            .or_else([this](auto) {
                std::cerr << "Failed to map created shared memory into process!" << std::endl;
                m_isInitialized = false;
                m_errorValue = SharedMemoryObjectError::MAPPING_SHARED_MEMORY_FAILED;
            });
    }

    if (!m_isInitialized)
    {
        auto flags = std::cerr.flags();
        std::cerr << "Unable to create a shared memory object with the following properties [ name = " << m_name
                  << ", sizeInBytes = " << m_memorySizeInBytes
                  << ", access mode = " << ACCESS_MODE_STRING[static_cast<uint64_t>(m_accessMode)]
                  << ", open mode = " << OPEN_MODE_STRING[static_cast<uint64_t>(m_openMode)] << ", baseAddressHint = ";
        if (m_baseAddressHint)
        {
            std::cerr << std::hex << m_baseAddressHint << std::dec;
        }
        else
        {
            std::cerr << "no hint set";
        }
        std::cerr << ", permissions = " << std::bitset<sizeof(mode_t)>(static_cast<mode_t>(m_permissions)) << " ]"
                  << std::endl;
        std::cerr.setf(flags);
        return;
    }

    if (m_isInitialized && m_sharedMemory->hasOwnership())
    {
        std::clog << "Reserving " << m_memorySizeInBytes << " bytes in the shared memory [" << m_name << "]" << std::endl;
        if (platform::IOX_SHM_WRITE_ZEROS_ON_CREATION)
        {
            // this lock is required for the case that multiple threads are creating multiple
            // shared memory objects concurrently
            std::lock_guard<std::mutex> lock(sigbusHandlerMutex);
            auto memsetSigbusGuard = registerSignalHandler(Signal::BUS, memsetSigbusHandler);

            snprintf(
                sigbusErrorMessage,
                SIGBUS_ERROR_MESSAGE_LENGTH,
                "While setting the acquired shared memory to zero a fatal SIGBUS signal appeared caused by memset. The "
                "shared memory object with the following properties [ name = %s, sizeInBytes = %llu, access mode = %s, "
                "open mode = %s, baseAddressHint = %p, permissions = %lu ] maybe requires more memory than it is "
                "currently available in the system.\n",
                m_name.c_str(),
                static_cast<unsigned long long>(m_memorySizeInBytes),
                ACCESS_MODE_STRING[static_cast<uint64_t>(m_accessMode)],
                OPEN_MODE_STRING[static_cast<uint64_t>(m_openMode)],
                m_baseAddressHint,
                std::bitset<sizeof(mode_t)>(static_cast<mode_t>(m_permissions)).to_ulong());

            memset(m_memoryMap->getBaseAddress(), 0, m_memorySizeInBytes);
        }
        std::clog << "[ Reserving shared memory successful ] " << std::endl;
    }
}

void* SharedMemoryObject::allocate(const uint64_t size, const uint64_t alignment) noexcept
{
    return m_allocator->allocate(size, alignment);
}

void SharedMemoryObject::finalizeAllocation() noexcept
{
    m_allocator->finalizeAllocation();
}

bool SharedMemoryObject::isInitialized() const noexcept
{
    return m_isInitialized;
}

Allocator* SharedMemoryObject::getAllocator() noexcept
{
    return &*m_allocator;
}

const void* SharedMemoryObject::getBaseAddress() const noexcept
{
    return m_memoryMap->getBaseAddress();
}

void* SharedMemoryObject::getBaseAddress() noexcept
{
    return m_memoryMap->getBaseAddress();
}

uint64_t SharedMemoryObject::getSizeInBytes() const noexcept
{
    return m_memorySizeInBytes;
}

int32_t SharedMemoryObject::getFileHandle() const noexcept
{
    return m_sharedMemory->getHandle();
}

bool SharedMemoryObject::hasOwnership() const noexcept
{
    return m_sharedMemory->hasOwnership();
}

void SharedMemoryObject::reopen() noexcept
{
    open();
    if (isInitialized() && m_allocator)
    {
        std::clog << "SharedMemoryObject reopen " << m_name
                  << " success, change shm base addr to " << m_memoryMap->getBaseAddress()
                  << std::endl;
        m_allocator->changeStartAddress(m_memoryMap->getBaseAddress());
    }
}


} // namespace posix
} // namespace iox
