import subprocess
import shlex
import serial.tools.list_ports
import os

# Function to find the ACM port dynamically
def find_owntech_port():
    # Get a list of all boards with Vendor ID 2FE3 (only way to have it working on windows.)
    available_ports = list(serial.tools.list_ports.grep("2FE3"))
    
    #TO DO, improve, for now only return first owntech board found.
    for port in available_ports:
        return port.device
    return None

# Find the ACM port and USB device
owntech_port = find_owntech_port()

if owntech_port:
    print(f"Found OwnTech board on port: {owntech_port}")
    # Adjust the mcumgr executable and binary path for Windows
    mcumgr_executable = "./mcumgr" if os.name == 'posix' else ".\\mcumgr.exe"
    binary_path = "./.pio/build/spin/zephyr.signed.bin" if os.name == 'posix' else ".\\build\\zephyr\\zephyr.signed.bin"

    # Initialize serial port
    # init_command = [mcumgr_executable, "conn", "add", "serial", 'type="serial"', f'connstring=dev={owntech_port}, baud=115200, mtu=128']
    init_command = f'conn add serial type="serial" connstring="dev={owntech_port},baud=115200,mtu=128"'
    # Weird bug when trying to define a list directly, it works using shlex and I don't understand why...
    parsed = shlex.split(init_command)
    # Only way found to add .\ in the path for windows
    parsed.insert(0, mcumgr_executable)
    try:
        # Execute the command
        subprocess.run(parsed, check=True)
        print("mcumgr command executed successfully.")
    except subprocess.CalledProcessError as e:
        print(f"Error executing mcumgr command: {e}")
    except FileNotFoundError:
        print("mcumgr executable not found. Make sure it's in your PATH.")

    # Define the command and arguments
    command = [mcumgr_executable, "-c", "serial", "image", "upload", binary_path]
    reset = [mcumgr_executable, "-c", "serial", "reset"]
    hash = [mcumgr_executable, "-c", "serial", "image", "list"]
    try:
        # Execute the command
        subprocess.run(command, check=True)
        print("Flashing successful")
        try : 
            mcumgr_output = subprocess.check_output(hash, stderr=subprocess.STDOUT, universal_newlines=True)            
            # Split the output into lines
            lines = mcumgr_output.split('\n')

            # Initialize variables to store image information
            image_slot_1_hash = None
            image_slot_0_hash = None

            # Iterate through the lines to extract the hash of image 0 slot 1
            for i in range(len(lines)):
                if "image=0 slot=0" in lines[i]:
                    for j in range(i, len(lines)):
                        if "hash:" in lines[j]:
                            image_slot_0_hash = lines[j].split("hash:")[1].strip()
                            break
            for i in range(len(lines)):
                if "image=0 slot=1" in lines[i]:
                    for j in range(i, len(lines)):
                        if "hash:" in lines[j]:
                            image_slot_1_hash = lines[j].split("hash:")[1].strip()
                            break

            # Check if the hash was found
            if image_slot_1_hash:
                print("Image 0 Slot 1 Hash:", image_slot_1_hash)
                print("Marking Image 0 Slot 1 for testing")
                test = [mcumgr_executable, "-c", "serial", "image", "test"]
                test.append(image_slot_1_hash)
                try : 
                    subprocess.run(test, check=True)
                except subprocess.CalledProcessError as e:
                    print(f"Error executing test command: {e}")
            else:
                print("Image 0 Slot 1 empty. Proceding to mark Slot 0 for testing")
                print("Image 0 Slot 0 Hash:", image_slot_0_hash)
                test = [mcumgr_executable, "-c", "serial", "image", "test"]
                test.append(image_slot_0_hash)
                try : 
                    subprocess.run(test, check=True)
                except subprocess.CalledProcessError as e:
                    print(f"Error executing test command: {e}")
        except subprocess.CalledProcessError as e:
            print(f"Error executing list command: {e}")
        print("resetting..")
        try :   
            subprocess.run(reset, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error executing reset command: {e}")
    except subprocess.CalledProcessError as e:
        print(f"Error executing mcumgr command: {e}")
    except FileNotFoundError:
        print("mcumgr executable not found. Make sure it's in your PATH.")
else:
    print("No valid USB port found")
