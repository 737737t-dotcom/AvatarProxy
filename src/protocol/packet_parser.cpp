#include "packet_parser.h"
#include <stdexcept>
#include <lz4.h>
#include <sstream>
#include <cstring>
#include <arpa/inet.h>

ParsedPacket PacketParser::parse(const std::vector<uint8_t>& data) {
    if (data.size() < 5) {
        throw std::runtime_error("Packet too short");
    }

    ParsedPacket packet;
    size_t offset = 4; // Skip packet length
    
    packet.flags.value = data[offset++];
    
    if (packet.flags.has_crc32()) {
        if (data.size() < offset + 4) {
            throw std::runtime_error("Missing CRC32");
        }
        offset += 4; // Skip CRC32 for now
    }
    
    std::vector<uint8_t> payload;
    if (packet.flags.has_lz4()) {
        if (data.size() < offset + 4) {
            throw std::runtime_error("Missing uncompressed size");
        }
        
        // Read uncompressed size in little-endian
        uint32_t uncompressed_size = data[offset] | 
                                   (data[offset+1] << 8) | 
                                   (data[offset+2] << 16) | 
                                   (data[offset+3] << 24);
        offset += 4;
        
        if (uncompressed_size > 10000000) { // 10MB limit
            throw std::runtime_error("Uncompressed size too large");
        }
        
        payload.resize(uncompressed_size);
        int result = LZ4_decompress_safe(
            reinterpret_cast<const char*>(&data[offset]),
            reinterpret_cast<char*>(payload.data()),
            data.size() - offset,
            uncompressed_size
        );
        
        if (result < 0) {
            throw std::runtime_error("LZ4 decompression failed");
        }
    } else {
        payload.assign(data.begin() + offset, data.end());
    }
    
    if (payload.empty()) {
        throw std::runtime_error("Empty payload");
    }
    
    const uint8_t* ptr = payload.data();
    const uint8_t* end = ptr + payload.size();
    
    packet.message_type = *ptr++;
    
    if (ptr < end) {
        packet.data = decode_map(ptr, end);
    }
    
    packet.raw_data = data;
    return packet;
}

ProtocolMap PacketParser::decode_map(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 4 > end) throw std::runtime_error("Not enough data for map count");
    
    uint32_t count = read_be32(ptr);
    if (count > 10000) throw std::runtime_error("Map too large: " + std::to_string(count));
    
    ProtocolMap map;
    
    for (uint32_t i = 0; i < count && ptr < end; ++i) {
        // Read key length
        if (ptr + 2 > end) break;
        uint32_t key_len = read_be16(ptr);
        
        // Handle long keys
        if (key_len > 32766) {
            if (ptr + 2 > end) break;
            uint32_t low = read_be16(ptr);
            key_len = (key_len << 16) | low;
        }
        
        if (key_len > 1000000) break; // Skip corrupted keys
        
        std::string key = decode_string(ptr, end, key_len);
        if (key.empty()) key = "unknown";
        
        ProtocolValue value = decode_value(ptr, end);
        map[key] = value;
    }
    
    return map;
}

ProtocolValue PacketParser::decode_value(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr >= end) return ProtocolValue{};
    
    uint8_t type = *ptr++;
    
    switch (static_cast<ValueKind>(type)) {
        case ValueKind::Null:
            return ProtocolValue{};
            
        case ValueKind::Bool:
            if (ptr >= end) return ProtocolValue{};
            return ProtocolValue{*ptr++ != 0};
            
        case ValueKind::Int32: {
            if (ptr + 4 > end) return ProtocolValue{};
            int32_t val = static_cast<int32_t>(read_be32(ptr));
            return ProtocolValue{val};
        }
        
        case ValueKind::Int64: {
            if (ptr + 8 > end) return ProtocolValue{};
            int64_t val = static_cast<int64_t>(read_be64(ptr));
            return ProtocolValue{val};
        }
        
        case ValueKind::Float64: {
            if (ptr + 8 > end) return ProtocolValue{};
            double val = read_double_be(ptr);
            return ProtocolValue{val};
        }
        
        case ValueKind::String: {
            if (ptr + 2 > end) return ProtocolValue{};
            uint16_t len = read_be16(ptr);
            return ProtocolValue{decode_string(ptr, end, len)};
        }
        
        case ValueKind::LongString: {
            if (ptr + 4 > end) return ProtocolValue{};
            uint32_t len = read_be32(ptr);
            return ProtocolValue{decode_string(ptr, end, len)};
        }
        
        case ValueKind::Map: {
            ProtocolMap map = decode_map(ptr, end);
            return ProtocolValue{map};
        }
        
        case ValueKind::Array: {
            if (ptr + 4 > end) return ProtocolValue{};
            uint32_t count = read_be32(ptr);
            if (count > 10000) return ProtocolValue{}; // Limit array size
            
            ProtocolArray array;
            array.reserve(count);
            
            for (uint32_t i = 0; i < count && ptr < end; ++i) {
                array.push_back(decode_value(ptr, end));
            }
            return ProtocolValue{array};
        }
        
        default:
            return ProtocolValue{};
    }
}

std::string PacketParser::decode_string(const uint8_t*& ptr, const uint8_t* end, uint32_t length) {
    if (ptr + length > end) return "";
    
    std::string result(reinterpret_cast<const char*>(ptr), length);
    ptr += length;
    return result;
}

uint32_t PacketParser::read_be32(const uint8_t*& ptr) {
    uint32_t val = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
    ptr += 4;
    return val;
}

uint16_t PacketParser::read_be16(const uint8_t*& ptr) {
    uint16_t val = ntohs(*reinterpret_cast<const uint16_t*>(ptr));
    ptr += 2;
    return val;
}

uint64_t PacketParser::read_be64(const uint8_t*& ptr) {
    uint64_t val = be64toh(*reinterpret_cast<const uint64_t*>(ptr));
    ptr += 8;
    return val;
}

double PacketParser::read_double_be(const uint8_t*& ptr) {
    uint64_t bits = read_be64(ptr);
    double val;
    memcpy(&val, &bits, sizeof(double));
    return val;
}

std::string ProtocolValue::to_json() const {
    std::string result;
    result.reserve(256); // Pre-allocate
    
    if (is_null()) {
        return "null";
    } else if (is_bool()) {
        return as_bool() ? "true" : "false";
    } else if (is_int32()) {
        return std::to_string(as_int32());
    } else if (is_int64()) {
        return std::to_string(as_int64());
    } else if (is_double()) {
        return std::to_string(as_double());
    } else if (is_string()) {
        result = "\"";
        const auto& str = as_string();
        result.reserve(str.length() + 10);
        for (unsigned char c : str) {
            if (c >= 32 || c == '\t' || c == '\n' || c == '\r') {
                if (c == '"' || c == '\\') result += '\\';
                result += c;
            }
        }
        result += "\"";
        return result;
    } else if (is_map()) {
        result = "{";
        bool first = true;
        for (const auto& [key, val] : as_map()) {
            if (!first) result += ",";
            result += "\"" + key + "\":" + val.to_json();
            first = false;
        }
        result += "}";
        return result;
    } else if (is_array()) {
        result = "[";
        bool first = true;
        for (const auto& val : as_array()) {
            if (!first) result += ",";
            result += val.to_json();
            first = false;
        }
        result += "]";
        return result;
    }
    
    return "null";
}
