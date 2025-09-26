#ifndef USER_CONFIG_H
#define USER_CONFIG_H

// WiFi Configuration
#define USER_WIFI_SSID        "YourWiFiSSID"
#define USER_WIFI_PASSWORD    "YourWiFiPassword"
#define USER_WIFI_HOSTNAME    "bitbots-ecoWatt"


// API Configuration
#define USER_API_KEY          "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFjOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExMg=="
#define USER_API_READ_URL     "http://20.15.114.131:8080/api/inverter/read"
#define USER_API_WRITE_URL    "http://20.15.114.131:8080/api/inverter/write"
#define USER_API_TIMEOUT_MS   5000

// Cloud Upload URL (Web Dashboard)
#define USER_CLOUD_UPLOAD_URL "http://172.20.10.4:5001/upload"

// Device Configuration
#define USER_SLAVE_ADDRESS        0x11
#define USER_POLL_INTERVAL_MS     5000
#define USER_UPLOAD_INTERVAL_MS   30000
#define USER_BUFFER_SIZE          10

#endif // USER_CONFIG_H
