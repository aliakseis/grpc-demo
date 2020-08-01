#pragma once

/*!
 * \brief The IPublishSubscribeClient interface
 */
struct IPublishSubscribeClient
{
    /** Connectivity state of a channel. */
    enum connectivity_state {
        /** channel is idle */
        GRPC_CHANNEL_IDLE,
        /** channel is connecting */
        GRPC_CHANNEL_CONNECTING,
        /** channel is ready for work */
        GRPC_CHANNEL_READY,
        /** channel has seen a failure but expects to recover */
        GRPC_CHANNEL_TRANSIENT_FAILURE,
        /** channel has seen a failure that it cannot recover from */
        GRPC_CHANNEL_SHUTDOWN
    };

    virtual ~IPublishSubscribeClient() = default;
    /*!
     * \brief TryCancel
     */
    virtual void TryCancel() = 0;
    /*!
     * \brief GetConnectionState
     * \param try_to_connect
     * \return
     */
    virtual connectivity_state GetConnectionState(bool try_to_connect) = 0;
};
