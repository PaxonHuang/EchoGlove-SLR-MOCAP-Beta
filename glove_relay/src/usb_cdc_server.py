"""USB CDC serial input server for P4 base station data."""
from __future__ import annotations
import asyncio
import logging
from typing import Optional, Callable

logger = logging.getLogger(__name__)


class USBCDCServer:
    """Async serial reader that receives raw bytes from the P4 base station over USB CDC."""

    def __init__(self, port: str = "/dev/ttyACM0", baud: int = 2000000):
        self.port = port
        self.baud = baud
        self._running = False
        self._on_data: Optional[Callable[[bytes], None]] = None

    def set_callback(self, callback: Callable[[bytes], None]):
        """Register a callback invoked with each chunk of raw bytes received."""
        self._on_data = callback

    async def start(self):
        """Open the serial port and read data in a loop until stopped."""
        self._running = True
        try:
            import serial_asyncio

            reader, _ = await serial_asyncio.open_serial_connection(
                url=self.port, baudrate=self.baud
            )
            logger.info("USB CDC connected: %s @ %d", self.port, self.baud)
            while self._running:
                data = await reader.read(4096)
                if data and self._on_data:
                    self._on_data(data)
        except Exception as e:
            logger.error("USB CDC error: %s", e)
            self._running = False

    def stop(self):
        """Signal the read loop to exit."""
        self._running = False
