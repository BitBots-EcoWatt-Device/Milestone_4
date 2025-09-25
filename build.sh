#!/bin/bash

# ESP8266 Firmware Build and Flash Script
# This script builds and flashes the firmware to the ESP8266 device

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if PlatformIO is installed
check_platformio() {
    if ! command -v pio &> /dev/null; then
        print_error "PlatformIO is not installed or not in PATH"
        print_status "Please install PlatformIO: https://platformio.org/install"
        exit 1
    fi
    print_success "PlatformIO found"
}

# Build the firmware
build_firmware() {
    print_status "Building ESP8266 firmware..."
    
    if pio run; then
        print_success "Firmware built successfully"
    else
        print_error "Failed to build firmware"
        exit 1
    fi
}

# Flash the firmware
flash_firmware() {
    print_status "Flashing firmware to ESP8266..."
    print_warning "Make sure your ESP8266 is connected via USB"
    
    if pio run --target upload; then
        print_success "Firmware flashed successfully"
    else
        print_error "Failed to flash firmware"
        print_status "Try the following:"
        print_status "1. Check USB connection"
        print_status "2. Press reset button on ESP8266"
        print_status "3. Check if correct COM port is detected"
        exit 1
    fi
}

# Monitor serial output
monitor_serial() {
    print_status "Starting serial monitor..."
    print_status "Press Ctrl+C to exit monitor"
    print_status "Available commands: status, restart, test, upload, wifi, help"
    
    pio device monitor
}

# Main script
main() {
    print_status "ESP8266 BitBots EcoWatt Firmware Build Script"
    echo "=============================================="
    
    # Check prerequisites
    check_platformio
    
    # Parse command line arguments
    case "${1:-build}" in
        "build")
            build_firmware
            ;;
        "flash")
            build_firmware
            flash_firmware
            ;;
        "monitor")
            monitor_serial
            ;;
        "all")
            build_firmware
            flash_firmware
            monitor_serial
            ;;
        "clean")
            print_status "Cleaning build files..."
            pio run --target clean
            print_success "Build files cleaned"
            ;;
        *)
            echo "Usage: $0 [build|flash|monitor|all|clean]"
            echo ""
            echo "Commands:"
            echo "  build   - Build firmware only"
            echo "  flash   - Build and flash firmware"
            echo "  monitor - Open serial monitor"
            echo "  all     - Build, flash, and open monitor"
            echo "  clean   - Clean build files"
            echo ""
            echo "Default: build"
            exit 1
            ;;
    esac
}

# Run main function
main "$@"