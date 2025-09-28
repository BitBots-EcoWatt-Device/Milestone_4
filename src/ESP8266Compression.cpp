#include "ESP8266Compression.h"

namespace Compression
{
    size_t varint_encode(uint32_t value, std::vector<uint8_t> &out)
    {
        size_t startSize = out.size();
        while (value >= 0x80)
        {
            out.push_back(static_cast<uint8_t>(value | 0x80));
            value >>= 7;
        }
        out.push_back(static_cast<uint8_t>(value));
        return out.size() - startSize;
    }

    bool varint_decode(const uint8_t *data, size_t len, size_t &offset, uint32_t &value_out)
    {
        uint32_t result = 0;
        int shift = 0;
        while (offset < len && shift <= 28)
        {
            uint8_t byte = data[offset++];
            result |= static_cast<uint32_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0)
            {
                value_out = result;
                return true;
            }
            shift += 7;
        }
        return false; // malformed
    }

    void delta_compress(const std::vector<long> &samples, std::vector<int32_t> &deltasOut)
    {
        deltasOut.clear();
        if (samples.empty())
            return;
        deltasOut.reserve(samples.size());
        long prev = samples[0];
        deltasOut.push_back(static_cast<int32_t>(prev)); // absolute first
        for (size_t i = 1; i < samples.size(); ++i)
        {
            long curr = samples[i];
            long d = curr - prev;
            deltasOut.push_back(static_cast<int32_t>(d));
            prev = curr;
        }
    }

    void delta_decompress(const std::vector<int32_t> &deltas, std::vector<long> &samplesOut)
    {
        samplesOut.clear();
        if (deltas.empty())
            return;
        samplesOut.reserve(deltas.size());
        long acc = deltas[0];
        samplesOut.push_back(acc);
        for (size_t i = 1; i < deltas.size(); ++i)
        {
            acc += deltas[i];
            samplesOut.push_back(acc);
        }
    }

    void encode_deltas_varint(const std::vector<int32_t> &deltas, std::vector<uint8_t> &bytesOut)
    {
        bytesOut.clear();
        bytesOut.reserve(deltas.size());
        for (size_t i = 0; i < deltas.size(); ++i)
        {
            uint32_t zz = zigzag_encode(deltas[i]);
            varint_encode(zz, bytesOut);
        }
    }

    bool decode_deltas_varint(const std::vector<uint8_t> &bytes, std::vector<int32_t> &deltasOut)
    {
        deltasOut.clear();
        size_t off = 0;
        while (off < bytes.size())
        {
            uint32_t v;
            if (!varint_decode(bytes.data(), bytes.size(), off, v))
                return false;
            deltasOut.push_back(zigzag_decode(v));
        }
        return true;
    }

    String hex_encode(const std::vector<uint8_t> &bytes)
    {
        String hex;
        hex.reserve(bytes.size() * 2);
        const char *hexChars = "0123456789ABCDEF";
        for (uint8_t b : bytes)
        {
            hex += hexChars[(b >> 4) & 0xF];
            hex += hexChars[b & 0xF];
        }
        return hex;
    }

    std::vector<uint8_t> hex_decode(const String &hex)
    {
        std::vector<uint8_t> out;
        auto hexValue = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        size_t n = hex.length();
        out.reserve(n / 2);
        for (size_t i = 0; i + 1 < n; i += 2)
        {
            int hi = hexValue(hex[i]);
            int lo = hexValue(hex[i + 1]);
            if (hi < 0 || lo < 0)
                break;
            out.push_back(static_cast<uint8_t>((hi << 4) | lo));
        }
        return out;
    }
}
