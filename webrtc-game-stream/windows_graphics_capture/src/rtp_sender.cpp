#include "rtp_sender.h"
#include <random>

namespace WGC {

RTPSender::RTPSender() 
    : socket_(INVALID_SOCKET),
      sequence_number_(0),
      timestamp_(0),
      ssrc_(0),
      payload_type_(96),
      target_port_(0) {
}

RTPSender::~RTPSender() {
    close();
}

ErrorCode RTPSender::initialize(
    const std::string& targetHost,
    int targetPort,
    uint8_t payloadType
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARNING("RTP sender already initialized");
        return ErrorCode::Success;
    }
    
    LOG_INFO("Initializing RTP sender...");
    LOG_INFO("Target: " + targetHost + ":" + std::to_string(targetPort));
    
    target_host_ = targetHost;
    target_port_ = targetPort;
    payload_type_ = payloadType;
    
    // Initialize Winsock (should already be done, but safe to call again)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    
    // Create UDP socket
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
        LOG_ERROR("Failed to create UDP socket");
        return ErrorCode::NetworkError;
    }
    
    // Setup server address
    memset(&server_addr_, 0, sizeof(server_addr_));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(targetPort);
    
    // Resolve hostname
    if (inet_pton(AF_INET, targetHost.c_str(), &server_addr_.sin_addr) != 1) {
        // Try to resolve as hostname
        struct addrinfo hints = {}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        
        if (getaddrinfo(targetHost.c_str(), nullptr, &hints, &result) != 0) {
            LOG_ERROR("Failed to resolve hostname: " + targetHost);
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            return ErrorCode::NetworkError;
        }
        
        server_addr_.sin_addr = ((struct sockaddr_in*)result->ai_addr)->sin_addr;
        freeaddrinfo(result);
    }
    
    // Generate random SSRC
    ssrc_ = generateSSRC();
    
    // Initialize sequence number and timestamp
    sequence_number_ = static_cast<uint16_t>(rand() % 65536);
    timestamp_ = static_cast<uint32_t>(rand());
    
    initialized_ = true;
    stats_.reset();
    
    LOG_INFO("RTP sender initialized successfully");
    LOG_INFO("SSRC: " + std::to_string(ssrc_));
    LOG_INFO("Payload type: " + std::to_string(payload_type_));
    
    return ErrorCode::Success;
}

ErrorCode RTPSender::sendFrame(const EncodedFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("RTP sender not initialized");
        return ErrorCode::InitializationFailed;
    }
    
    // Convert timestamp to RTP timestamp (90kHz clock)
    uint32_t rtp_timestamp = convertTimestamp(frame.timestamp);
    
    // Check if frame needs fragmentation
    if (frame.data.size() > MAX_RTP_PAYLOAD_SIZE) {
        return fragmentAndSend(frame);
    }
    
    // Create RTP packet
    std::vector<uint8_t> packet(12 + frame.data.size());
    
    // Create RTP header
    size_t header_size = createRTPHeader(
        packet.data(),
        true,  // marker bit (end of frame)
        rtp_timestamp,
        sequence_number_
    );
    
    // Copy payload
    memcpy(packet.data() + header_size, frame.data.data(), frame.data.size());
    
    // Send packet
    ErrorCode result = sendPacket(packet.data(), packet.size());
    
    if (result == ErrorCode::Success) {
        sequence_number_++;
        timestamp_ = rtp_timestamp;
        stats_.framesSent++;
        stats_.totalBytesEncoded += frame.data.size();
    } else {
        stats_.framesDropped++;
    }
    
    return result;
}

void RTPSender::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    
    initialized_ = false;
    
    LOG_INFO("RTP sender closed");
}

size_t RTPSender::createRTPHeader(
    uint8_t* buffer,
    bool marker,
    uint32_t timestamp,
    uint16_t sequenceNumber
) {
    // RTP header format (12 bytes):
    // 0                   1                   2                   3
    // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |V=2|P|X|  CC   |M|     PT      |       sequence number         |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |                           timestamp                           |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |           synchronization source (SSRC) identifier            |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    
    // Byte 0: V=2, P=0, X=0, CC=0
    buffer[0] = 0x80;
    
    // Byte 1: M bit + payload type
    buffer[1] = (marker ? 0x80 : 0x00) | (payload_type_ & 0x7F);
    
    // Bytes 2-3: Sequence number
    buffer[2] = (sequenceNumber >> 8) & 0xFF;
    buffer[3] = sequenceNumber & 0xFF;
    
    // Bytes 4-7: Timestamp
    buffer[4] = (timestamp >> 24) & 0xFF;
    buffer[5] = (timestamp >> 16) & 0xFF;
    buffer[6] = (timestamp >> 8) & 0xFF;
    buffer[7] = timestamp & 0xFF;
    
    // Bytes 8-11: SSRC
    buffer[8] = (ssrc_ >> 24) & 0xFF;
    buffer[9] = (ssrc_ >> 16) & 0xFF;
    buffer[10] = (ssrc_ >> 8) & 0xFF;
    buffer[11] = ssrc_ & 0xFF;
    
    return 12;
}

ErrorCode RTPSender::sendPacket(const uint8_t* data, size_t size) {
    int result = sendto(
        socket_,
        reinterpret_cast<const char*>(data),
        static_cast<int>(size),
        0,
        reinterpret_cast<const sockaddr*>(&server_addr_),
        sizeof(server_addr_)
    );
    
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        LOG_ERROR("Failed to send RTP packet: " + std::to_string(error));
        return ErrorCode::NetworkError;
    }
    
    return ErrorCode::Success;
}

ErrorCode RTPSender::fragmentAndSend(const EncodedFrame& frame) {
    // Fragment large frames into multiple RTP packets
    // This is a simplified implementation - production would use proper
    // H.264 fragmentation (FU-A packets)
    
    LOG_WARNING("Frame fragmentation not fully implemented");
    
    size_t offset = 0;
    size_t remaining = frame.data.size();
    uint32_t rtp_timestamp = convertTimestamp(frame.timestamp);
    
    while (remaining > 0) {
        size_t chunk_size = std::min(remaining, MAX_RTP_PAYLOAD_SIZE);
        bool is_last = (remaining == chunk_size);
        
        // Create packet
        std::vector<uint8_t> packet(12 + chunk_size);
        
        // Create header
        size_t header_size = createRTPHeader(
            packet.data(),
            is_last,  // marker bit only on last packet
            rtp_timestamp,
            sequence_number_
        );
        
        // Copy payload chunk
        memcpy(packet.data() + header_size, 
               frame.data.data() + offset, 
               chunk_size);
        
        // Send packet
        ErrorCode result = sendPacket(packet.data(), packet.size());
        if (result != ErrorCode::Success) {
            stats_.framesDropped++;
            return result;
        }
        
        sequence_number_++;
        offset += chunk_size;
        remaining -= chunk_size;
    }
    
    timestamp_ = rtp_timestamp;
    stats_.framesSent++;
    stats_.totalBytesEncoded += frame.data.size();
    
    return ErrorCode::Success;
}

uint32_t RTPSender::generateSSRC() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
    return dis(gen);
}

uint32_t RTPSender::convertTimestamp(uint64_t timestampUs) {
    // Convert microseconds to 90kHz clock (standard for video RTP)
    // 90000 Hz = 90 ticks per millisecond = 0.09 ticks per microsecond
    return static_cast<uint32_t>((timestampUs * 90) / 1000);
}

} // namespace WGC
