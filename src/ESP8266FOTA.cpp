#include "ESP8266FOTA.h"
#include "ESP8266Config.h"
#include "ESP8266Security.h"
#include <base64.hpp>
#include <LittleFS.h>
#include <SHA256.h>

ESP8266FOTA::ESP8266FOTA()
{
    reset();
}

void ESP8266FOTA::begin()
{
    Serial.println("[FOTA] Initializing FOTA system...");
    
    // Initialize LittleFS for chunk storage
    if (!LittleFS.begin())
    {
        Serial.println("[FOTA] Warning: Failed to initialize LittleFS");
        Serial.println("[FOTA] Attempting to format LittleFS...");
        if (LittleFS.format())
        {
            Serial.println("[FOTA] LittleFS formatted successfully");
            if (!LittleFS.begin())
            {
                Serial.println("[FOTA] Error: Still cannot initialize LittleFS");
            }
            else
            {
                Serial.println("[FOTA] LittleFS initialized after format");
            }
        }
        else
        {
            Serial.println("[FOTA] Error: Failed to format LittleFS");
        }
    }
    else
    {
        Serial.println("[FOTA] LittleFS initialized successfully");
    }
    
    // Clean up any existing FOTA files from previous incomplete updates
    cleanupPreviousFOTA();
    
    reset();
    Serial.println("[FOTA] FOTA system initialized");
}

void ESP8266FOTA::reset()
{
    manifest_.reset();
    last_chunk_received_ = 0;
    chunk_verified_ = false;
    update_in_progress_ = false;
    manifest_received_ = false;
    total_chunks_received_ = 0;
    memset(chunks_received_bitmap_, 0, sizeof(chunks_received_bitmap_));
}

float ESP8266FOTA::getProgress() const
{
    if (!manifest_.valid || manifest_.total_chunks == 0)
        return 0.0f;
    
    return (float)total_chunks_received_ / (float)manifest_.total_chunks * 100.0f;
}

bool ESP8266FOTA::isComplete() const
{
    return manifest_.valid && 
           total_chunks_received_ == manifest_.total_chunks && 
           manifest_.total_chunks > 0;
}

