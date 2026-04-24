import tkinter as tk
from tkinter import ttk
import serial
import serial.tools.list_ports
import json
import threading

class DashboardApp:
    def __init__(self, root):
        self.root = root
        self.root.title("MP6519 Brake Driver Dashboard")
        self.root.geometry("600x500")
        self.root.configure(bg="#2c3e50")
        
        self.serial_port = None
        self.is_reading = False
        
        self.setup_ui()
        
    def setup_ui(self):
        style = ttk.Style()
        style.theme_use('clam')
        style.configure('TFrame', background='#2c3e50')
        style.configure('TLabel', background='#2c3e50', foreground='#ecf0f1', font=('Helvetica', 12))
        style.configure('Header.TLabel', font=('Helvetica', 16, 'bold'), foreground='#3498db')
        style.configure('Value.TLabel', font=('Helvetica', 14, 'bold'), foreground='#f1c40f')
        style.configure('Fault.TLabel', font=('Helvetica', 14, 'bold'), foreground='#e74c3c')
        
        # Header Frame (COM Port Selection)
        header_frame = ttk.Frame(self.root)
        header_frame.pack(fill=tk.X, padx=20, pady=20)
        
        ttk.Label(header_frame, text="COM Port:").pack(side=tk.LEFT, padx=5)
        
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(header_frame, textvariable=self.port_var, width=15)
        self.port_combo.pack(side=tk.LEFT, padx=5)
        self.refresh_ports()
        
        self.connect_btn = ttk.Button(header_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        
        ttk.Button(header_frame, text="Refresh Ports", command=self.refresh_ports).pack(side=tk.LEFT, padx=5)
        
        # Data Frame
        data_frame = ttk.Frame(self.root)
        data_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=10)
        
        # Define grid
        for i in range(4):
            data_frame.columnconfigure(i, weight=1)
            
        labels = [
            ("Voltage:", "V", " V"),
            ("Current:", "I", " A"),
            ("Live Watts:", "W", " W"),
            ("Frequency:", "Freq", " Hz"),
            ("Duty Cycle:", "Duty", " %"),
            ("Max Detected Watts:", "MaxW", " W"),
            ("Target Hold Watts (10%):", "TgtW", " W"),
        ]
        
        self.value_labels = {}
        
        row = 0
        for title, key, unit in labels:
            ttk.Label(data_frame, text=title).grid(row=row, column=0, sticky=tk.W, pady=10)
            val_lbl = ttk.Label(data_frame, text="---" + unit, style='Value.TLabel')
            val_lbl.grid(row=row, column=1, sticky=tk.E, pady=10)
            self.value_labels[key] = (val_lbl, unit)
            row += 1
            
        # Status Frame
        status_frame = ttk.Frame(self.root)
        status_frame.pack(fill=tk.X, padx=20, pady=20)
        
        ttk.Label(status_frame, text="System State:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.lbl_state = ttk.Label(status_frame, text="DISCONNECTED", style='Value.TLabel')
        self.lbl_state.grid(row=0, column=1, sticky=tk.E, pady=5, padx=20)
        
        ttk.Label(status_frame, text="Fault Status:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.lbl_fault = ttk.Label(status_frame, text="NONE", style='Value.TLabel')
        self.lbl_fault.grid(row=1, column=1, sticky=tk.E, pady=5, padx=20)

    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.current(0)
            
    def toggle_connection(self):
        if self.is_reading:
            self.is_reading = False
            if self.serial_port:
                self.serial_port.close()
            self.connect_btn.config(text="Connect")
            self.lbl_state.config(text="DISCONNECTED", foreground="#f1c40f")
        else:
            port = self.port_var.get()
            if port:
                try:
                    self.serial_port = serial.Serial(port, 115200, timeout=1)
                    self.is_reading = True
                    self.connect_btn.config(text="Disconnect")
                    self.lbl_state.config(text="CONNECTING...", foreground="#3498db")
                    threading.Thread(target=self.read_serial, daemon=True).start()
                except Exception as e:
                    print(f"Error opening port: {e}")
                    
    def read_serial(self):
        while self.is_reading and self.serial_port and self.serial_port.is_open:
            try:
                line = self.serial_port.readline().decode('utf-8').strip()
                if line.startswith("{") and line.endswith("}"):
                    data = json.loads(line)
                    if "log" not in data:
                        self.root.after(0, self.update_data, data)
            except Exception as e:
                pass

    def update_data(self, data):
        for key, (lbl, unit) in self.value_labels.items():
            if key in data:
                lbl.config(text=f"{data[key]}{unit}")
                
        if "State" in data:
            state = data["State"]
            color = "#2ecc71" if state != "FAULT" else "#e74c3c"
            if state == "PEAK": color = "#e67e22"
            elif state == "RAMP_DOWN": color = "#3498db"
            self.lbl_state.config(text=state, foreground=color)
            
        if "Fault" in data:
            fault = data["Fault"]
            color = "#e74c3c" if fault != "NONE" else "#2ecc71"
            self.lbl_fault.config(text=fault, foreground=color)

if __name__ == "__main__":
    root = tk.Tk()
    app = DashboardApp(root)
    root.mainloop()
