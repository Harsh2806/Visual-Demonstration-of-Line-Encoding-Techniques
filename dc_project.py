import serial
import time

# Configure the serial port
ser = serial.Serial(
    port='/dev/ttyACM0',  # Confirmed Arduino port
    baudrate=9600,        # Must match Arduino's Serial.begin()
    timeout=1             # Read timeout in seconds
)

# Line coding options
LINE_CODING_SCHEMES = {
    'NRZ': 1,      # Non-Return-to-Zero
    'NRZI': 2,     # Non-Return-to-Zero Inverted
    'Manchester': 3, # Manchester coding
    'AMI': 4       # Alternate Mark Inversion
}

# Function to convert ASCII text to binary
def text_to_binary(text):
    binary = ""
    for char in text:
        # Convert each character to its ASCII value, then to binary
        # Remove '0b' prefix and pad to 8 bits
        binary_char = bin(ord(char))[2:].zfill(8)
        binary += binary_char
    return binary

# Function to encode data using NRZ (Non-Return-to-Zero)
def encode_nrz(data):
    encoded = ""
    for bit in data:
        if bit == '1':
            encoded += '1'  # High voltage for 1
        else:
            encoded += '0'  # Low voltage for 0
    return encoded

# Function to encode data using NRZI (Non-Return-to-Zero Inverted)
def encode_nrzi(data):
    encoded = ""
    current_state = '0'  # Start with low state
    
    for bit in data:
        if bit == '1':
            # Toggle state
            current_state = '1' if current_state == '0' else '0'
        # If bit is 0, state remains the same
        encoded += current_state
    
    return encoded

# Function to encode data using Manchester coding
def encode_manchester(data):
    encoded = ""
    for bit in data:
        if bit == '1':
            encoded += "10"  # 1 is encoded as high-to-low transition
        else:
            encoded += "01"  # 0 is encoded as low-to-high transition
    return encoded

# Function to encode data using AMI (Alternate Mark Inversion)
def encode_ami(data):
    encoded = ""
    last_polarity = 1  # Start with positive polarity
    
    for bit in data:
        if bit == '0':
            encoded += '0'  # Zero is always zero
        else:
            # Alternate between positive and negative for 1s
            last_polarity = -last_polarity
            encoded += '1' if last_polarity > 0 else '2'  # Use '2' to represent negative voltage
    
    return encoded

# Wait for the serial connection to establish
time.sleep(2)
print("Starting line coding transmission...")

# Select coding scheme
coding_scheme = 'NRZ'  # Change this to use different coding schemes
scheme_id = LINE_CODING_SCHEMES[coding_scheme]

def send_message():
    # Get message from user
    text_message = input("Enter message to send (or 'quit' to exit): ")
    
    if text_message.lower() == 'quit':
        return False
        
    # Convert text to binary
    binary_data = text_to_binary(text_message)
    
    # Encode the binary data according to selected scheme
    if coding_scheme == 'NRZ':
        encoded_data = encode_nrz(binary_data)
    elif coding_scheme == 'NRZI':
        encoded_data = encode_nrzi(binary_data)
    elif coding_scheme == 'Manchester':
        encoded_data = encode_manchester(binary_data)
    elif coding_scheme == 'AMI':
        encoded_data = encode_ami(binary_data)
    
    # Prepare message to send to Arduino
    # Format: S<scheme_id>:<original_text>:<binary_data>:<encoded_data>E
    message = f"S{scheme_id}:{text_message}:{binary_data}:{encoded_data}E\n"
    
    # Send the message
    ser.write(message.encode())
    print(f"Sent message: {text_message}")
    print(f"Binary: {binary_data}")
    print(f"Encoded ({coding_scheme}): {encoded_data}")
    
    # Check for acknowledgment
    try:
        response = ser.readline().decode().strip()
        if response:
            print(f"Arduino response: {response}")
    except:
        print("No response from Arduino")
    
    return True

try:
    running = True
    while running:
        running = send_message()
        time.sleep(1)  # Small delay before prompt
        
    print("Exiting program...")
    ser.close()
except KeyboardInterrupt:
    print("\nStopping...")
    ser.close()  # Close the serial port
except serial.SerialException as e:
    print(f"Serial error: {e}")
    ser.close()