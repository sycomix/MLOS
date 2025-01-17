//*********************************************************************
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root
// for license information.
//
// @File: InterProcessMlosContext.cpp
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
// Shared memory mapping name must start with "Host_" prefix, to be accessible from certain applications.
//
const char* const GlobalMemoryMapName = "Host_Mlos.GlobalMemory";
const char* const ControlChannelMemoryMapName = "Host_Mlos.ControlChannel";
const char* const FeedbackChannelMemoryMapName = "Host_Mlos.FeedbackChannel";

// Synchronization OS primitive names.
//
const char* const ControlChannelEventName = "Global\\ControlChannel_Event";
const char* const FeedbackChannelEventName = "Global\\FeedbackChannel_Event";
const char* const TargetProcessEventName = "Global\\Mlos_Global";

//----------------------------------------------------------------------------
// NAME: InterProcessMlosContextInitializer::Constructor.
//
// PURPOSE:
//  Move constructor.
//
// NOTES:
//
InterProcessMlosContextInitializer::InterProcessMlosContextInitializer(
    _In_ InterProcessMlosContextInitializer&& initializer) noexcept
  : m_globalMemoryRegionView(std::move(initializer.m_globalMemoryRegionView)),
    m_controlChannelMemoryMapView(std::move(initializer.m_controlChannelMemoryMapView)),
    m_feedbackChannelMemoryMapView(std::move(initializer.m_feedbackChannelMemoryMapView)),
    m_controlChannelPolicy(std::move(initializer.m_controlChannelPolicy)),
    m_feedbackChannelPolicy(std::move(initializer.m_feedbackChannelPolicy)),
    m_targetProcessNamedEvent(std::move(initializer.m_targetProcessNamedEvent))
{
}

//----------------------------------------------------------------------------
// NAME: InterProcessMlosContextInitializer::Initialize
//
// PURPOSE:
//  Opens the shared memory and synchronization primitives used for the communication channel.
//
// NOTES:
//
_Must_inspect_result_
HRESULT InterProcessMlosContextInitializer::Initialize()
{
    // #TODO const as codegen, pass a config struct ?
    //
    const size_t SharedMemorySize = 65536;

    // TODO: Make these config regions configurable to support multiple processes.
    //
    HRESULT hr = m_globalMemoryRegionView.CreateOrOpen(GlobalMemoryMapName, SharedMemorySize);
    if (SUCCEEDED(hr))
    {
        // Increase the usage counter. When closing global shared memory, we will decrease the counter.
        // If there is no process using the shared memory, we will clean the OS resources. On Windows OS,
        // this is no-op; on Linux, we unlink created files.
        //
        Internal::GlobalMemoryRegion& globalMemoryRegion = m_globalMemoryRegionView.MemoryRegion();
        globalMemoryRegion.AttachedProcessesCount.fetch_add(1);
    }

    if (SUCCEEDED(hr))
    {
        hr = m_controlChannelMemoryMapView.CreateOrOpen(ControlChannelMemoryMapName, SharedMemorySize);
    }

    if (SUCCEEDED(hr))
    {
        hr = m_feedbackChannelMemoryMapView.CreateOrOpen(FeedbackChannelMemoryMapName, SharedMemorySize);
    }

    if (SUCCEEDED(hr))
    {
        hr = m_controlChannelPolicy.m_notificationEvent.CreateOrOpen(ControlChannelEventName);
    }

    if (SUCCEEDED(hr))
    {
        hr = m_feedbackChannelPolicy.m_notificationEvent.CreateOrOpen(FeedbackChannelEventName);
    }

    if (SUCCEEDED(hr))
    {
        hr = m_targetProcessNamedEvent.CreateOrOpen(TargetProcessEventName);
    }

    if (SUCCEEDED(hr))
    {
        hr = m_targetProcessNamedEvent.Signal();
    }

    if (FAILED(hr))
    {
        bool cleanupOnClose = false;
        if (!m_globalMemoryRegionView.Buffer.IsInvalid())
        {
            // Decrease the usage counter.
            //
            Internal::GlobalMemoryRegion& globalMemoryRegion = m_globalMemoryRegionView.MemoryRegion();
            uint32_t usageCount = globalMemoryRegion.AttachedProcessesCount.fetch_sub(1);
            if (usageCount == 1)
            {
                // This is the last process using shared memory map.
                //
                cleanupOnClose = true;
            }
        }

        // Close all the shared maps if we fail to create one.
        //
        m_globalMemoryRegionView.CleanupOnClose |= cleanupOnClose;
        m_globalMemoryRegionView.Close();

        m_controlChannelMemoryMapView.CleanupOnClose |= cleanupOnClose;
        m_controlChannelMemoryMapView.Close();

        m_feedbackChannelMemoryMapView.CleanupOnClose |= cleanupOnClose;
        m_feedbackChannelMemoryMapView.Close();

        m_controlChannelPolicy.m_notificationEvent.CleanupOnClose |= cleanupOnClose;
        m_controlChannelPolicy.m_notificationEvent.Close();

        m_feedbackChannelPolicy.m_notificationEvent.CleanupOnClose |= cleanupOnClose;
        m_feedbackChannelPolicy.m_notificationEvent.Close();
    }

    return hr;
}

//----------------------------------------------------------------------------
// NAME: InterProcessMlosContext::Constructor
//
// PURPOSE:
//  Creates InterProcessMlosContext.
//
// NOTES:
//
InterProcessMlosContext::InterProcessMlosContext(_In_ InterProcessMlosContextInitializer&& initializer) noexcept
  : MlosContext(initializer.m_globalMemoryRegionView.MemoryRegion(), m_controlChannel, m_controlChannel, m_feedbackChannel),
    m_contextInitializer(std::move(initializer)),
    m_controlChannel(
        m_contextInitializer.m_globalMemoryRegionView.MemoryRegion().ControlChannelSynchronization,
        m_contextInitializer.m_controlChannelMemoryMapView,
        std::move(m_contextInitializer.m_controlChannelPolicy)),
    m_feedbackChannel(
        m_contextInitializer.m_globalMemoryRegionView.MemoryRegion().FeedbackChannelSynchronization,
        m_contextInitializer.m_feedbackChannelMemoryMapView,
        std::move(m_contextInitializer.m_feedbackChannelPolicy))
{
}

//----------------------------------------------------------------------------
// NAME: InterProcessMlosContext::Destructor
//
// PURPOSE:
//  Destroys InterProcessMlosContext object.
//
// NOTES:
//
InterProcessMlosContext::~InterProcessMlosContext()
{
    // Decrease the usage counter.
    //
    uint32_t usageCount = m_globalMemoryRegion.AttachedProcessesCount.fetch_sub(1);
    if (usageCount == 1)
    {
        // This is the last process using shared memory map.
        //
        m_contextInitializer.m_globalMemoryRegionView.CleanupOnClose = true;
        m_contextInitializer.m_controlChannelMemoryMapView.CleanupOnClose = true;
        m_contextInitializer.m_feedbackChannelMemoryMapView.CleanupOnClose = true;
        m_controlChannel.ChannelPolicy.m_notificationEvent.CleanupOnClose = true;
        m_feedbackChannel.ChannelPolicy.m_notificationEvent.CleanupOnClose = true;
        m_contextInitializer.m_targetProcessNamedEvent.CleanupOnClose = true;

        // Close all the shared config memory regions.
        //
        m_sharedConfigManager.CleanupOnClose = true;
    }
}
}
}
