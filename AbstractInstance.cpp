/*
    QmVk - simple Vulkan library created for QMPlay2
    Copyright (C) 2020  Błażej Szczygieł

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "AbstractInstance.hpp"
#include "PhysicalDevice.hpp"

vk::DispatchLoaderDynamic vk::defaultDispatchLoaderDynamic;

namespace QmVk {

static unique_ptr<vk::DynamicLoader> g_dyld;
static bool s_dldInitialized = false;
static mutex s_dldMutex;

void AbstractInstance::init(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr)
{
    lock_guard<mutex> locker(s_dldMutex);
    if (s_dldInitialized)
        return;

    if (!vkGetInstanceProcAddr)
    {
        g_dyld = make_unique<vk::DynamicLoader>();
        if (!g_dyld->success())
            throw vk::InitializationFailedError("Unable to load Vulkan library");
        vkGetInstanceProcAddr = g_dyld->getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        if (!vkGetInstanceProcAddr)
            throw vk::InitializationFailedError("Unable to get \"vkGetInstanceProcAddr\"");
    }

    vk::defaultDispatchLoaderDynamic.init(*this, vkGetInstanceProcAddr);
    s_dldInitialized = true;
}

vector<shared_ptr<PhysicalDevice>> AbstractInstance::enumeratePhysicalDevices(bool compatibleOnly)
{
    const auto physicalDevices = static_cast<vk::Instance *>(this)->enumeratePhysicalDevices();

    vector<shared_ptr<PhysicalDevice>> physicalDeviceInstances;
    physicalDeviceInstances.reserve(physicalDevices.size());

    for (auto &&vkPhysicalDevice : physicalDevices)
    {
        auto physicalDevice = make_shared<PhysicalDevice>(
            shared_from_this(),
            vkPhysicalDevice,
            PhysicalDevice::Priv()
        );
        physicalDevice->init();
        if (!compatibleOnly || isCompatibleDevice(physicalDevice))
            physicalDeviceInstances.push_back(physicalDevice);
    }

    if (physicalDeviceInstances.empty())
        throw vk::InitializationFailedError("No compatible devices found");

    return physicalDeviceInstances;
}

shared_ptr<Device> AbstractInstance::createDevice(
    const shared_ptr<PhysicalDevice> &physicalDevice,
    vk::QueueFlags queueFlags,
    const vk::PhysicalDeviceFeatures &physicalDeviceFeatures,
    const vector<const char *> &physicalDeviceExtensions,
    uint32_t maxQueueCount)
{
    auto device = physicalDevice->createDevice(
        physicalDevice->getQueueFamilyIndex(queueFlags),
        physicalDeviceFeatures,
        physicalDevice->filterAvailableExtensions(physicalDeviceExtensions),
        maxQueueCount
    );

    lock_guard<mutex> locker(m_deviceMutex);
    m_deviceWeak = device;
    return device;
}
void AbstractInstance::resetDevice(const shared_ptr<Device> &deviceToReset)
{
    if (!deviceToReset)
        return;

    lock_guard<mutex> locker(m_deviceMutex);
    if (m_deviceWeak.lock() == deviceToReset)
        m_deviceWeak.reset();
}
shared_ptr<Device> AbstractInstance::device() const
{
    lock_guard<mutex> locker(m_deviceMutex);
    return m_deviceWeak.lock();
}

}
