import tkinter as tk
from tkinter import ttk
import serial
import serial.tools.list_ports
import json
import threading

class BrakeChannelUI:
    def __init__(self, parent, channel_name, color):
        self.frame = ttk.LabelFrame(parent, text=channel_name)
        self.frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.values = {}
        fields = [
            ("Voltage", "V", " V"),
            ("Current", "I", " A"),
            ("Watts", "W", " W"),
            ("State", "State", ""),
            ("Fault", "Fault", ""),
            ("Duty", "Duty", " %")
        ]
        
        for i, (label, key, unit) in enumerate(fields):
            ttk.Label(self.frame, text=f"{label}:").grid(row=i, column=0, sticky=tk.W, padx=5, pady=2)
            val_lbl = ttk.Label(self.frame, text="---", font=('Helvetica', 10, 'bold'), foreground=color)
            val_lbl.grid(row=i, column=1, sticky=tk.E, padx=5, pady=2)
            self.values[key] = (val_lbl, unit)

    def update(self, data):
        for key, (lbl, unit) in self.values.items():
            if key in data:
                val = data[key]
                lbl.config(text=f"{val}{unit}")
                if key == "State":
                    color = "#2ecc71" if val == "HOLD" else "#e67e22" if val == "PEAK" else "#95a5a6"
                    lbl.config(foreground=color)
                if key == "Fault":
                    color = "#e74c3c" if val != "NONE" else "#2ecc71"
                    lbl.config(foreground=color)

class DashboardApp:
    def __init__(self, root):
        self.root = root
        self.root.title("MP6519 3-Channel Production Dashboard v1.2.0")
        self.root.geometry("900x500")
        self.root.configure(bg="#1a1a1a")
        
        self.serial_port = None
        self.is_reading = False
        
        self.setup_ui()
        
    def setup_ui(self):
        style = ttk.Style()
        style.theme_use('clam')
        style.configure('TFrame', background='#1a1a1a')
        style.configure('TLabelframe', background='#1a1a1a', foreground='#ffffff')
        style.configure('TLabelframe.Label', background='#1a1a1a', foreground='#3498db', font=('Helvetica', 10, 'bold'))
        style.configure('TLabel', background='#1a1a1a', foreground='#ecf0f1')
        
        # Connection Bar
        conn_frame = ttk.Frame(self.root)
        conn_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(conn_frame, text="COM Port:").pack(side=tk.LEFT, padx=5)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn_frame, textvariable=self.port_var, width=10)
        self.port_combo.pack(side=tk.LEFT, padx=5)
        self.refresh_ports()
        
        self.connect_btn = ttk.Button(conn_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        
        self.status_lbl = ttk.Label(conn_frame, text="DISCONNECTED", foreground="#e74c3c", font=('Helvetica', 10, 'bold'))
        self.status_lbl.pack(side=tk.RIGHT, padx=20)

        # Main Channels Container
        channels_container = ttk.Frame(self.root)
        channels_container.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        self.channels = {
            "CH1": BrakeChannelUI(channels_container, "Brake 1 (External)", "#3498db"),
            "CH2": BrakeChannelUI(channels_container, "Brake 2 (External)", "#2ecc71"),
            "CH3": BrakeChannelUI(channels_container, "Brake 3 (Internal)", "#f1c40f")
        }
        
        # System Status Bar
        sys_frame = ttk.LabelFrame(self.root, text="System Protection Status")
        sys_frame.pack(fill=tk.X, padx=10, pady=10)
        
        self.sys_values = {}
        sys_fields = [
            ("PWR_SYS_OK", "PWR_OK"),
            ("EXT_FAULT", "EXT_F"),
            ("INT_FAULT", "INT_F")
        ]
        
        for i, (label, key) in enumerate(sys_fields):
            ttk.Label(sys_frame, text=f"{label}:").pack(side=tk.LEFT, padx=10)
            val_lbl = ttk.Label(sys_frame, text="---", font=('Helvetica', 10, 'bold'))
            val_lbl.pack(side=tk.LEFT, padx=5)
            self.sys_values[key] = val_lbl

    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports: self.port_combo.current(0)
            
    def toggle_connection(self):
        if self.is_reading:
            self.is_reading = False
            if self.serial_port: self.serial_port.close()
            self.connect_btn.config(text="Connect")
            self.status_lbl.config(text="DISCONNECTED", foreground="#e74c3c")
        else:
            port = self.port_var.get()
            if port:
                try:
                    self.serial_port = serial.Serial(port, 115200, timeout=0.1)
                    self.is_reading = True
                    self.connect_btn.config(text="Disconnect")
                    self.status_lbl.config(text="CONNECTED", foreground="#2ecc71")
                    threading.Thread(target=self.read_serial, daemon=True).start()
                except Exception as e:
                    print(f"Error: {e}")
                    
    def read_serial(self):
        while self.is_reading:
            try:
                line = self.serial_port.readline().decode('utf-8').strip()
                if line.startswith("{") and line.endswith("}"):
                    data = json.loads(line)
                    self.root.after(0, self.update_ui, data)
            except: pass

    def update_ui(self, data):
        # Update Individual Channels
        for ch_id in ["CH1", "CH2", "CH3"]:
            if ch_id in data:
                self.channels[ch_id].update(data[ch_id])
        
        # Update System Status
        for key, lbl in self.sys_values.items():
            if key in data:
                val = data[key]
                text = "OK" if val == 1 else "FAULT"
                color = "#2ecc71" if val == 1 else "#e74c3c"
                lbl.config(text=text, foreground=color)

if __name__ == "__main__":
    root = tk.Tk()
    app = DashboardApp(root)
    root.mainloop()
