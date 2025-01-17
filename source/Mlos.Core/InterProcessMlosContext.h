//*********************************************************************
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root
// for license information.
//
// @File: InterProcessMlosContext.h
//
// Purpose:
//      <description>
//
// Notes:
//      <special-instructions>
//
//*********************************************************************

#pragma once

namespace Mlos
{
namespace Core
{
//----------------------------------------------------------------------------
// NAME: InterProcessMlosContextInitializer
//
// PURPOSE:
//  Helper class used to initialize shared memory for inter-process MlosContexts.
//
// NOTES:
//
class InterProcessMlosContextInitializer
{
public:
    InterProcessMlosContextInitializer() {}

    _Must_inspect_result_
    HRESULT Initialize();

    InterProcessMlosContextInitializer(_In_ InterProcessMlosContextInitializer&& initializer) noexcept;

    InterProcessMlosContextInitializer(const InterProcessMlosContextInitializer&) = delete;

    InterProcessMlosContextInitializer& operator=(const InterProcessMlosContextInitializer&) = delete;

private:
    // Global shared memory region.
    //
    SharedMemoryRegionView<Internal::GlobalMemoryRegion> m_globalMemoryRegionView;

    // Named shared memory for Telemetry and Control Channel.
    //
    SharedMemoryMapView m_controlChannelMemoryMapView;

    // Named shared memory for Feedback Channel.
    //
    SharedMemoryMapView m_feedbackChannelMemoryMapView;

    // Channel policy for control channel.
    //
    InterProcessSharedChannelPolicy m_controlChannelPolicy;

    // Channel policy for feedback channel.
    //
    InterProcessSharedChannelPolicy m_feedbackChannelPolicy;

    // Notification event signalled after the target process initialized Mlos context.
    //
    NamedEvent m_targetProcessNamedEvent;

    friend class InterProcessMlosContext;
};

//----------------------------------------------------------------------------
// NAME: InterProcessMlosContext
//
// PURPOSE:
//  Implementation of an inter-process MlosContext.
//
// NOTES:
//
class InterProcessMlosContext : public MlosContext
{
public:
    typedef InterProcessMlosContextInitializer InitializerType;

    InterProcessMlosContext(_In_ InterProcessMlosContextInitializer&&) noexcept;

    ~InterProcessMlosContext();

private:
    InterProcessMlosContextInitializer m_contextInitializer;

    InterProcessSharedChannel m_controlChannel;

    InterProcessSharedChannel m_feedbackChannel;

    NamedEvent m_controlChannelNamedEvent;

    NamedEvent m_feedbackChannelNamedEvent;
};
}
}
