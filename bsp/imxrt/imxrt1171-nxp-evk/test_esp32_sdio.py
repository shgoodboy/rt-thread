#!/usr/bin/env python3
"""
ESP32 SDIO Test Script for IMXRT1171
This script helps test and validate the ESP32 SDIO AT command implementation.
"""

import serial
import time
import sys
import argparse

class ESP32SDIOTester:
    def __init__(self, port, baudrate=115200):
        """Initialize the tester with serial connection parameters."""
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        
    def connect(self):
        """Connect to the serial port."""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=5)
            print(f"Connected to {self.port} at {self.baudrate} baud")
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from the serial port."""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("Disconnected")
    
    def send_command(self, command, wait_time=2):
        """Send a command and wait for response."""
        if not self.ser or not self.ser.is_open:
            print("Not connected to serial port")
            return None
            
        print(f">> {command}")
        self.ser.write(f"{command}\r\n".encode())
        time.sleep(wait_time)
        
        response = ""
        while self.ser.in_waiting:
            response += self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
            time.sleep(0.1)
        
        print(f"<< {response.strip()}")
        return response
    
    def test_basic_functionality(self):
        """Test basic ESP32 SDIO functionality."""
        print("\n=== Testing Basic ESP32 SDIO Functionality ===")
        
        # Test device initialization
        print("\n1. Testing ESP32 SDIO device...")
        response = self.send_command("esp32_at AT")
        if "OK" in response or "esp32_sdio" in response:
            print("✓ ESP32 SDIO device responding")
        else:
            print("✗ ESP32 SDIO device not responding")
            return False
        
        # Test firmware version
        print("\n2. Getting firmware version...")
        response = self.send_command("esp32_at AT+GMR")
        if "AT version" in response or "SDK version" in response:
            print("✓ Firmware version retrieved")
        else:
            print("✗ Failed to get firmware version")
        
        # Test WiFi mode setting
        print("\n3. Setting WiFi mode...")
        response = self.send_command("esp32_at AT+CWMODE=1")
        if "OK" in response:
            print("✓ WiFi mode set to Station")
        else:
            print("✗ Failed to set WiFi mode")
        
        # Test WiFi scan
        print("\n4. Scanning WiFi networks...")
        response = self.send_command("esp32_at AT+CWLAP", wait_time=5)
        if "+CWLAP:" in response or "OK" in response:
            print("✓ WiFi scan completed")
        else:
            print("✗ WiFi scan failed")
        
        return True
    
    def test_demo_commands(self):
        """Test the demo commands."""
        print("\n=== Testing Demo Commands ===")
        
        # Test demo function
        print("\n1. Running ESP32 SDIO demo...")
        response = self.send_command("esp32_sdio_demo", wait_time=10)
        if "ESP32 SDIO AT Command Demo" in response:
            print("✓ Demo executed successfully")
        else:
            print("✗ Demo execution failed")
        
        return True
    
    def test_wifi_connection(self, ssid=None, password=None):
        """Test WiFi connection if credentials provided."""
        if not ssid or not password:
            print("\n=== Skipping WiFi Connection Test (no credentials) ===")
            return True
            
        print(f"\n=== Testing WiFi Connection to {ssid} ===")
        
        # Connect to WiFi
        response = self.send_command(f"esp32_wifi {ssid} {password}", wait_time=10)
        if "WiFi connection successful" in response:
            print("✓ WiFi connection successful")
            
            # Get IP address
            time.sleep(2)
            response = self.send_command("esp32_at AT+CIFSR")
            if "STAIP" in response:
                print("✓ IP address obtained")
            else:
                print("✗ Failed to get IP address")
        else:
            print("✗ WiFi connection failed")
        
        return True
    
    def run_all_tests(self, ssid=None, password=None):
        """Run all tests."""
        print("ESP32 SDIO Test Suite for IMXRT1171")
        print("=" * 40)
        
        if not self.connect():
            return False
        
        try:
            # Wait for system to be ready
            print("Waiting for system initialization...")
            time.sleep(3)
            
            # Send a few enters to get to shell prompt
            self.send_command("", wait_time=1)
            self.send_command("", wait_time=1)
            
            # Run tests
            success = True
            success &= self.test_basic_functionality()
            success &= self.test_demo_commands()
            success &= self.test_wifi_connection(ssid, password)
            
            if success:
                print("\n🎉 All tests completed successfully!")
            else:
                print("\n❌ Some tests failed. Check the output above.")
                
        finally:
            self.disconnect()
        
        return success

def main():
    parser = argparse.ArgumentParser(description='Test ESP32 SDIO implementation on IMXRT1171')
    parser.add_argument('port', help='Serial port (e.g., COM3, /dev/ttyUSB0)')
    parser.add_argument('-b', '--baudrate', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('-s', '--ssid', help='WiFi SSID for connection test')
    parser.add_argument('-p', '--password', help='WiFi password for connection test')
    
    args = parser.parse_args()
    
    tester = ESP32SDIOTester(args.port, args.baudrate)
    success = tester.run_all_tests(args.ssid, args.password)
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()