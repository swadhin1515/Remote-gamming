#pragma once

#include "common.h"

namespace WGC {

/**
 * @brief RTP packet sender for streaming encoded frames
 * 
 * This class handles:
 * - RTP packet creation and formatting
 * - UDP socket management
 * - Sequence number and timestamp tracking
 * - Fragmentation for large frames
 * - Network error handling
 */
class RTPSender {
public:
    RTPSender();
    ~RTPSender();

    /**
     * @brief Initialize RTP sender
     * @param targetHost Target host IP or hostname
     * @param targetPort Target UDP port
     * @param payloadType RTP payload type (96 for H.264, 97 for HEVC)
     * @return ErrorCode indicating success or failure
     */
    ErrorCode initialize(
        const std::string& targetHost,
        int targetPort,
        uint8_t payloadType = 96
    );

    /**
     * @brief Send encoded frame as RTP packets
     * @param frame Encoded frame to send
     * @return ErrorCode indicating success or failure
     */
    ErrorCode sendFrame(const EncodedFrame& frame);

    /**
     * @brief Close RTP sender and cleanup
     */
    void close();

    /**
     * @brief Check if sender is initialized
     * @return true if ready to send
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get statistics
     * @return Statistics structure
     */
    Statistics getStatistics() const { return stats_; }

    /**
     * @brief Reset statistics
     */
    void resetStatistics() { stats_.reset(); }

private:
    SOCKET socket_;
    sockaddr_in server_addr_;
    
    // RTP state
    uint16_t sequence_number_;
    uint32_t timestamp_;
    uint32_t ssrc_;
    uint8_t payload_type_;
    
    // Configuration
    std::string target_host_;
    int target_port_;
    
    // State
    std::atomic<bool> initialized_{false};
    
    // Statistics
    Statistics stats_;
    
    // Synchronization
    mutable std::mutex mutex_;
    
    // Maximum RTP payload size (MTU - IP header - UDP header - RTP header)
    static constexpr size_t MAX_RTP_PAYLOAD_SIZE = 1400;
    
    /**
     * @brief Create RTP header
     * @param buffer Output buffer
     * @param marker Marker bit
     * @param timestamp RTP timestamp
     * @param sequenceNumber Sequence number
     * @return Header size in bytes
     */
    size_t createRTPHeader(
        uint8_t* buffer,
        bool marker,
        uint32_t timestamp,
        uint16_t sequenceNumber
    );

    /**
     * @brief Send single RTP packet
     * @param data Packet data (including RTP header)
     * @param size Packet size
     * @return ErrorCode indicating success or failure
     */
    ErrorCode sendPacket(const uint8_t* data, size_t size);

    /**
     * @brief Fragment and send large frame
     * @param frame Frame to fragment
     * @return ErrorCode indicating success or failure
     */
    ErrorCode fragmentAndSend(const EncodedFrame& frame);

    /**
     * @brief Generate random SSRC
     * @return Random 32-bit SSRC value
     */
    uint32_t generateSSRC();

    /**
     * @brief Convert timestamp from microseconds to RTP timestamp
     * @param timestampUs Timestamp in microseconds
     * @return RTP timestamp (90kHz clock)
     */
    uint32_t convertTimestamp(uint64_t timestampUs);
};

} // namespace WGC
