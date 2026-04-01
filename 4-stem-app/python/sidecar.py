"""
sidecar.py — 4-Stem entry point. Started by Electron at launch.
Prints SIDECAR_PORT:<port> to stdout then starts Flask.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from server import start_server

if __name__ == "__main__":
    start_server()
