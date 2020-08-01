#pragma once

/// @file

#include "notifications.hpp"

#include "IPublishSubscribeClient.h"

#include <functional>
#include <memory>
#include <string>


using PublishSubscribeClientCallback = std::function<void (const PlainFoiEvent &)>;
using NotifyClientCallback = std::function<void(const PlainFoiNotify &)>;

/*!
 * \brief MakePublishSubscribeClient
 * \param targetIpAddress The URI of the endpoint to connect to.
 * \param id
 * \param callback a PublishSubscribeClientCallback instance
 * \return
 */
std::unique_ptr<IPublishSubscribeClient> MakePublishSubscribeClient(
    const std::string& targetIpAddress, const std::string& id, PublishSubscribeClientCallback callback);

/*!
 * \brief MakeNotifyClient
 * \param targetIpAddress The URI of the endpoint to connect to.
 * \param id
 * \param callback a NotifyClientCallback instance
 * \return
 */
std::unique_ptr<IPublishSubscribeClient> MakeNotifyClient(
    const std::string& targetIpAddress, const std::string& id, NotifyClientCallback callback);
