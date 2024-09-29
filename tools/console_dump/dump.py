import serial
import time
import sys
import argparse
import binascii


def connect_to_serial(port, baudrate=115200, timeout=0.1):
    try:
        ser = serial.Serial(port, baudrate, timeout=timeout)
        ser.write(b"\n")
        time.sleep(0.25)
        ser.reset_input_buffer()
        print(f"Connected to {port} at {baudrate} baudrate.")
        return ser
    except Exception as e:
        print(f"Error connecting to serial port: {e}")
        return None


def send_dump_command_byte_by_byte(ser, start_address):
    command = f"dump {hex(start_address)}\n"
    print(f"Sending command byte by byte: {command.strip()}")
    for byte in command:
        ser.write(byte.encode())
        time.sleep(0.01)


def read_response(ser):
    response = b""

    while True:
        line = ser.readline().decode(errors='ignore').strip()
        if line.startswith("addr"):
            break

    while True:
        line = ser.readline().decode(errors='ignore').strip()
        if line == ">":
            break

        try:
            bytes_line = binascii.unhexlify(line.replace(" ", ""))
            response += bytes_line
        except binascii.Error as e:
            print(f"Error decoding line: {line}, {e}")
            return None

    return response


def save_dump_to_file_binary(dump_data, filename="memory_dump.bin"):
    with open(filename, 'wb') as f:
        f.write(dump_data)
    print(f"Binary dump saved to {filename}")


def dump_memory(port, start_address, size, output_file="memory_dump.bin"):
    ser = connect_to_serial(port)
    if ser is None:
        return False

    try:
        total_dump = b""
        end_address = start_address + size
        current_address = start_address

        while current_address < end_address:
            send_dump_command_byte_by_byte(ser, current_address)
            dump_data = read_response(ser)
            if dump_data is None:
                print(f"Error: No valid response for address {hex(current_address)}")
                return False
            print(f"Received data for {hex(current_address)}")
            total_dump += dump_data
            current_address += 0x100

        save_dump_to_file_binary(total_dump, output_file)
        return True

    except Exception as e:
        print(f"Error during memory dump: {e}")
        return False
    finally:
        ser.close()


def validate_address(address):
    if address % 4 != 0:
        print(f"Error: Address {hex(address)} is not aligned to 4 bytes.")
        return False
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Memory dump utility over UART")
    parser.add_argument("--addr", type=lambda x: int(x, 0), required=True,
                        help="Starting memory address (must be aligned to 4 bytes)")
    parser.add_argument("--size", type=lambda x: int(x, 0), required=True,
                        help="Size of memory to dump in bytes")
    parser.add_argument("--port", type=str, default='/dev/ttyUSB0', help="UART port (e.g., COM3 or /dev/ttyUSB0)")
    parser.add_argument("--output", "-o", type=str, default="memory_dump.bin", help="Output file for memory dump")

    args = parser.parse_args()

    if not validate_address(args.addr):
        sys.exit(-1)

    status = dump_memory(args.port, args.addr, args.size, args.output)
    if not status:
        sys.exit(-1)