bool ESP8266FOTA::processSecureFOTAResponse(const String &secureResponse)
{
    // Parse the secure wrapper
    StaticJsonDocument<2048> secureDoc;
    DeserializationError error = deserializeJson(secureDoc, secureResponse);
    
    if (error)
    {
        Serial.print("[FOTA] Error parsing secure response: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Check if this is a secure wrapper with nonce, payload, and mac
    if (!secureDoc.containsKey("nonce") || !secureDoc.containsKey("payload") || !secureDoc.containsKey("mac"))
    {
        // Assume it's plain JSON, try to process directly
        if (secureDoc.containsKey("fota"))
        {
            return processPlainFOTAResponse(secureDoc["fota"]);
        }
        Serial.println("[FOTA] No secure wrapper or FOTA data found");
        return false;
    }
    
    // Extract secure wrapper components
    uint32_t nonce = secureDoc["nonce"];
    String encodedPayload = secureDoc["payload"];
    String receivedMac = secureDoc["mac"];
    
    // Verify MAC first
    const char *psk = configManager.getSecurityConfig().psk;
    String calculatedMac = ESP8266Security::calculateHMAC(psk, nonce, encodedPayload);
    
    if (calculatedMac != receivedMac)
    {
        Serial.println("[FOTA] Error: MAC verification failed for secure FOTA response");
        return false;
    }
    
    // Decode base64 payload
    unsigned int decodedLength = decode_base64_length((unsigned char *)encodedPayload.c_str());
    unsigned char *decodedBuffer = new unsigned char[decodedLength + 1];
    
    decode_base64((unsigned char *)encodedPayload.c_str(), decodedBuffer);
    decodedBuffer[decodedLength] = '\0';
    
    String decodedPayload = String((char *)decodedBuffer);
    delete[] decodedBuffer;
    
    // Parse the decoded payload
    StaticJsonDocument<2048> payloadDoc;
    error = deserializeJson(payloadDoc, decodedPayload);
    
    if (error)
    {
        Serial.print("[FOTA] Error parsing decoded payload: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Process FOTA data if present
    if (payloadDoc.containsKey("fota"))
    {
        return processPlainFOTAResponse(payloadDoc["fota"]);
    }
    
    // No FOTA data in this response
    return true;
}

bool ESP8266FOTA::processPlainFOTAResponse(const JsonObject &fotaObj)
{
    Serial.print("[FOTA] Processing FOTA message: ");
    serializeJson(fotaObj, Serial);
    Serial.println();

    // Check if this is a manifest (first FOTA message)
    if (fotaObj.containsKey("manifest"))
    {
        if (processManifest(fotaObj))
        {
            Serial.println("[FOTA] Manifest processed successfully");
            return true;
        }
        else
        {
            Serial.println("[FOTA] Failed to process manifest");
            return false;
        }
    }
    // Check if this is a chunk message
    else if (fotaObj.containsKey("chunk_number"))
    {
        if (processChunk(fotaObj))
        {
            Serial.print("[FOTA] Chunk ");
            Serial.print(fotaObj["chunk_number"].as<int>());
            Serial.println(" processed successfully");
            return true;
        }
        else
        {
            Serial.print("[FOTA] Failed to process chunk ");
            Serial.println(fotaObj["chunk_number"].as<int>());
            return false;
        }
    }
    else
    {
        Serial.println("[FOTA] Unknown FOTA message format");
        return false;
    }
}

bool ESP8266FOTA::processManifest(const JsonObject &fota)
{
    if (!fota.containsKey("manifest"))
    {
        Serial.println("[FOTA] Error: No manifest in FOTA message");
        return false;
    }

    JsonObject manifest = fota["manifest"];
    
    // Parse manifest fields
    String version = manifest["version"] | "";
    uint32_t size = manifest["size"] | 0;
    String hash = manifest["hash"] | "";
    uint16_t chunk_size = manifest["chunk_size"] | 0;
    uint16_t total_chunks = manifest["total_chunks"] | 0;

    // Create temporary manifest for validation
    FOTAManifest tempManifest;
    tempManifest.version = version;
    tempManifest.size = size;
    tempManifest.hash = hash;
    tempManifest.chunk_size = chunk_size;
    tempManifest.total_chunks = total_chunks;
    tempManifest.valid = true;
    
    // Validate manifest
    if (!validateManifest(tempManifest))
    {
        Serial.println("[FOTA] Error: Manifest validation failed");
        return false;
    }

    // Check if this is a different version than current
    if (version.equals(configManager.getFirmwareVersion()))
    {
        Serial.println("[FOTA] Warning: Manifest version same as current firmware");
        return false;
    }

    // Store manifest
    manifest_ = tempManifest;
    manifest_received_ = true;
    update_in_progress_ = true;
    last_chunk_received_ = 0;
    chunk_verified_ = true;
    total_chunks_received_ = 0;
    memset(chunks_received_bitmap_, 0, sizeof(chunks_received_bitmap_));

    Serial.println("[FOTA] Manifest processed successfully:");
    Serial.print("  Version: ");
    Serial.println(version);
    Serial.print("  Size: ");
    Serial.println(size);
    Serial.print("  Hash: ");
    Serial.println(hash);
    Serial.print("  Chunk Size: ");
    Serial.println(chunk_size);
    Serial.print("  Total Chunks: ");
    Serial.println(total_chunks);

    return true;
}

bool ESP8266FOTA::processChunk(const JsonObject &fota)
{
    if (!update_in_progress_ || !manifest_.valid)
    {
        Serial.println("[FOTA] Error: No FOTA update in progress or invalid manifest");
        return false;
    }

    // Parse chunk fields
    uint16_t chunk_number = fota["chunk_number"] | 0;
    String data = fota["data"] | "";
    String mac = fota["mac"] | "";
    uint16_t total_chunks = fota["total_chunks"] | 0;

    // Validate chunk
    if (!validateChunk(chunk_number, data, mac))
    {
        chunk_verified_ = false;
        return false;
    }

    // Verify total chunks matches manifest
    if (total_chunks != manifest_.total_chunks)
    {
        Serial.println("[FOTA] Error: Chunk total_chunks mismatch with manifest");
        chunk_verified_ = false;
        return false;
    }

    // Check if we already received this chunk
    if (isChunkReceived(chunk_number))
    {
        Serial.print("[FOTA] Warning: Chunk ");
        Serial.print(chunk_number);
        Serial.println(" already received, skipping");
        last_chunk_received_ = chunk_number;
        chunk_verified_ = true;
        return true;
    }

    // Verify chunk MAC
    if (!verifyChunkMAC(data, mac))
    {
        Serial.print("[FOTA] Error: Chunk ");
        Serial.print(chunk_number);
        Serial.println(" MAC verification failed");
        chunk_verified_ = false;
        return false;
    }

    // Store the chunk
    if (!storeFirmwareChunk(chunk_number, data, mac))
    {
        Serial.print("[FOTA] Error: Failed to store chunk ");
        Serial.println(chunk_number);
        chunk_verified_ = false;
        return false;
    }

    // Update status
    markChunkReceived(chunk_number);
    last_chunk_received_ = chunk_number;
    chunk_verified_ = true;

    Serial.print("[FOTA] Chunk ");
    Serial.print(chunk_number);
    Serial.print(" stored successfully (");
    Serial.print(total_chunks_received_);
    Serial.print("/");
    Serial.print(manifest_.total_chunks);
    Serial.println(" received)");

    // Check if all chunks received
    if (isComplete())
    {
        Serial.println("[FOTA] All chunks received! Assembling firmware...");
        
        if (assembleFirmware())
        {
            Serial.println("[FOTA] Firmware assembled successfully");
            
            if (validateAssembledFirmware())
            {
                Serial.println("[FOTA] Firmware validation successful - Ready for installation!");
                // TODO: Trigger firmware installation/reboot
                // ESP.restart(); // Uncomment when ready to flash new firmware
            }
            else
            {
                Serial.println("[FOTA] Error: Firmware validation failed");
                // Clean up invalid firmware
                LittleFS.remove("/fota_firmware.bin");
            }
        }
        else
        {
            Serial.println("[FOTA] Error: Failed to assemble firmware");
        }
    }

    return true;
}

void ESP8266FOTA::markChunkReceived(uint16_t chunk_num)
{
    if (chunk_num < 512) // Max supported chunks
    {
        uint8_t byte_index = chunk_num / 32;
        uint8_t bit_index = chunk_num % 32;
        if (!(chunks_received_bitmap_[byte_index] & (1UL << bit_index)))
        {
            chunks_received_bitmap_[byte_index] |= (1UL << bit_index);
            total_chunks_received_++;
        }
    }
}

bool ESP8266FOTA::isChunkReceived(uint16_t chunk_num) const
{
    if (chunk_num >= 512) return false;
    uint8_t byte_index = chunk_num / 32;
    uint8_t bit_index = chunk_num % 32;
    return (chunks_received_bitmap_[byte_index] & (1UL << bit_index)) != 0;
}

void ESP8266FOTA::addStatusToConfigRequest(JsonObject &requestObj) const
{
    // Add FOTA status if there's an ongoing update
    if (update_in_progress_ && last_chunk_received_ > 0)
    {
        JsonObject fotaStatusObj = requestObj.createNestedObject("fota_status");
        fotaStatusObj["chunk_received"] = last_chunk_received_;
        fotaStatusObj["verified"] = chunk_verified_;
    }
}

bool ESP8266FOTA::validateManifest(const FOTAManifest &manifest) const
{
    // Check basic fields
    if (manifest.version.isEmpty() || 
        manifest.size == 0 || 
        manifest.hash.isEmpty() || 
        manifest.chunk_size == 0 || 
        manifest.total_chunks == 0)
    {
        Serial.println("[FOTA] Error: Invalid manifest data - missing required fields");
        return false;
    }
    
    // Check reasonable size limits (e.g., max 4MB firmware)
    if (manifest.size > 4 * 1024 * 1024)
    {
        Serial.println("[FOTA] Error: Firmware size too large");
        return false;
    }
    
    // Check chunk size is reasonable (e.g., 512-4096 bytes)
    if (manifest.chunk_size < 512 || manifest.chunk_size > 4096)
    {
        Serial.println("[FOTA] Error: Invalid chunk size");
        return false;
    }
    
    // Check total chunks limit (max 512 supported by bitmap)
    if (manifest.total_chunks > 512)
    {
        Serial.println("[FOTA] Error: Too many chunks (max 512 supported)");
        return false;
    }
    
    // Verify size/chunk calculation
    uint32_t expectedSize = (manifest.total_chunks - 1) * manifest.chunk_size + 
                           (manifest.size % manifest.chunk_size == 0 ? manifest.chunk_size : manifest.size % manifest.chunk_size);
    if (abs((int32_t)(expectedSize - manifest.size)) > (int32_t)manifest.chunk_size)
    {
        Serial.println("[FOTA] Error: Size/chunk calculation mismatch");
        return false;
    }
    
    return true;
}

bool ESP8266FOTA::validateChunk(uint16_t chunk_number, const String &data, const String &mac) const
{
    // Validate chunk data
    if (data.isEmpty() || mac.isEmpty())
    {
        Serial.println("[FOTA] Error: Invalid chunk data - missing data or MAC");
        return false;
    }
    
    // Check if chunk is within valid range
    if (chunk_number >= manifest_.total_chunks)
    {
        Serial.println("[FOTA] Error: Chunk number out of range");
        return false;
    }
    
    return true;
}

bool ESP8266FOTA::storeFirmwareChunk(uint16_t chunk_number, const String &data, const String &mac)
{
    String filename = getChunkFilename(chunk_number);
    
    Serial.print("[FOTA] Storing chunk ");
    Serial.print(chunk_number);
    Serial.print(" (");
    Serial.print(data.length());
    Serial.print(" base64 chars) to ");
    Serial.println(filename);

    // Decode base64 data
    unsigned int decodedLength = decode_base64_length((unsigned char *)data.c_str());
    if (decodedLength == 0)
    {
        Serial.println("[FOTA] Error: Invalid base64 data length");
        return false;
    }
    
    unsigned char *decodedBuffer = new unsigned char[decodedLength];
    if (!decodedBuffer)
    {
        Serial.println("[FOTA] Error: Failed to allocate memory for decoded chunk");
        return false;
    }
    
    decode_base64((unsigned char *)data.c_str(), decodedBuffer);
    
    // Write to LittleFS
    File chunkFile = LittleFS.open(filename, "w");
    if (!chunkFile)
    {
        Serial.print("[FOTA] Error: Failed to create chunk file ");
        Serial.println(filename);
        delete[] decodedBuffer;
        return false;
    }
    
    size_t bytesWritten = chunkFile.write(decodedBuffer, decodedLength);
    chunkFile.close();
    delete[] decodedBuffer;
    
    if (bytesWritten != decodedLength)
    {
        Serial.print("[FOTA] Error: Failed to write complete chunk. Expected ");
        Serial.print(decodedLength);
        Serial.print(" bytes, wrote ");
        Serial.println(bytesWritten);
        LittleFS.remove(filename); // Clean up partial file
        return false;
    }
    
    // Verify the file was written correctly
    File verifyFile = LittleFS.open(filename, "r");
    if (!verifyFile)
    {
        Serial.println("[FOTA] Error: Failed to verify chunk file");
        return false;
    }
    
    size_t fileSize = verifyFile.size();
    verifyFile.close();
    
    if (fileSize != decodedLength)
    {
        Serial.print("[FOTA] Error: Chunk file size mismatch. Expected ");
        Serial.print(decodedLength);
        Serial.print(" bytes, got ");
        Serial.println(fileSize);
        LittleFS.remove(filename);
        return false;
    }
    
    Serial.print("[FOTA] Successfully stored chunk ");
    Serial.print(chunk_number);
    Serial.print(" (");
    Serial.print(decodedLength);
    Serial.println(" bytes)");
    
    return true;
}

bool ESP8266FOTA::verifyChunkMAC(const String &data, const String &mac)
{
    if (mac.isEmpty())
    {
        Serial.println("[FOTA] Error: Empty MAC");
        return false;
    }

    // Calculate HMAC of the chunk data using PSK from configuration
    const char *psk = configManager.getSecurityConfig().psk;
    if (strlen(psk) == 0)
    {
        Serial.println("[FOTA] Error: No PSK configured for MAC verification");
        return false;
    }
    
    // Use nonce 0 for chunk MAC calculation (chunks don't use nonces)
    String calculatedMac = ESP8266Security::calculateHMAC(psk, 0, data);
    
    Serial.print("[FOTA] Expected MAC: ");
    Serial.println(mac);
    Serial.print("[FOTA] Calculated MAC: ");
    Serial.println(calculatedMac);
    
    bool macValid = calculatedMac.equalsIgnoreCase(mac);
    if (!macValid)
    {
        Serial.println("[FOTA] Error: MAC verification failed");
        return false;
    }
    
    Serial.println("[FOTA] MAC verification successful");
    return true;
}

void ESP8266FOTA::printStatus() const
{
    if (update_in_progress_)
    {
        Serial.print("FOTA Update: IN PROGRESS (");
        if (manifest_.valid)
        {
            Serial.print(total_chunks_received_);
            Serial.print("/");
            Serial.print(manifest_.total_chunks);
            Serial.print(" chunks, v");
            Serial.print(manifest_.version);
        }
        Serial.println(")");
    }
    else
    {
        Serial.println("FOTA Update: IDLE");
    }
}

void ESP8266FOTA::printDetailedStatus() const
{
    Serial.println("[FOTA] FOTA Status:");
    Serial.print("  Update in progress: ");
    Serial.println(update_in_progress_ ? "Yes" : "No");
    Serial.print("  Manifest received: ");
    Serial.println(manifest_received_ ? "Yes" : "No");
    
    if (manifest_.valid)
    {
        Serial.print("  Target version: ");
        Serial.println(manifest_.version);
        Serial.print("  Firmware size: ");
        Serial.println(manifest_.size);
        Serial.print("  Total chunks: ");
        Serial.println(manifest_.total_chunks);
        Serial.print("  Chunks received: ");
        Serial.print(total_chunks_received_);
        Serial.print("/");
        Serial.println(manifest_.total_chunks);
        
        if (total_chunks_received_ > 0)
        {
            Serial.print("  Progress: ");
            Serial.print(getProgress(), 1);
            Serial.println("%");
        }
        
        if (isComplete())
        {
            Serial.println("  Status: COMPLETE - Ready for installation");
        }
    }
    
    Serial.print("  Last chunk received: ");
    Serial.println(last_chunk_received_);
    Serial.print("  Last chunk verified: ");
    Serial.println(chunk_verified_ ? "Yes" : "No");
}

void ESP8266FOTA::cleanupPreviousFOTA()
{
    Serial.println("[FOTA] Cleaning up previous FOTA files...");
    
    // Remove any existing chunk files
    Dir dir = LittleFS.openDir("/");
    while (dir.next())
    {
        String fileName = dir.fileName();
        if (fileName.startsWith("/fota_chunk_") && fileName.endsWith(".bin"))
        {
            Serial.print("[FOTA] Removing old chunk file: ");
            Serial.println(fileName);
            LittleFS.remove(fileName);
        }
    }
    
    // Remove assembled firmware file if it exists
    if (LittleFS.exists("/fota_firmware.bin"))
    {
        Serial.println("[FOTA] Removing old firmware file");
        LittleFS.remove("/fota_firmware.bin");
    }
    
    Serial.println("[FOTA] Cleanup complete");
}

String ESP8266FOTA::getChunkFilename(uint16_t chunk_number) const
{
    return "/fota_chunk_" + String(chunk_number) + ".bin";
}

bool ESP8266FOTA::assembleFirmware()
{
    if (!isComplete())
    {
        Serial.println("[FOTA] Error: Cannot assemble firmware - not all chunks received");
        return false;
    }
    
    Serial.println("[FOTA] Assembling firmware from chunks...");
    
    // Create the assembled firmware file
    File firmwareFile = LittleFS.open("/fota_firmware.bin", "w");
    if (!firmwareFile)
    {
        Serial.println("[FOTA] Error: Failed to create firmware file");
        return false;
    }
    
    // Buffer for reading chunks
    const size_t bufferSize = 1024;
    uint8_t *buffer = new uint8_t[bufferSize];
    if (!buffer)
    {
        Serial.println("[FOTA] Error: Failed to allocate assembly buffer");
        firmwareFile.close();
        return false;
    }
    
    uint32_t totalBytesWritten = 0;
    bool assemblySuccess = true;
    
    // Assemble chunks in order
    for (uint16_t chunkNum = 0; chunkNum < manifest_.total_chunks; chunkNum++)
    {
        String chunkFilename = getChunkFilename(chunkNum);
        
        if (!LittleFS.exists(chunkFilename))
        {
            Serial.print("[FOTA] Error: Missing chunk file ");
            Serial.println(chunkFilename);
            assemblySuccess = false;
            break;
        }
        
        File chunkFile = LittleFS.open(chunkFilename, "r");
        if (!chunkFile)
        {
            Serial.print("[FOTA] Error: Failed to open chunk file ");
            Serial.println(chunkFilename);
            assemblySuccess = false;
            break;
        }
        
        // Copy chunk data to firmware file
        size_t chunkSize = chunkFile.size();
        size_t bytesRead = 0;
        
        while (bytesRead < chunkSize)
        {
            size_t toRead = min(bufferSize, chunkSize - bytesRead);
            size_t actualRead = chunkFile.read(buffer, toRead);
            
            if (actualRead != toRead)
            {
                Serial.print("[FOTA] Error: Failed to read from chunk ");
                Serial.println(chunkNum);
                assemblySuccess = false;
                break;
            }
            
            size_t written = firmwareFile.write(buffer, actualRead);
            if (written != actualRead)
            {
                Serial.print("[FOTA] Error: Failed to write to firmware file at chunk ");
                Serial.println(chunkNum);
                assemblySuccess = false;
                break;
            }
            
            bytesRead += actualRead;
            totalBytesWritten += written;
        }
        
        chunkFile.close();
        
        if (!assemblySuccess)
            break;
            
        Serial.print("[FOTA] Assembled chunk ");
        Serial.print(chunkNum);
        Serial.print(" (");
        Serial.print(chunkSize);
        Serial.println(" bytes)");
    }
    
    delete[] buffer;
    firmwareFile.close();
    
    if (!assemblySuccess)
    {
        Serial.println("[FOTA] Firmware assembly failed");
        LittleFS.remove("/fota_firmware.bin");
        return false;
    }
    
    // Verify assembled size matches manifest
    if (totalBytesWritten != manifest_.size)
    {
        Serial.print("[FOTA] Error: Assembled firmware size mismatch. Expected ");
        Serial.print(manifest_.size);
        Serial.print(" bytes, got ");
        Serial.println(totalBytesWritten);
        LittleFS.remove("/fota_firmware.bin");
        return false;
    }
    
    Serial.print("[FOTA] Firmware assembly complete (");
    Serial.print(totalBytesWritten);
    Serial.println(" bytes)");
    
    return true;
}

bool ESP8266FOTA::validateAssembledFirmware() const
{
    if (!LittleFS.exists("/fota_firmware.bin"))
    {
        Serial.println("[FOTA] Error: No assembled firmware file found");
        return false;
    }
    
    File firmwareFile = LittleFS.open("/fota_firmware.bin", "r");
    if (!firmwareFile)
    {
        Serial.println("[FOTA] Error: Cannot open assembled firmware file");
        return false;
    }
    
    // Verify file size
    size_t fileSize = firmwareFile.size();
    if (fileSize != manifest_.size)
    {
        Serial.print("[FOTA] Error: Firmware file size mismatch. Expected ");
        Serial.print(manifest_.size);
        Serial.print(" bytes, got ");
        Serial.println(fileSize);
        firmwareFile.close();
        return false;
    }
    
    // Calculate SHA256 hash of assembled firmware
    Serial.println("[FOTA] Calculating firmware hash...");
    SHA256 sha256;
    sha256.reset();
    
    const size_t bufferSize = 1024;
    uint8_t *buffer = new uint8_t[bufferSize];
    if (!buffer)
    {
        Serial.println("[FOTA] Error: Failed to allocate hash buffer");
        firmwareFile.close();
        return false;
    }
    
    size_t bytesRead = 0;
    while (bytesRead < fileSize)
    {
        size_t toRead = min(bufferSize, fileSize - bytesRead);
        size_t actualRead = firmwareFile.read(buffer, toRead);
        
        if (actualRead != toRead)
        {
            Serial.println("[FOTA] Error: Failed to read firmware for hashing");
            delete[] buffer;
            firmwareFile.close();
            return false;
        }
        
        sha256.update(buffer, actualRead);
        bytesRead += actualRead;
    }
    
    delete[] buffer;
    firmwareFile.close();
    
    // Finalize hash
    uint8_t hash[SHA256::HASH_SIZE];
    sha256.finalize(hash, sizeof(hash));
    
    // Convert hash to hex string
    String calculatedHash = "";
    for (int i = 0; i < SHA256::HASH_SIZE; i++)
    {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        calculatedHash += hex;
    }
    
    Serial.print("[FOTA] Expected hash: ");
    Serial.println(manifest_.hash);
    Serial.print("[FOTA] Calculated hash: ");
    Serial.println(calculatedHash);
    
    // Compare with manifest hash
    if (!calculatedHash.equalsIgnoreCase(manifest_.hash))
    {
        Serial.println("[FOTA] Error: Firmware hash validation failed");
        return false;
    }
    
    Serial.println("[FOTA] Firmware hash validation successful");
    return true;
}