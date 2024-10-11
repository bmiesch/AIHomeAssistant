import asyncio
from bleak import BleakClient
from random import choice

ADDRESSES = [
    "2DC0DCE8-324D-3A88-1DED-D455BFF46107",  # Replace with your first device's MAC address
    "18FC4642-ADEA-7B0A-39BE-AB6C8961C82C"   # Replace with your second device's MAC address
]
UUID = "0000fff3-0000-1000-8000-00805f9b34fb"  # Replace with the correct characteristic UUID

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
    def __init__(self, address, uuid):
        self.address = address
        self.uuid = uuid
        self.client = None

    async def connect(self):
        self.client = BleakClient(self.address)
        await self.client.connect()
        print(f"Connected to {self.address}")

    async def send_msg(self, cmd_length, action, datatype, data_a, data_b, data_c, additional):
        data = bytes.fromhex(f"7e{cmd_length}{action}{datatype}{data_a}{data_b}{data_c}{additional}ef")
        await self.client.write_gatt_char(self.uuid, data)

    async def turn_on(self):
        await self.send_msg("07", "04", "ff", "00", "01", "02", "01")

    async def turn_off(self):
        await self.set_colour_rgb(*COLORS["black"])

    async def set_colour_hex(self, r, g, b):
        await self.send_msg("07", "05", "03", r, g, b, "10")

    async def set_colour_rgb(self, r, g, b):
        r_hex = f"{r:02x}"
        g_hex = f"{g:02x}"
        b_hex = f"{b:02x}"
        await self.set_colour_hex(r_hex, g_hex, b_hex)

    async def print_service_characteristics(self):
        services = await self.client.get_services()
        for service in services:
            print(f"Service: {service}")
            for characteristic in service.characteristics:
                print(f"  Characteristic: {characteristic}")

async def control_lights(controller):
    await controller.connect()
    await controller.print_service_characteristics()
    await controller.turn_on()

    try:
        while True:
            # Set to default color (light blue)
            await controller.set_colour_rgb(*COLORS["light_blue"])
            await asyncio.sleep(0.25)
    except KeyboardInterrupt:
        print("Keyboard interrupt received. Turning off lights...")
    finally:
        await controller.turn_off()
        print("Lights turned off. Exiting...")

async def main():
    controllers = [ControllableLight(address, UUID) for address in ADDRESSES]
    await asyncio.gather(*(control_lights(controller) for controller in controllers))

if __name__ == "__main__":
    asyncio.run(main())