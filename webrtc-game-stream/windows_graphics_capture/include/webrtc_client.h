#pragma once

#include "common.h"
#include <json/json.h>

namespace WGC {

/**
 * @brief WebRTC client for streaming encoded frames
 * 
 * This class handles:
 * - WebSocket signaling connection
 * - WebRTC peer connection setup
 * - SDP offer/answer exchange
 * - ICE candidate handling
 * - Data channel for control messages
 * - Video track management
 * - Network statistics
 */
class WebRTCClient {
public:
    using ConnectionStateCallback = std::function<void(const std::string& state)>;
    using DataChannelMessageCallback = std::function<void(const std::string& message)>;

    WebRTCClient();
    ~WebRTCClient();

    /**
     * @brief Initialize WebRTC client
     * @param config Configuration with signaling server URL
     * @return ErrorCode indicating success or failure
     */
    ErrorCode initialize(const Config& config);

    /**
     * @brief Connect to signaling server
     * @return ErrorCode indicating success or failure
     */
    ErrorCode connect();

    /**
     * @brief Disconnect from signaling server and close peer connection
     */
    void disconnect();

    /**
     * @brief Send encoded frame over WebRTC
     * @param frame Encoded frame data
     * @return ErrorCode indicating success or failure
     */
    ErrorCode sendFrame(const EncodedFrame& frame);

    /**
     * @brief Check if connected to peer
     * @return true if peer connection is established
     */
    bool isConnected() const { return connected_; }

    /**
     * @brief Set connection state change callback
     * @param callback Function to call on state change
     */
    void setConnectionStateCallback(ConnectionStateCallback callback) {
        connectionStateCallback_ = callback;
    }

    /**
     * @brief Set data channel message callback
     * @param callback Function to call on message received
     */
    void setDataChannelCallback(DataChannelMessageCallback callback) {
        dataChannelCallback_ = callback;
    }

    /**
     * @brief Send message over data channel
     * @param message Message to send
     * @return ErrorCode indicating success or failure
     */
    ErrorCode sendDataChannelMessage(const std::string& message);

    /**
     * @brief Get network statistics
     * @return Statistics structure
     */
    Statistics getStatistics() const { return stats_; }

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics() { stats_.reset(); }

    /**
     * @brief Get current RTT (Round Trip Time)
     * @return RTT in milliseconds
     */
    double getCurrentRTT() const;

    /**
     * @brief Get available send bandwidth
     * @return Bandwidth in bits per second
     */
    uint64_t getAvailableBandwidth() const;

private:
    Config config_;
    
    // WebSocket for signaling
    struct WebSocketImpl;
    std::unique_ptr<WebSocketImpl> wsClient_;
    
    // WebRTC peer connection (placeholder - actual implementation needs WebRTC library)
    struct PeerConnectionImpl;
    std::unique_ptr<PeerConnectionImpl> peerConnection_;
    
    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};
    std::string sessionId_;
    
    // Callbacks
    ConnectionStateCallback connectionStateCallback_;
    DataChannelMessageCallback dataChannelCallback_;
    
    // Statistics
    Statistics stats_;
    uint64_t connectionStartTime_{0};
    
    // Synchronization
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    /**
     * @brief Initialize WebSocket connection
     * @return ErrorCode indicating success or failure
     */
    ErrorCode initializeWebSocket();

    /**
     * @brief Initialize WebRTC peer connection
     * @return ErrorCode indicating success or failure
     */
    ErrorCode initializePeerConnection();

    /**
     * @brief Handle incoming signaling message
     * @param message JSON message from signaling server
     */
    void handleSignalingMessage(const Json::Value& message);

    /**
     * @brief Handle SDP offer from peer
     * @param sdp SDP offer string
     */
    void handleOffer(const std::string& sdp);

    /**
     * @brief Handle SDP answer from peer
     * @param sdp SDP answer string
     */
    void handleAnswer(const std::string& sdp);

    /**
     * @brief Handle ICE candidate from peer
     * @param candidate ICE candidate JSON
     */
    void handleIceCandidate(const Json::Value& candidate);

    /**
     * @brief Create SDP offer
     * @return SDP offer string
     */
    std::string createOffer();

    /**
     * @brief Create SDP answer
     * @param offer Remote SDP offer
     * @return SDP answer string
     */
    std::string createAnswer(const std::string& offer);

    /**
     * @brief Send signaling message to server
     * @param message JSON message to send
     * @return ErrorCode indicating success or failure
     */
    ErrorCode sendSignalingMessage(const Json::Value& message);

    /**
     * @brief Setup ICE servers (STUN/TURN)
     * @return ErrorCode indicating success or failure
     */
    ErrorCode setupIceServers();

    /**
     * @brief Create data channel for control messages
     * @return ErrorCode indicating success or failure
     */
    ErrorCode createDataChannel();

    /**
     * @brief Handle peer connection state change
     * @param state New connection state
     */
    void onConnectionStateChange(const std::string& state);

    /**
     * @brief Handle ICE connection state change
     * @param state New ICE state
     */
    void onIceConnectionStateChange(const std::string& state);

    /**
     * @brief Handle local ICE candidate generation
     * @param candidate ICE candidate
     */
    void onIceCandidate(const Json::Value& candidate);

    /**
     * @brief Handle data channel open
     */
    void onDataChannelOpen();

    /**
     * @brief Handle data channel message
     * @param message Received message
     */
    void onDataChannelMessage(const std::string& message);

    /**
     * @brief Cleanup WebRTC resources
     */
    void cleanup();

    /**
     * @brief Parse JSON message safely
     * @param data Raw message data
     * @return Parsed JSON value
     */
    Json::Value parseJson(const std::string& data);

    /**
     * @brief Serialize JSON to string
     * @param json JSON value
     * @return Serialized string
     */
    std::string serializeJson(const Json::Value& json);
};

} // namespace WGC
