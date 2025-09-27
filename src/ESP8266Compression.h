#ifndef ESP8266_COMPRESSION_H
#define ESP8266_COMPRESSION_H

#include <Arduino.h>
#include <vector>

namespace Compression
{
    // ZigZag transforms signed integers to unsigned for efficient varint
    inline uint32_t zigzag_encode(int32_t v)
    {
        return (static_cast<uint32_t>(v) << 1) ^ static_cast<uint32_t>(v >> 31);
    }

    inline int32_t zigzag_decode(uint32_t n)
    {
        return static_cast<int32_t>((n >> 1) ^ (~(n & 1) + 1));
    }

    // Varint encode (7-bit per byte, MSB as continuation)
    size_t varint_encode(uint32_t value, std::vector<uint8_t> &out);
    bool varint_decode(const uint8_t *data, size_t len, size_t &offset, uint32_t &value_out);

    // Delta compress a sequence of scaled int samples.
    // Output format for deltasOut: first element is absolute first sample, then signed deltas.
    void delta_compress(const std::vector<long> &samples, std::vector<int32_t> &deltasOut);
    void delta_decompress(const std::vector<int32_t> &deltas, std::vector<long> &samplesOut);

    // Encode deltas into a compact byte stream: varint(zigzag(delta_i)) for i from 0..N-1.
    // Note: For i==0, delta is the absolute first sample.
    void encode_deltas_varint(const std::vector<int32_t> &deltas, std::vector<uint8_t> &bytesOut);
    bool decode_deltas_varint(const std::vector<uint8_t> &bytes, std::vector<int32_t> &deltasOut);

    // Hex helpers for optional transport/debug
    String hex_encode(const std::vector<uint8_t> &bytes);
    std::vector<uint8_t> hex_decode(const String &hex);
}

#endif // ESP8266_COMPRESSION_H
