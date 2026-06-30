import threading
import math
import re
import tkinter as tk
from tkinter import ttk, messagebox

try:
    import serial
    from serial.tools import list_ports
except Exception as exc:
    raise SystemExit(
        "pyserial is required. Install with: pip install pyserial\n"
        f"Import error: {exc}"
    )

BAUD = 115200
MAX_STEP = 500
SPEED_MAX = 90.0
RPM_MAX = 9000.0
STEP_DISPLAY_MAX = 1200
STEP_PRESETS = (2, 5, 10, 50, 100)

BACKGROUND = "#dfe3ea"
CARD_BG = "#f8f9fb"
CARD_BORDER = "#b8bfca"
ACCENT = "#f07f2f"
TEXT_DARK = "#1f2a3a"
TEXT_MUTED = "#596578"


class GaugeDial:
    def __init__(self, parent, title: str, min_label: str, max_label: str):
        self.value = 0.1
        self.canvas = tk.Canvas(
            parent,
            width=260,
            height=220,
            bg=CARD_BG,
            highlightthickness=0,
            bd=0,
        )
        self.canvas.create_text(130, 16, text=title, fill=TEXT_DARK, font=("Segoe UI", 12, "bold"))

        self.cx = 130
        self.cy = 122
        self.r = 92
        self.canvas.create_oval(
            self.cx - self.r,
            self.cy - self.r,
            self.cx + self.r,
            self.cy + self.r,
            outline="#22252c",
            width=3,
        )
        self.canvas.create_oval(
            self.cx - self.r + 6,
            self.cy - self.r + 6,
            self.cx + self.r - 6,
            self.cy + self.r - 6,
            outline="#9ea8b7",
            width=1,
        )

        for tick in range(0, 11):
            ratio = tick / 10.0
            angle_deg = 210.0 - (ratio * 180.0)
            angle = math.radians(angle_deg)
            outer_x = self.cx + math.cos(angle) * (self.r - 8)
            outer_y = self.cy - math.sin(angle) * (self.r - 8)
            inner_len = 13 if tick % 2 == 0 else 8
            inner_x = self.cx + math.cos(angle) * (self.r - 8 - inner_len)
            inner_y = self.cy - math.sin(angle) * (self.r - 8 - inner_len)
            self.canvas.create_line(inner_x, inner_y, outer_x, outer_y, fill="#8893a3", width=2 if tick % 2 == 0 else 1)

        self.canvas.create_text(self.cx - 48, self.cy + 72, text=min_label, fill=TEXT_DARK, font=("Segoe UI", 11))
        self.canvas.create_text(self.cx + 54, self.cy + 40, text=max_label, fill=TEXT_DARK, font=("Segoe UI", 11))
        self.canvas.create_arc(
            self.cx - self.r + 10,
            self.cy - self.r + 10,
            self.cx + self.r - 10,
            self.cy + self.r - 10,
            start=200,
            extent=180,
            style="arc",
            width=2,
            outline="#c6ccd6",
        )

        self.needle = self.canvas.create_line(0, 0, 0, 0, fill=ACCENT, width=4, capstyle="round")
        self.hub = self.canvas.create_oval(122, 114, 138, 130, fill="#ffffff", outline="#22252c", width=2)
        self.step_readout = self.canvas.create_text(
            self.cx,
            self.cy + 64,
            text="0 / 1200 steps",
            fill=TEXT_MUTED,
            font=("Segoe UI", 10, "bold"),
        )
        self.set_value(0.0)

    def set_value(self, ratio: float):
        self.value = max(0.0, min(1.0, ratio))
        # Sweep from lower-left (0.0) to upper-right (1.0).
        angle_deg = 210.0 - (self.value * 180.0)
        angle = math.radians(angle_deg)
        x2 = self.cx + math.cos(angle) * (self.r - 26)
        y2 = self.cy - math.sin(angle) * (self.r - 26)
        self.canvas.coords(self.needle, self.cx, self.cy, x2, y2)
        self.canvas.tag_raise(self.hub)
        self.set_step_display(int(round(self.value * STEP_DISPLAY_MAX)))

    def set_step_display(self, steps: int):
        if steps < 0:
            steps = 0
        if steps > STEP_DISPLAY_MAX:
            steps = STEP_DISPLAY_MAX
        self.canvas.itemconfigure(self.step_readout, text=f"{steps} / {STEP_DISPLAY_MAX} steps")


