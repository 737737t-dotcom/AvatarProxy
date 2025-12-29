#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <memory>

struct PacketFlags {
    uint8_t value;
    static constexpr uint8_t LZ4 = 0x04;
    static constexpr uint8_t CRC32 = 0x08;
    
    bool has_lz4() const { return value & LZ4; }
    bool has_crc32() const { return value & CRC32; }
};

class ProtocolValue;
using ProtocolMap = std::unordered_map<std::string, ProtocolValue>;
using ProtocolArray = std::vector<ProtocolValue>;

enum class ValueKind : uint8_t {
    Null = 0, Bool = 1, Int32 = 2, Int64 = 3,
    Float64 = 4, String = 5, Map = 6, Array = 7, LongString = 8
};

class ProtocolValue {
    using Storage = std::variant<
        std::monostate, bool, int32_t, int64_t, double, std::string,
        std::shared_ptr<ProtocolMap>, std::shared_ptr<ProtocolArray>
    >;
    Storage data_;

public:
    ProtocolValue() = default;
    ProtocolValue(std::nullptr_t) : data_(std::monostate{}) {}
    ProtocolValue(bool v) : data_(v) {}
    ProtocolValue(int32_t v) : data_(v) {}
    ProtocolValue(int64_t v) : data_(v) {}
    ProtocolValue(double v) : data_(v) {}
    ProtocolValue(const char* v) : data_(std::string(v)) {}
    ProtocolValue(std::string v) : data_(std::move(v)) {}
    ProtocolValue(ProtocolMap m) : data_(std::make_shared<ProtocolMap>(std::move(m))) {}
    ProtocolValue(ProtocolArray a) : data_(std::make_shared<ProtocolArray>(std::move(a))) {}

    bool is_null() const { return std::holds_alternative<std::monostate>(data_); }
    bool is_bool() const { return std::holds_alternative<bool>(data_); }
    bool is_int32() const { return std::holds_alternative<int32_t>(data_); }
    bool is_int64() const { return std::holds_alternative<int64_t>(data_); }
    bool is_double() const { return std::holds_alternative<double>(data_); }
    bool is_string() const { return std::holds_alternative<std::string>(data_); }
    bool is_map() const { return std::holds_alternative<std::shared_ptr<ProtocolMap>>(data_); }
    bool is_array() const { return std::holds_alternative<std::shared_ptr<ProtocolArray>>(data_); }

    bool as_bool() const { return std::get<bool>(data_); }
    int32_t as_int32() const { return std::get<int32_t>(data_); }
    int64_t as_int64() const { return is_int32() ? std::get<int32_t>(data_) : std::get<int64_t>(data_); }
    double as_double() const { return std::get<double>(data_); }
    const std::string& as_string() const { return std::get<std::string>(data_); }
    const ProtocolMap& as_map() const { return *std::get<std::shared_ptr<ProtocolMap>>(data_); }
    const ProtocolArray& as_array() const { return *std::get<std::shared_ptr<ProtocolArray>>(data_); }

    std::string to_json() const;
};

struct ParsedPacket {
    uint8_t message_type;
    ProtocolValue data;
    PacketFlags flags;
    std::vector<uint8_t> raw_data;
};

class PacketParser {
public:
    static ParsedPacket parse(const std::vector<uint8_t>& data);
private:
    static ProtocolValue decode_value(const uint8_t*& ptr, const uint8_t* end);
    static std::string decode_string(const uint8_t*& ptr, const uint8_t* end, uint32_t length);
    static ProtocolMap decode_map(const uint8_t*& ptr, const uint8_t* end);
    static uint32_t read_be32(const uint8_t*& ptr);
    static uint16_t read_be16(const uint8_t*& ptr);
    static uint64_t read_be64(const uint8_t*& ptr);
    static double read_double_be(const uint8_t*& ptr);
};
