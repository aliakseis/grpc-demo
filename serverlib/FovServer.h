#pragma once

/// @file

#include "notifications.hpp"

#include <memory>
#include <string>

/*!
 * \brief The IPublishSubscribeServer interface
 */
struct IPublishSubscribeServer
{
    /*!
     * \brief Push a PlainFoiEvent instance
     * \param notification a PlainFoiEvent instance
     */
    virtual void Push(const PlainFoiEvent& notification) = 0;
    virtual ~IPublishSubscribeServer() = default;
};

/*!
 * \brief The INotifyServer interface
 */
struct INotifyServer
{
    /*!
     * \brief Push a PlainFoiNotify instance
     * \param notification a PlainFoiNotify instance
     */
    virtual void Push(const PlainFoiNotify& notification) = 0;
    virtual ~INotifyServer() = default;
};

/*!
 * \brief MakePublishSubscribeServer make a server broadcasting PlainFoiEvent notigications
 * \param serverIpAddress The address to try to bind to the server in URI form. If
 * the scheme name is omitted, "dns:///" is assumed. To bind to any address,
 * please use IPv6 any, i.e., [::]:<port>, which also accepts IPv4
 * connections.  Valid values include dns:///localhost:1234, /
 * 192.168.1.1:31416, dns:///[::1]:27182, etc.).
 * \return
 */
std::unique_ptr<IPublishSubscribeServer> MakePublishSubscribeServer(const std::string& serverIpAddress);

/*!
 * \brief MakeNotifyServer make a server broadcasting PlainFoiNotify notigications
 * \param serverIpAddress The address to try to bind to the server in URI form. If
 * the scheme name is omitted, "dns:///" is assumed. To bind to any address,
 * please use IPv6 any, i.e., [::]:<port>, which also accepts IPv4
 * connections.  Valid values include dns:///localhost:1234, /
 * 192.168.1.1:31416, dns:///[::1]:27182, etc.).
 * \return
 */
std::unique_ptr<INotifyServer> MakeNotifyServer(const std::string& serverIpAddress);
