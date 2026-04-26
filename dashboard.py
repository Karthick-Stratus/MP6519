import tkinter as tk
from tkinter import ttk
import serial
import serial.tools.list_ports
import json
import threading

class DashboardApp:
    def __init__(self, root):
        self.root = root
        self.root.title("MP6519 3-Channel Telemetry Dashboard v1.3.0")
        self.root.geometry("1000x600")
        self.root.configure(bg="#2c3e50")
        
        self.serial_port = None
        self.is_reading = False
        
        self.setup_ui()
        
    def setup_ui(self):
        style = ttk.Style()
        style.theme_use('clam')
        style.configure('TFrame', background='#2c3e50')
        style.configure('TLabelframe', background='#2c3e50', foreground='#ecf0f1')
        style.configure('TLabelframe.Label', background='#2c3e50', foreground='#3498db', font=('Helvetica', 12, 'bold'))
        style.configure('TLabel', background='#2c3e50', foreground='#ecf0f1', font=('Helvetica', 10))
        
        # Header / Connection
        header = ttk.Frame(self.root)
        header.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header, text="COM Port:").pack(side=tk.LEFT, padx=5)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(header, textvariable=self.port_var, width=15)
        self.port_combo.pack(side=tk.LEFT, padx=5)
        
        self.connect_btn = ttk.Button(header, text="Connect", command=self.toggle_connection)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        
        ttk.Button(header, text="Refresh", command=self.refresh_ports).pack(side=tk.LEFT, padx=5)
        
        self.status_lbl = ttk.Label(header, text="DISCONNECTED", foreground="#e74c3c", font=('Helvetica', 12, 'bold'))
        self.status_lbl.pack(side=tk.RIGHT, padx=10)

        # Main Layout
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        # Left Panel - Inputs
        left_panel = ttk.Frame(main_frame)
        left_panel.pack(side=tk.LEFT, fill=tk.Y, padx=5)
        
        input_frame = ttk.LabelFrame(left_panel, text="System Inputs")
        input_frame.pack(fill=tk.X, pady=5)
        
        self.input_labels = {}
        inputs_list = [
            ("COMBINED", "CH3 Trigger"), 
            ("EMERGENCY", "CH1/2 Trigger"), 
            ("PWR_OK", "Power System OK"), 
            ("EXT_F", "Ext Brake Fault"), 
            ("INT_F", "Int Brake Fault")
        ]
        
        for i, (key, desc) in enumerate(inputs_list):
            ttk.Label(input_frame, text=f"{desc}:").grid(row=i, column=0, sticky=tk.W, padx=5, pady=5)
            lbl = ttk.Label(input_frame, text="---", font=('Helvetica', 10, 'bold'))
            lbl.grid(row=i, column=1, sticky=tk.E, padx=5, pady=5)
            self.input_labels[key] = lbl
            
        # Right Panel - Channels
        channels_panel = ttk.Frame(main_frame)
        channels_panel.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5)
        
        self.channel_uis = {}
        for ch in ["CH1", "CH2", "CH3"]:
            frame = ttk.LabelFrame(channels_panel, text=f"Brake {ch[-1]}")
            frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
            
            fields = [
                ("Voltage (V)", "V"), ("Current (A)", "I"), ("Power (W)", "W"),
                ("Duty Cycle (%)", "Duty"), ("Peak Watts", "PeakW"), ("Hold Watts", "HoldW"),
                ("State", "State"), ("System Fault", "Fault"),
                ("FT Pin", "FT_PIN"), ("ALERT Pin", "ALERT_PIN")
            ]
            
            ui_elements = {}
            for i, (label, key) in enumerate(fields):
                ttk.Label(frame, text=f"{label}:").grid(row=i, column=0, sticky=tk.W, padx=5, pady=5)
                val_lbl = ttk.Label(frame, text="---", font=('Helvetica', 10, 'bold'))
                val_lbl.grid(row=i, column=1, sticky=tk.E, padx=5, pady=5)
                ui_elements[key] = val_lbl
                
            self.channel_uis[ch] = ui_elements

        self.refresh_ports()

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
                    self.serial_port = serial.Serial(port, 115200, timeout=1)
                    self.is_reading = True
                    self.connect_btn.config(text="Disconnect")
                    self.status_lbl.config(text="CONNECTED", foreground="#2ecc71")
                    threading.Thread(target=self.read_loop, daemon=True).start()
                except Exception as e:
                    self.status_lbl.config(text="ERROR", foreground="#e74c3c")

    def read_loop(self):
        while self.is_reading and self.serial_port and self.serial_port.is_open:
            try:
                line = self.serial_port.readline()
                if not line: continue
                
                try:
                    line_str = line.decode('utf-8', errors='ignore').strip()
                except:
                    continue
                
                if line_str.startswith("{") and line_str.endswith("}"):
                    try:
                        data = json.loads(line_str)
                        self.root.after(0, self.update_ui, data)
                    except json.JSONDecodeError:
                        pass # Ignore malformed JSON to keep app stable
            except Exception as e:
                pass # Ignore serial errors, keep loop running if possible

    def update_ui(self, data):
        # Update Inputs
        if "INPUTS" in data:
            inputs = data["INPUTS"]
            for key, lbl in self.input_labels.items():
                if key in inputs:
                    val = inputs[key]
                    text = "HIGH (1)" if val == 1 else "LOW (0)"
                    color = "#2ecc71" if val == 1 else "#e74c3c"
                    
                    # Special coloring logic for inputs
                    if key in ["PWR_OK"]:
                        color = "#2ecc71" if val == 1 else "#e74c3c"
                    elif key in ["EXT_F", "INT_F"]:
                        color = "#e74c3c" if val == 1 else "#2ecc71" # Fault is bad if HIGH
                    elif key in ["COMBINED", "EMERGENCY"]:
                        color = "#3498db" if val == 1 else "#7f8c8d" # Just trigger colors
                        
                    lbl.config(text=text, foreground=color)
                    
        # Update Channels
        for ch in ["CH1", "CH2", "CH3"]:
            if ch in data:
                ch_data = data[ch]
                ui_elements = self.channel_uis[ch]
                
                for key, val in ch_data.items():
                    if key in ui_elements:
                        lbl = ui_elements[key]
                        
                        # Formatting and Coloring
                        if key in ["V", "I", "W", "Duty", "PeakW", "HoldW"]:
                            lbl.config(text=str(val), foreground="#f1c40f")
                        elif key == "State":
                            color = "#2ecc71" if val == "HOLD" else "#e67e22" if val == "PEAK" else "#7f8c8d"
                            lbl.config(text=val, foreground=color)
                        elif key == "Fault":
                            color = "#e74c3c" if val != "NONE" else "#2ecc71"
                            lbl.config(text=val, foreground=color)
                        elif key in ["FT_PIN", "ALERT_PIN"]:
                            text = "HIGH (1)" if val == 1 else "LOW (0)"
                            # Assuming HIGH is good for FT and ALERT (pull-ups)
                            color = "#2ecc71" if val == 1 else "#e74c3c"
                            lbl.config(text=text, foreground=color)

if __name__ == "__main__":
    root = tk.Tk()
    app = DashboardApp(root)
    root.mainloop()
