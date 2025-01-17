//*********************************************************************
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root
// for license information.
//
// @File: SharedConfigManager.cpp
//
// Purpose:
//      <description>
//
// Notes:
//      <special-instructions>
//
//*********************************************************************

#include "Mlos.Core.h"
#include "Mlos.Core.inl"

namespace Mlos
{
namespace Core
{
// Shared memory mapping name for to store shared configs.
//
const char* const ApplicationConfigSharedMemoryName = "Host_Mlos.Config.SharedMemory";

//----------------------------------------------------------------------------
// NAME: SharedConfigManager::Constructor
//
// PURPOSE:
//
// RETURNS:
//
// NOTES:
//
SharedConfigManager::SharedConfigManager(_In_ MlosContext& mlosContext) noexcept
  : m_mlosContext(mlosContext),
    CleanupOnClose(false)
{
}

//----------------------------------------------------------------------------
// NAME: SharedConfigManager::Destructor
//
// PURPOSE:
//
// RETURNS:
//
// NOTES:
//
SharedConfigManager::~SharedConfigManager()
{
    m_sharedConfigMemoryRegionView.CleanupOnClose |= CleanupOnClose;
}

//----------------------------------------------------------------------------
// NAME: SharedConfigManager::AssignSharedConfigMemoryRegion
//
// PURPOSE:
//
// RETURNS:
//
// NOTES:
//
void SharedConfigManager::AssignSharedConfigMemoryRegion(
    _In_ SharedMemoryRegionView<Internal::SharedConfigMemoryRegion>&& sharedConfigMemoryRegionView)
{
    m_sharedConfigMemoryRegionView.Assign(std::move(sharedConfigMemoryRegionView));
}

//----------------------------------------------------------------------------
// NAME: SharedConfigManager::RegisterSharedConfigMemoryRegion
//
// PURPOSE:
//  Creates a shared config memory region and registers it with the agent.
//
// RETURNS:
//  HRESULT.
//
// NOTES:
//
_Must_inspect_result_
HRESULT SharedConfigManager::CreateSharedConfigMemoryRegion()
{
    // Create (allocate and register) shared config memory region.
    // See Also: Mlos.Agent/MainAgent.cs
    // #TODO now it can be configurable.
    //
    const size_t SharedMemorySize = 65536;

    HRESULT hr = m_mlosContext.CreateMemoryRegion(
        ApplicationConfigSharedMemoryName,
        SharedMemorySize,
        m_sharedConfigMemoryRegionView);
    if (FAILED(hr))
    {
        return hr;
    }

    Internal::SharedConfigMemoryRegion& sharedConfigMemoryRegion = m_sharedConfigMemoryRegionView.MemoryRegion();

    // Register shared memory region.
    //
    ComponentConfig<Internal::RegisteredMemoryRegionConfig> registeredMemoryRegion(m_mlosContext);

    registeredMemoryRegion.MemoryRegionType = sharedConfigMemoryRegion.MemoryHeader.MemoryRegionId.Type;
    registeredMemoryRegion.MemoryRegionIndex = sharedConfigMemoryRegion.MemoryHeader.MemoryRegionId.Index;
    registeredMemoryRegion.SharedMemoryMapName = ApplicationConfigSharedMemoryName;
    registeredMemoryRegion.MemoryRegionSize = SharedMemorySize;

    // Register memory map region in the global shared region.
    //
    hr = SharedConfigManager::CreateOrUpdateFrom(
        m_mlosContext.m_globalMemoryRegion.SharedConfigDictionary,
        registeredMemoryRegion);

    // Register a shared config memory region.
    //
    Internal::RegisterSharedConfigMemoryRegionRequestMessage msg = {};
    msg.SharedMemoryRegionIndex = sharedConfigMemoryRegion.MemoryHeader.MemoryRegionId.Index;

    m_mlosContext.SendControlMessage(msg);

    return hr;
}
}
}
