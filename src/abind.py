#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path
from pynput import keyboard

CONFIG_DIR = Path.home() / ".config" / ".abind"
CONFIG_FILE = CONFIG_DIR / "conf"

def ensure_config_exists():
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)
    if not CONFIG_FILE.exists():
        CONFIG_FILE.write_text("""[Ctrl+g]
firefox
[Ctrl+d]
dmenu
""")

def load_config():
    config = {}
    current_key = None
    if CONFIG_FILE.exists():
        with CONFIG_FILE.open() as f:
            for line in f:
                line = line.strip()
                if line.startswith('[') and line.endswith(']'):
                    current_key = line[1:-1]
                elif current_key and line and not line.startswith('#'):
                    config[current_key] = line
                    current_key = None
    return config

class KeybindDaemon:
    def __init__(self):
        self.pressed_keys = set()
        self.key_mapping = {
            'ctrl': keyboard.Key.ctrl,
            'shift': keyboard.Key.shift,
            'alt': keyboard.Key.alt,
            'super': keyboard.Key.cmd
        }
        self.active_combos = self.load_combinations()

    def load_combinations(self):
        combos = []
        config = load_config()
        for combo_str, command in config.items():
            keys = []
            try:
                for part in combo_str.lower().split('+'):
                    part = part.strip()
                    keys.append(self.key_mapping.get(part, part))
                combos.append({'keys': keys, 'command': command})
            except Exception:
                continue
        return combos

    def is_combo_pressed(self, combo):
        return all(
            (k in self.pressed_keys) if isinstance(k, str) else (k in self.pressed_keys)
            for k in combo['keys']
        )

    def on_press(self, key):
        try:
            char = key.char if hasattr(key, 'char') else None
            self.pressed_keys.add(char if char else key)
            
            for combo in self.active_combos:
                if self.is_combo_pressed(combo):
                    subprocess.Popen(combo['command'], shell=True)
                    break
        except Exception:
            pass

    def on_release(self, key):
        try:
            char = key.char if hasattr(key, 'char') else None
            self.pressed_keys.discard(char if char else key)
        except Exception:
            pass

    def run(self):
        with keyboard.Listener(
            on_press=self.on_press,
            on_release=self.on_release
        ) as listener:
            listener.join()

if __name__ == "__main__":
    ensure_config_exists()
    daemon = KeybindDaemon()
    daemon.run()
