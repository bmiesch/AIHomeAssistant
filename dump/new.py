import asyncio
from bleak import BleakClient
from random import choice

ADDRESSES = [
    "2DC0DCE8-324D-3A88-1DED-D455BFF46107",  # Replace with your first device's MAC address
    "18FC4642-ADEA-7B0A-39BE-AB6C8961C82C"   # Replace with your second device's MAC address
]
UUID_FFF3 = "0000fff3-0000-1000-8000-00805f9b34fb"  # Replace with the correct characteristic UUID
UUID_FFF4 = "0000fff4-0000-1000-8000-00805f9b34fb"  # Replace with the correct characteristic UUID

# Color map
COLORS = {
    "red": (255, 0, 0),
    "green": (0, 255, 0),
    "blue": (0, 0, 255),
    "yellow": (255, 255, 0),
    "cyan": (0, 255, 255),
    "magenta": (255, 0, 255),
    "white": (255, 255, 255),
    "black": (0, 0, 0),
    "light_blue": (0, 255, 255)  # Default color
}

class ControllableLight:
    def __init__(self, address, uuid_fff3, uuid_fff4):
        self.address = address
        self.uuid_fff3 = uuid_fff3
        self.uuid_fff4 = uuid_fff4
        self.client = None

    async def connect(self):
        self.client = BleakClient(self.address)
        await self.client.connect()
        print(f"Connected to {self.address}")

    async def send_msg(self, uuid, cmd_length, action, datatype, data_a, data_b, data_c, additional):
        data = bytes.fromhex(f"7e{cmd_length}{action}{datatype}{data_a}{data_b}{data_c}{additional}ef")
        await self.client.write_gatt_char(uuid, data)

    async def turn_on(self):
        await self.send_msg(self.uuid_fff3, "07", "04", "ff", "00", "01", "02", "01")

    async def turn_off(self):
        await self.set_colour_rgb(*COLORS["black"])

    async def set_colour_hex(self, r, g, b):
        await self.send_msg(self.uuid_fff3, "07", "05", "03", r, g, b, "10")

    async def set_colour_rgb(self, r, g, b):
        r_hex = f"{r:02x}"
        g_hex = f"{g:02x}"
        b_hex = f"{b:02x}"
        await self.set_colour_hex(r_hex, g_hex, b_hex)

    async def print_service_characteristics(self):
        for service in self.client.services:
            print(f"Service: {service}")
            for char in service.characteristics:
                print(f"  Characteristic: {char}")

    async def read_fff4(self):
        try:
            value = await self.client.read_gatt_char(self.uuid_fff4)
            print(f"Read from FFF4: {value}")
        except Exception as e:
            print(f"Failed to read from FFF4: {e}")

    async def write_fff4(self, data):
        try:
            await self.client.write_gatt_char(self.uuid_fff4, data)
            print(f"Wrote to FFF4: {data}")
        except Exception as e:
            print(f"Failed to write to FFF4: {e}")

async def control_lights(controller):
    await controller.connect()
    await controller.print_service_characteristics()  # Print services and characteristics
    await controller.read_fff4()  # Read from FFF4
    await controller.write_fff4(bytearray([0x01]))  # Write to FFF4 with example data
    await controller.turn_on()

    try:
        while True:
            # Set to default color (light blue)
            await controller.set_colour_rgb(*COLORS["light_blue"])
            await asyncio.sleep(0.25)
    except KeyboardInterrupt:
        print(f"Keyboard interrupt received. Turning off lights for {controller.address}...")
    finally:
        await controller.turn_off()
        print(f"Lights turned off for {controller.address}. Exiting...")

async def main():
    controllers = [ControllableLight(address, UUID_FFF3, UUID_FFF4) for address in ADDRESSES]
    try:
        await asyncio.gather(*(control_lights(controller) for controller in controllers))
    except KeyboardInterrupt:
        print("Keyboard interrupt received. Turning off all lights...")
        await asyncio.gather(*(controller.turn_off() for controller in controllers))
        print("All lights turned off. Exiting...")

if __name__ == "__main__":
    asyncio.run(main())