class GaugeSerialGui:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Gauge Serial Control")
        self.ser = None
        self.reader_stop = threading.Event()

        self.port_var = tk.StringVar()
        self.step_var = tk.StringVar(value="50")
        self.increment_var = tk.IntVar(value=50)
        self.quick_cmd_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Disconnected")
        self.left_visual_ratio = 0.0
        self.right_visual_ratio = 0.0
        self.left_travel_limit = 1200.0
        self.right_travel_limit = 1200.0
        self.config_mode = False

        self.config_buttons = []
        self.config_entries = []
        self.mode_indicator_var = tk.StringVar(value="RUN")

        self._build_ui()
        self._refresh_ports()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_ui(self):
        self.root.geometry("1120x760")
        self.root.minsize(960, 680)
        self.root.configure(bg=BACKGROUND)
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure("Panel.TFrame", background=BACKGROUND)
        style.configure("Card.TFrame", background=CARD_BG, relief="solid", borderwidth=1)
        style.configure("Header.TLabel", background=BACKGROUND, foreground=TEXT_DARK, font=("Segoe UI", 13, "bold"))
        style.configure("Body.TLabel", background=CARD_BG, foreground=TEXT_DARK, font=("Segoe UI", 10))
        style.configure("Muted.TLabel", background=CARD_BG, foreground=TEXT_MUTED, font=("Segoe UI", 9))
        style.configure("Status.TLabel", background=BACKGROUND, foreground=TEXT_DARK, font=("Segoe UI", 10, "bold"))
        style.configure("ModeRun.TLabel", background=BACKGROUND, foreground="#1f6f43", font=("Segoe UI", 11, "bold"))
        style.configure("ModeConfig.TLabel", background=BACKGROUND, foreground="#8a4f00", font=("Segoe UI", 11, "bold"))
        style.configure("Action.TButton", font=("Segoe UI", 10, "bold"), padding=6)

        shell = ttk.Frame(self.root, style="Panel.TFrame", padding=(14, 12, 14, 12))
        shell.grid(row=0, column=0, sticky="nsew")
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        shell.columnconfigure(0, weight=1)
        shell.rowconfigure(2, weight=1)

        top = ttk.Frame(shell, style="Panel.TFrame")
        top.grid(row=0, column=0, sticky="ew")

        ttk.Label(top, text="Gauge Setup Console", style="Header.TLabel").grid(row=0, column=0, sticky="w", padx=(2, 18))
        ttk.Label(top, text="Port:").grid(row=0, column=1, sticky="w")
        self.port_combo = ttk.Combobox(top, textvariable=self.port_var, width=26, state="readonly")
        self.port_combo.grid(row=0, column=2, sticky="we", padx=(4, 6))
        ttk.Button(top, text="Refresh", command=self._refresh_ports).grid(row=0, column=3, padx=2)
        self.connect_btn = ttk.Button(top, text="Connect", style="Action.TButton", command=self._toggle_connect)
        self.connect_btn.grid(row=0, column=4, padx=2)

        ttk.Label(top, text="Status:", style="Status.TLabel").grid(row=0, column=5, sticky="e", padx=(16, 2))
        ttk.Label(top, textvariable=self.status_var, style="Status.TLabel").grid(row=0, column=6, sticky="w")
        ttk.Label(top, text="Mode:", style="Status.TLabel").grid(row=0, column=7, sticky="e", padx=(10, 2))
        self.mode_indicator = ttk.Label(top, textvariable=self.mode_indicator_var, style="ModeRun.TLabel")
        self.mode_indicator.grid(row=0, column=8, sticky="w")

        for i in range(9):
            top.columnconfigure(i, weight=0)
        top.columnconfigure(2, weight=1)

        controls = ttk.Frame(shell, style="Panel.TFrame", padding=(0, 12, 0, 0))
        controls.grid(row=2, column=0, sticky="nsew")
        controls.columnconfigure(0, weight=1)
        controls.columnconfigure(1, weight=0)
        controls.columnconfigure(2, weight=1)
        controls.rowconfigure(1, weight=1)

        left_card = ttk.Frame(controls, style="Card.TFrame", padding=10)
        left_card.grid(row=0, column=0, sticky="nsew", padx=(0, 8))
        left_card.columnconfigure(0, weight=1)

        right_card = ttk.Frame(controls, style="Card.TFrame", padding=10)
        right_card.grid(row=0, column=2, sticky="nsew", padx=(8, 0))
        right_card.columnconfigure(0, weight=1)

        center_col = ttk.Frame(controls, style="Panel.TFrame")
        center_col.grid(row=0, column=1, sticky="ns", padx=4)

        ttk.Label(left_card, text="Speed Gauge", style="Body.TLabel", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, sticky="w")
        self.left_dial = GaugeDial(left_card, "MPH", "0", "1200")
        self.left_dial.canvas.grid(row=1, column=0, sticky="nsew", pady=4)
        self._add_config_button(left_card, "Check max position", self._left_max, row=2, column=0, sticky="ew", pady=(2, 3))
        self._add_config_button(left_card, "Set max position", self._left_set_max, row=3, column=0, sticky="ew", pady=(0, 3))
        self._add_config_button(left_card, "Check min position", self._left_zero, row=4, column=0, sticky="ew", pady=(0, 3))
        self._add_config_button(left_card, "Set min position", self._left_set_min, row=5, column=0, sticky="ew", pady=(0, 5))
        ttk.Label(left_card, text="Move increment", style="Muted.TLabel").grid(row=6, column=0, sticky="w", pady=(4, 0))
        left_move = ttk.Frame(left_card, style="Card.TFrame")
        left_move.grid(row=7, column=0, sticky="ew", pady=(1, 0))
        left_move.columnconfigure(0, weight=1)
        left_move.columnconfigure(1, weight=1)
        self._add_config_button(left_move, text="Move +", command=lambda: self._move_with_sign("ml", 1), row=0, column=0, sticky="ew", padx=(0, 3))
        self._add_config_button(left_move, text="Move -", command=lambda: self._move_with_sign("ml", -1), row=0, column=1, sticky="ew", padx=(3, 0))

        ttk.Label(right_card, text="Tach Gauge", style="Body.TLabel", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, sticky="w")
        self.right_dial = GaugeDial(right_card, "RPM", "0", "1200")
        self.right_dial.canvas.grid(row=1, column=0, sticky="nsew", pady=4)
        self._add_config_button(right_card, "Check max position", self._right_max, row=2, column=0, sticky="ew", pady=(2, 3))
        self._add_config_button(right_card, "Set max position", self._right_set_max, row=3, column=0, sticky="ew", pady=(0, 3))
        self._add_config_button(right_card, "Check min position", self._right_zero, row=4, column=0, sticky="ew", pady=(0, 3))
        self._add_config_button(right_card, "Set min position", self._right_set_min, row=5, column=0, sticky="ew", pady=(0, 5))
        ttk.Label(right_card, text="Move increment", style="Muted.TLabel").grid(row=6, column=0, sticky="w", pady=(4, 0))
        right_move = ttk.Frame(right_card, style="Card.TFrame")
        right_move.grid(row=7, column=0, sticky="ew", pady=(1, 0))
        right_move.columnconfigure(0, weight=1)
        right_move.columnconfigure(1, weight=1)
        self._add_config_button(right_move, text="Move +", command=lambda: self._move_with_sign("mr", 1), row=0, column=0, sticky="ew", padx=(0, 3))
        self._add_config_button(right_move, text="Move -", command=lambda: self._move_with_sign("mr", -1), row=0, column=1, sticky="ew", padx=(3, 0))

        setup_card = ttk.Frame(center_col, style="Card.TFrame", padding=10)
        setup_card.grid(row=0, column=0, sticky="ew")
        ttk.Label(setup_card, text="Configuration", style="Body.TLabel", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, sticky="w")
        ttk.Button(setup_card, text="Configure mode", command=self._enter_config_mode).grid(row=1, column=0, sticky="ew", pady=(8, 4))
        ttk.Button(setup_card, text="Run mode", command=self._enter_run_mode).grid(row=2, column=0, sticky="ew", pady=2)
        self._add_config_button(setup_card, text="Home needles", command=lambda: self._send("home"), row=3, column=0, sticky="ew", pady=2)
        self._add_config_button(setup_card, text="Write to EEPROM", command=lambda: self._send("write eeprom"), row=4, column=0, sticky="ew", pady=(2, 0))

        increment_card = ttk.Frame(center_col, style="Card.TFrame", padding=10)
        increment_card.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        ttk.Label(increment_card, text="Step increment", style="Body.TLabel", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, sticky="w")
        chips = ttk.Frame(increment_card, style="Card.TFrame")
        chips.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        for idx, step in enumerate(STEP_PRESETS):
            rb = ttk.Radiobutton(chips, text=str(step), value=step, variable=self.increment_var, command=self._sync_increment_field)
            rb.grid(row=0, column=idx, sticky="w", padx=(0, 4))
            self.config_buttons.append(rb)

        step_entry_row = ttk.Frame(increment_card, style="Card.TFrame")
        step_entry_row.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        ttk.Label(step_entry_row, text="Custom +/-", style="Muted.TLabel").grid(row=0, column=0, sticky="w")
        step_entry = ttk.Entry(step_entry_row, textvariable=self.step_var, width=8)
        step_entry.grid(row=0, column=1, sticky="w", padx=(6, 0))
        self.config_entries.append(step_entry)

        stream_card = ttk.Frame(center_col, style="Card.TFrame", padding=10)
        stream_card.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        ttk.Label(stream_card, text="Data Stream", style="Body.TLabel", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, sticky="w")
        self._add_config_button(stream_card, text="Show data on", command=lambda: self._send("show data on"), row=1, column=0, sticky="ew", pady=(8, 4))
        self._add_config_button(stream_card, text="Show data off", command=lambda: self._send("show data off"), row=2, column=0, sticky="ew", pady=2)
        self._add_config_button(stream_card, text="Help", command=lambda: self._send("help"), row=3, column=0, sticky="ew", pady=(2, 0))

        quick_card = ttk.Frame(center_col, style="Card.TFrame", padding=10)
        quick_card.grid(row=3, column=0, sticky="ew", pady=(8, 0))
        ttk.Label(quick_card, text="Quick command", style="Body.TLabel", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, sticky="w")
        cmd_entry = ttk.Entry(quick_card, textvariable=self.quick_cmd_var, width=18)
        cmd_entry.grid(row=1, column=0, sticky="ew", pady=(8, 4))
        cmd_entry.bind("<Return>", self._send_quick_command)
        self.config_entries.append(cmd_entry)
        self._add_config_button(quick_card, text="Send", command=self._send_quick_command, row=2, column=0, sticky="ew")

        log_card = ttk.Frame(controls, style="Card.TFrame", padding=(8, 8, 8, 8))
        log_card.grid(row=1, column=0, columnspan=3, sticky="nsew", pady=(8, 0))
        log_card.columnconfigure(0, weight=1)
        log_card.rowconfigure(1, weight=1)
        ttk.Label(log_card, text="Serial log", style="Body.TLabel", font=("Segoe UI", 10, "bold")).grid(row=0, column=0, sticky="w", pady=(0, 4))

        self.log = tk.Text(log_card, height=11, wrap="word", bg="#fbfcfe", fg=TEXT_DARK, insertbackground=TEXT_DARK, relief="flat")
        self.log.grid(row=1, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(controls, orient="vertical", command=self.log.yview)
        scroll.grid(row=1, column=3, sticky="ns", pady=(8, 0))
        self.log.configure(yscrollcommand=scroll.set)

        self._set_config_mode(False)
        self._log("Ready. Enter configuration mode to use calibration/move controls.")

    def _add_config_button(self, parent, text, command, **grid_kwargs):
        btn = ttk.Button(parent, text=text, command=command)
        btn.grid(**grid_kwargs)
        self.config_buttons.append(btn)
        return btn

    def _set_config_mode(self, enabled: bool):
        self.config_mode = enabled
        mode = "normal" if enabled else "disabled"
        for btn in self.config_buttons:
            btn.configure(state=mode)
        for entry in self.config_entries:
            entry.configure(state=mode)

        if enabled:
            self.mode_indicator_var.set("CONFIG")
            self.mode_indicator.configure(style="ModeConfig.TLabel")
        else:
            self.mode_indicator_var.set("RUN")
            self.mode_indicator.configure(style="ModeRun.TLabel")

    def _enter_config_mode(self):
        if not self._send("setup on"):
            return
        self._set_config_mode(True)

    def _enter_run_mode(self):
        if not self._send("setup off"):
            return
        self._set_config_mode(False)
        # Keep telemetry on in run mode so the dials can follow live values.
        self._send("show data on")

    def _sync_increment_field(self):
        self.step_var.set(str(self.increment_var.get()))

    def _refresh_ports(self):
        ports = [p.device for p in list_ports.comports()]
        self.port_combo["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def _send_quick_command(self, _event=None):
        if not self.config_mode:
            messagebox.showinfo("Run Mode", "Switch to Configure mode to send manual commands.")
            return
        cmd = self.quick_cmd_var.get().strip()
        if not cmd:
            return
        self._send(cmd)
        self.quick_cmd_var.set("")

    def _toggle_connect(self):
        if self.ser and self.ser.is_open:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        port = self.port_var.get().strip()
        if not port:
            messagebox.showerror("Port Required", "Select a COM port first.")
            return
        try:
            self.ser = serial.Serial(port, BAUD, timeout=0.1)
            self.reader_stop.clear()
            threading.Thread(target=self._reader_loop, daemon=True).start()
            self.status_var.set(f"Connected @ {port}")
            self.connect_btn.configure(text="Disconnect")
            self._log(f"Connected to {port} @ {BAUD}")
            self._set_config_mode(False)
            self._send("setup")
        except Exception as exc:
            messagebox.showerror("Connect Failed", str(exc))

    def _disconnect(self):
        self.reader_stop.set()
        try:
            if self.ser:
                self.ser.close()
        except Exception:
            pass
        self.ser = None
        self.status_var.set("Disconnected")
        self.connect_btn.configure(text="Connect")
        self._set_config_mode(False)
        self._log("Disconnected")

    def _reader_loop(self):
        while not self.reader_stop.is_set() and self.ser and self.ser.is_open:
            try:
                data = self.ser.read(self.ser.in_waiting or 1)
                if data:
                    text = data.decode(errors="replace")
                    self.root.after(0, self._log, text, False)
            except Exception as exc:
                self.root.after(0, self._log, f"\n[Serial read error] {exc}\n")
                break

    def _parse_steps(self):
        raw = self.step_var.get().strip()
        try:
            value = int(raw)
        except ValueError:
            raise ValueError("Step value must be an integer.")
        if value > MAX_STEP or value < -MAX_STEP:
            raise ValueError(f"Step value must be between -{MAX_STEP} and {MAX_STEP}.")
        return value

    def _move_with_sign(self, command_prefix: str, sign: int):
        if not self.config_mode:
            messagebox.showinfo("Run Mode", "Switch to Configure mode to move needles manually.")
            return
        try:
            steps = self._parse_steps()
        except ValueError as exc:
            messagebox.showerror("Invalid Step", str(exc))
            return
        magnitude = abs(steps)
        # Invert GUI button sign mapping to match current firmware motor polarity.
        value = -(magnitude * sign)
        self._send(f"{command_prefix}{value}")
        default_delta = float(magnitude) / float(MAX_STEP * 5)
        delta = default_delta if value > 0 else -default_delta
        if command_prefix == "ml":
            self.left_visual_ratio = max(0.0, min(1.0, self.left_visual_ratio + delta))
            self.left_dial.set_value(self.left_visual_ratio)
        else:
            self.right_visual_ratio = max(0.0, min(1.0, self.right_visual_ratio + delta))
            self.right_dial.set_value(self.right_visual_ratio)

    def _left_zero(self):
        if not self.config_mode:
            return
        self._send("lzero")
        self.left_visual_ratio = 0.0
        self.left_dial.set_value(self.left_visual_ratio)

    def _left_max(self):
        if not self.config_mode:
            return
        self._send("lmax")
        self.left_visual_ratio = 1.0
        self.left_dial.set_value(self.left_visual_ratio)

    def _left_set_min(self):
        if not self.config_mode:
            return
        self._send("set min left")

    def _left_set_max(self):
        if not self.config_mode:
            return
        self._send("set max left")

    def _right_zero(self):
        if not self.config_mode:
            return
        self._send("rzero")
        self.right_visual_ratio = 0.0
        self.right_dial.set_value(self.right_visual_ratio)

    def _right_max(self):
        if not self.config_mode:
            return
        self._send("rmax")
        self.right_visual_ratio = 1.0
        self.right_dial.set_value(self.right_visual_ratio)

    def _right_set_min(self):
        if not self.config_mode:
            return
        self._send("set min right")

    def _right_set_max(self):
        if not self.config_mode:
            return
        self._send("set max right")

    def _send(self, command: str):
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Not Connected", "Connect to a COM port first.")
            return False
        try:
            self.ser.write((command + "\n").encode())
            self._log(f"> {command}\n", False)
            return True
        except Exception as exc:
            messagebox.showerror("Send Failed", str(exc))
            return False

    def _try_update_dials_from_serial_text(self, text: str):
        lowered = text.lower()
        if "setup mode enabled" in lowered or "setup mode is on" in lowered:
            self._set_config_mode(True)
        elif "setup mode disabled" in lowered or "setup mode is off" in lowered:
            self._set_config_mode(False)

        if "homing complete" in lowered:
            self.left_visual_ratio = 0.0
            self.right_visual_ratio = 0.0
            self.left_dial.set_value(0.0)
            self.right_dial.set_value(0.0)

        if "left gauge -> zero" in text:
            self.left_visual_ratio = 0.0
            self.left_dial.set_value(self.left_visual_ratio)
        elif "left gauge -> max" in text:
            self.left_visual_ratio = 1.0
            self.left_dial.set_value(self.left_visual_ratio)

        if "right gauge -> zero" in text:
            self.right_visual_ratio = 0.0
            self.right_dial.set_value(self.right_visual_ratio)
        elif "right gauge -> max" in text:
            self.right_visual_ratio = 1.0
            self.right_dial.set_value(self.right_visual_ratio)

        left_raw_match = re.search(r"left\s*->\s*(\d+)", text)
        if left_raw_match:
            left_raw = float(left_raw_match.group(1))
            denom = self.left_travel_limit if self.left_travel_limit > 0 else 1200.0
            self.left_visual_ratio = max(0.0, min(1.0, left_raw / denom))
            self.left_dial.set_value(self.left_visual_ratio)

        right_raw_match = re.search(r"right\s*->\s*(\d+)", text)
        if right_raw_match:
            right_raw = float(right_raw_match.group(1))
            denom = self.right_travel_limit if self.right_travel_limit > 0 else 1200.0
            self.right_visual_ratio = max(0.0, min(1.0, right_raw / denom))
            self.right_dial.set_value(self.right_visual_ratio)

        speed_limit_match = re.search(r"speed\s+zero=.*travelLimit=(\d+)", text)
        if speed_limit_match:
            self.left_travel_limit = max(1.0, float(speed_limit_match.group(1)))

        rpm_limit_match = re.search(r"rpm\s+zero=.*travelLimit=(\d+)", text)
        if rpm_limit_match:
            self.right_travel_limit = max(1.0, float(rpm_limit_match.group(1)))

        if not self.config_mode:
            speed_match = re.search(r"speed=(\d+)", text)
            if speed_match:
                speed_val = float(speed_match.group(1))
                self.left_visual_ratio = max(0.0, min(1.0, speed_val / SPEED_MAX))
                self.left_dial.set_value(self.left_visual_ratio)

            rpm_match = re.search(r"rpm=(\d+)", text)
            if rpm_match:
                rpm_val = float(rpm_match.group(1))
                self.right_visual_ratio = max(0.0, min(1.0, rpm_val / RPM_MAX))
                self.right_dial.set_value(self.right_visual_ratio)

    def _log(self, text: str, add_newline: bool = True):
        if add_newline and not text.endswith("\n"):
            text += "\n"
        self.log.insert("end", text)
        self.log.see("end")
        self._try_update_dials_from_serial_text(text)

    def _on_close(self):
        self._disconnect()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = GaugeSerialGui(root)
    root.mainloop()
