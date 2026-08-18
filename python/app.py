import os
import sys
import time
import socket
import threading
import requests
import numpy as np
import cv2
import PIL.Image
import PIL.ImageTk
import tkinter as tk
from tkinter import ttk
from flask import Flask, request, jsonify
from ultralytics import YOLO

import logging
log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)

state_lock = threading.Lock()

latest_cam_frame = None
latest_cam_timestamp = 0
cam_fps = 0.0
cam_frame_count = 0
cam_fps_timer = time.time()

latest_mlx_data = None
latest_mlx_timestamp = 0
mlx_fps = 0.0
mlx_frame_count = 0
mlx_fps_timer = time.time()

esp32main_ip = None
motor_A_speed = 0
motor_B_speed = 0

person_count = 0

last_sent_A = None
last_sent_B = None

def send_direct_motor_cmd(ch, speed):
    global esp32main_ip, last_sent_A, last_sent_B

    ch_upper = str(ch).upper()
    if ch_upper == 'A':
        if last_sent_A == speed:
            return
        last_sent_A = speed
    elif ch_upper == 'B':
        if last_sent_B == speed:
            return
        last_sent_B = speed

    ip = esp32main_ip
    if ip:
        def _send():
            try:
                requests.get(f"http://{ip}/motor?ch={ch}&speed={speed}", timeout=0.8)
            except Exception:
                pass
        threading.Thread(target=_send, daemon=True).start()

def get_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

flask_app = Flask(__name__)

@flask_app.route('/upload_cam', methods=['POST'])
def upload_cam():
    global latest_cam_frame, latest_cam_timestamp, cam_frame_count, cam_fps, cam_fps_timer
    try:
        data = request.data
        if not data:
            return jsonify({"error": "Empty body"}), 400
        
        np_arr = np.frombuffer(data, np.uint8)
        frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)

        if frame is not None:
            with state_lock:
                latest_cam_frame = frame
                latest_cam_timestamp = time.time()
                cam_frame_count += 1
                now = time.time()
                if now - cam_fps_timer >= 1.0:
                    cam_fps = cam_frame_count / (now - cam_fps_timer)
                    cam_frame_count = 0
                    cam_fps_timer = now
            return jsonify({"status": "ok"}), 200
        else:
            return jsonify({"error": "Failed to decode JPEG"}), 400
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@flask_app.route('/upload_mlx', methods=['POST'])
def upload_mlx():
    global latest_mlx_data, latest_mlx_timestamp, mlx_frame_count, mlx_fps, mlx_fps_timer
    global motor_A_speed, motor_B_speed, esp32main_ip
    try:
        esp32main_ip = request.remote_addr
        json_data = request.get_json(force=True, silent=True)
        if json_data is None:
            return jsonify({"error": "Invalid JSON"}), 400

        if isinstance(json_data, list) and len(json_data) == 768:
            arr = np.array(json_data, dtype=np.float32).reshape((24, 32))
            with state_lock:
                latest_mlx_data = arr
                latest_mlx_timestamp = time.time()
                mlx_frame_count += 1
                now = time.time()
                if now - mlx_fps_timer >= 1.0:
                    mlx_fps = mlx_frame_count / (now - mlx_fps_timer)
                    mlx_frame_count = 0
                    mlx_fps_timer = now
                
                resp = {
                    "status": "ok",
                    "motorA": motor_A_speed,
                    "motorB": motor_B_speed,
                    "persons": person_count
                }
            return jsonify(resp), 200
        else:
            return jsonify({"error": "Expected array of 768 numbers"}), 400
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@flask_app.route('/motor', methods=['GET', 'POST'])
def handle_motor_api():
    global motor_A_speed, motor_B_speed
    ch = request.args.get('ch') or request.form.get('ch')
    speed = request.args.get('speed') or request.form.get('speed')
    
    if ch and speed is not None:
        try:
            sp = int(speed)
            sp = max(-255, min(255, sp))
            with state_lock:
                if ch.upper() == 'A':
                    motor_A_speed = sp
                elif ch.upper() == 'B':
                    motor_B_speed = sp
            return jsonify({"status": "ok", "ch": ch, "speed": sp, "persons": person_count})
        except ValueError:
            return jsonify({"error": "Speed must be integer"}), 400

    return jsonify({"motorA": motor_A_speed, "motorB": motor_B_speed, "persons": person_count})


@flask_app.route('/motorStop', methods=['GET', 'POST'])
def handle_motor_stop_api():
    global motor_A_speed, motor_B_speed
    with state_lock:
        motor_A_speed = 0
        motor_B_speed = 0
    return jsonify({"status": "stopped", "motorA": 0, "motorB": 0, "persons": person_count})


def run_flask():
    flask_app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)


class MainApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ResQBot Multi-Sensor Monitoring & Motor Control (WASD Drive Enabled)")
        self.geometry("1100x820")
        self.configure(bg="#1e1e1e")
        self.resizable(True, True)

        self.pressed_keys = set()
        self.key_release_timers = {}
        self.cruise_speed = 255
        self.target_motor_A = 0
        self.target_motor_B = 0
        self.current_motor_A = 0.0
        self.current_motor_B = 0.0
        self.ramp_step = 12.0
        self.is_ramping_update = False

        self.model_path = os.path.join(os.path.dirname(__file__), "person.pt")
        if not os.path.exists(self.model_path):
            self.model_path = "person.pt"

        print(f"Loading YOLO Model from: {self.model_path}...")
        try:
            self.yolo_model = YOLO(self.model_path)
            print("YOLO Model loaded successfully!")
        except Exception as e:
            print(f"Error loading YOLO model: {e}")
            self.yolo_model = None

        self.local_ip = get_local_ip()
        self.create_widgets()

        self.bind('<KeyPress>', self.on_key_press)
        self.bind('<KeyRelease>', self.on_key_release)

        self.update_gui()

    def create_widgets(self):
        header_frame = tk.Frame(self, bg="#2d2d2d", pady=10, padx=15)
        header_frame.pack(fill=tk.X, side=tk.TOP)

        title_label = tk.Label(
            header_frame,
            text="ResQBot Control Station",
            font=("Segoe UI", 18, "bold"),
            fg="#ffffff",
            bg="#2d2d2d"
        )
        title_label.pack(side=tk.LEFT, padx=10)

        server_info = tk.Label(
            header_frame,
            text=f"Server Running @ http://{self.local_ip}:5000  (ESP32 Endpoint)",
            font=("Segoe UI", 11, "bold"),
            fg="#4CAF50",
            bg="#2d2d2d"
        )
        server_info.pack(side=tk.RIGHT, padx=10)

        feeds_frame = tk.Frame(self, bg="#1e1e1e", pady=10)
        feeds_frame.pack(fill=tk.BOTH, expand=True, padx=15)

        cam_container = tk.LabelFrame(
            feeds_frame,
            text=" Camera Feed (YOLOv26 COCO Person Detection) ",
            font=("Segoe UI", 11, "bold"),
            fg="#64B5F6",
            bg="#252526",
            bd=2,
            relief=tk.GROOVE
        )
        cam_container.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5)

        self.cam_label = tk.Label(cam_container, bg="#000000", text="Waiting for ESP32-CAM stream...", fg="#888888", font=("Segoe UI", 12))
        self.cam_label.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        mlx_container = tk.LabelFrame(
            feeds_frame,
            text=" Thermal Feed (MLX90640 Heatmap) ",
            font=("Segoe UI", 11, "bold"),
            fg="#FF8A65",
            bg="#252526",
            bd=2,
            relief=tk.GROOVE
        )
        mlx_container.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=5)

        self.mlx_label = tk.Label(mlx_container, bg="#000000", text="Waiting for MLX90640 stream...", fg="#888888", font=("Segoe UI", 12))
        self.mlx_label.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        motor_frame = tk.LabelFrame(
            self,
            text=" L9110 Motor Driver Control Panel ",
            font=("Segoe UI", 12, "bold"),
            fg="#E0E0E0",
            bg="#252526",
            bd=2,
            relief=tk.GROOVE,
            pady=10,
            padx=15
        )
        motor_frame.pack(fill=tk.X, side=tk.TOP, padx=15, pady=10)

        motorA_box = tk.Frame(motor_frame, bg="#2d2d2d", bd=1, relief=tk.RAISED, pady=10, padx=15)
        motorA_box.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=10)

        mA_title = tk.Label(motorA_box, text="Propeller / Motor A", font=("Segoe UI", 12, "bold"), fg="#ffffff", bg="#2d2d2d")
        mA_title.pack()

        self.valA_label = tk.Label(motorA_box, text="Speed: 0", font=("Segoe UI", 11, "bold"), fg="#64B5F6", bg="#2d2d2d")
        self.valA_label.pack(pady=2)

        self.sliderA = tk.Scale(
            motorA_box,
            from_=-255,
            to=255,
            orient=tk.HORIZONTAL,
            bg="#2d2d2d",
            fg="#ffffff",
            highlightthickness=0,
            troughcolor="#444444",
            activebackground="#64B5F6",
            command=self.on_sliderA_change
        )
        self.sliderA.set(0)
        self.sliderA.pack(fill=tk.X, pady=5)

        btnA_frame = tk.Frame(motorA_box, bg="#2d2d2d")
        btnA_frame.pack()

        tk.Button(btnA_frame, text="Rev (-255)", bg="#333333", fg="#ffffff", width=8, command=lambda: self.set_motor('A', -255)).pack(side=tk.LEFT, padx=3)
        tk.Button(btnA_frame, text="Stop (0)", bg="#333333", fg="#ffffff", width=8, command=lambda: self.set_motor('A', 0)).pack(side=tk.LEFT, padx=3)
        tk.Button(btnA_frame, text="Fwd (+255)", bg="#333333", fg="#ffffff", width=8, command=lambda: self.set_motor('A', 255)).pack(side=tk.LEFT, padx=3)

        wasd_box = tk.Frame(motor_frame, bg="#2d2d2d", bd=1, relief=tk.RAISED, pady=5, padx=10)
        wasd_box.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10)

        wasd_title = tk.Label(wasd_box, text="WASD Game Steering", font=("Segoe UI", 11, "bold"), fg="#ffffff", bg="#2d2d2d")
        wasd_title.pack(pady=2)

        pad_frame = tk.Frame(wasd_box, bg="#2d2d2d")
        pad_frame.pack(pady=2)

        self.btn_w = tk.Button(pad_frame, text="W\nFwd", font=("Segoe UI", 8, "bold"), width=5, bg="#333333", fg="#ffffff", activebackground="#4CAF50")
        self.btn_w.grid(row=0, column=1, padx=2, pady=2)

        self.btn_a = tk.Button(pad_frame, text="A\nLeft", font=("Segoe UI", 8, "bold"), width=5, bg="#333333", fg="#ffffff", activebackground="#2196F3")
        self.btn_a.grid(row=1, column=0, padx=2, pady=2)

        self.btn_s = tk.Button(pad_frame, text="S\nRev", font=("Segoe UI", 8, "bold"), width=5, bg="#333333", fg="#ffffff", activebackground="#FF9800")
        self.btn_s.grid(row=1, column=1, padx=2, pady=2)

        self.btn_d = tk.Button(pad_frame, text="D\nRight", font=("Segoe UI", 8, "bold"), width=5, bg="#333333", fg="#ffffff", activebackground="#2196F3")
        self.btn_d.grid(row=1, column=2, padx=2, pady=2)

        self.btn_w.bind('<ButtonPress-1>', lambda e: self.simulate_key_press('w'))
        self.btn_w.bind('<ButtonRelease-1>', lambda e: self.simulate_key_release('w'))

        self.btn_a.bind('<ButtonPress-1>', lambda e: self.simulate_key_press('a'))
        self.btn_a.bind('<ButtonRelease-1>', lambda e: self.simulate_key_release('a'))

        self.btn_s.bind('<ButtonPress-1>', lambda e: self.simulate_key_press('s'))
        self.btn_s.bind('<ButtonRelease-1>', lambda e: self.simulate_key_release('s'))

        self.btn_d.bind('<ButtonPress-1>', lambda e: self.simulate_key_press('d'))
        self.btn_d.bind('<ButtonRelease-1>', lambda e: self.simulate_key_release('d'))

        self.wasd_status_label = tk.Label(wasd_box, text="Status: [STOPPED]", font=("Segoe UI", 9, "bold"), fg="#888888", bg="#2d2d2d")
        self.wasd_status_label.pack(pady=2)

        stop_btn = tk.Button(
            wasd_box,
            text="STOP (Space)",
            font=("Segoe UI", 9, "bold"),
            bg="#d32f2f",
            fg="#ffffff",
            activebackground="#b71c1c",
            activeforeground="#ffffff",
            command=self.stop_all_motors
        )
        stop_btn.pack(pady=2)

        motorB_box = tk.Frame(motor_frame, bg="#2d2d2d", bd=1, relief=tk.RAISED, pady=10, padx=15)
        motorB_box.pack(side=tk.RIGHT, fill=tk.X, expand=True, padx=10)

        mB_title = tk.Label(motorB_box, text="Propeller / Motor B", font=("Segoe UI", 12, "bold"), fg="#ffffff", bg="#2d2d2d")
        mB_title.pack()

        self.valB_label = tk.Label(motorB_box, text="Speed: 0", font=("Segoe UI", 11, "bold"), fg="#FF8A65", bg="#2d2d2d")
        self.valB_label.pack(pady=2)

        self.sliderB = tk.Scale(
            motorB_box,
            from_=-255,
            to=255,
            orient=tk.HORIZONTAL,
            bg="#2d2d2d",
            fg="#ffffff",
            highlightthickness=0,
            troughcolor="#444444",
            activebackground="#FF8A65",
            command=self.on_sliderB_change
        )
        self.sliderB.set(0)
        self.sliderB.pack(fill=tk.X, pady=5)

        btnB_frame = tk.Frame(motorB_box, bg="#2d2d2d")
        btnB_frame.pack()

        tk.Button(btnB_frame, text="Rev (-255)", bg="#333333", fg="#ffffff", width=8, command=lambda: self.set_motor('B', -255)).pack(side=tk.LEFT, padx=3)
        tk.Button(btnB_frame, text="Stop (0)", bg="#333333", fg="#ffffff", width=8, command=lambda: self.set_motor('B', 0)).pack(side=tk.LEFT, padx=3)
        tk.Button(btnB_frame, text="Fwd (+255)", bg="#333333", fg="#ffffff", width=8, command=lambda: self.set_motor('B', 255)).pack(side=tk.LEFT, padx=3)

        self.status_bar = tk.Label(
            self,
            text=" Status: System Ready | Cam FPS: 0.0 | MLX FPS: 0.0 | Persons Detected: 0 ",
            font=("Segoe UI", 10),
            fg="#aaaaaa",
            bg="#111111",
            anchor=tk.W,
            pady=5,
            padx=10
        )
        self.status_bar.pack(fill=tk.X, side=tk.BOTTOM)

    def on_sliderA_change(self, val):
        if self.is_ramping_update:
            return
        v = int(val)
        self.target_motor_A = v
        self.current_motor_A = float(v)
        self.valA_label.config(text=f"Speed: {v}")
        with state_lock:
            global motor_A_speed
            motor_A_speed = v
        send_direct_motor_cmd('A', v)

    def on_sliderB_change(self, val):
        if self.is_ramping_update:
            return
        v = int(val)
        self.target_motor_B = v
        self.current_motor_B = float(v)
        self.valB_label.config(text=f"Speed: {v}")
        with state_lock:
            global motor_B_speed
            motor_B_speed = v
        send_direct_motor_cmd('B', v)

    def set_motor(self, ch, val):
        if ch == 'A':
            self.target_motor_A = val
            self.current_motor_A = float(val)
            self.sliderA.set(val)
        elif ch == 'B':
            self.target_motor_B = val
            self.current_motor_B = float(val)
            self.sliderB.set(val)

    def stop_all_motors(self):
        self.pressed_keys.clear()
        self.target_motor_A = 0
        self.target_motor_B = 0
        self.current_motor_A = 0.0
        self.current_motor_B = 0.0
        self.is_ramping_update = True
        self.sliderA.set(0)
        self.sliderB.set(0)
        self.is_ramping_update = False
        self.valA_label.config(text="Speed: 0")
        self.valB_label.config(text="Speed: 0")
        with state_lock:
            global motor_A_speed, motor_B_speed
            motor_A_speed = 0
            motor_B_speed = 0
        send_direct_motor_cmd('A', 0)
        send_direct_motor_cmd('B', 0)
        self.wasd_status_label.config(text="Status: [STOPPED]")
        self.update_wasd_button_styles()

    def on_key_press(self, event):
        key = event.keysym.lower()
        if key in ['w', 'a', 's', 'd', 'space']:
            if key in self.key_release_timers:
                self.after_cancel(self.key_release_timers[key])
                del self.key_release_timers[key]

            if key not in self.pressed_keys:
                self.pressed_keys.add(key)
                self.process_wasd_steering()

    def on_key_release(self, event):
        key = event.keysym.lower()
        if key in ['w', 'a', 's', 'd', 'space']:
            if key in self.key_release_timers:
                self.after_cancel(self.key_release_timers[key])

            self.key_release_timers[key] = self.after(60, lambda k=key: self._confirm_key_release(k))

    def _confirm_key_release(self, key):
        if key in self.key_release_timers:
            del self.key_release_timers[key]
        if key in self.pressed_keys:
            self.pressed_keys.remove(key)
            self.process_wasd_steering()

    def simulate_key_press(self, key):
        if key in self.key_release_timers:
            self.after_cancel(self.key_release_timers[key])
            del self.key_release_timers[key]
        self.pressed_keys.add(key)
        self.process_wasd_steering()

    def simulate_key_release(self, key):
        if key in self.pressed_keys:
            self.pressed_keys.remove(key)
            self.process_wasd_steering()

    def process_wasd_steering(self):
        speed = self.cruise_speed

        if 'space' in self.pressed_keys:
            target_A, target_B = 0, 0
            self.current_motor_A = 0.0
            self.current_motor_B = 0.0
            mode_text = "[STOPPED]"
        elif 'w' in self.pressed_keys and 'a' in self.pressed_keys:
            target_A, target_B = int(speed * 0.4), speed
            mode_text = "[FORWARD LEFT]"
        elif 'w' in self.pressed_keys and 'd' in self.pressed_keys:
            target_A, target_B = speed, int(speed * 0.4)
            mode_text = "[FORWARD RIGHT]"
        elif 'w' in self.pressed_keys:
            target_A, target_B = speed, speed
            mode_text = "[FORWARD]"
        elif 's' in self.pressed_keys and 'a' in self.pressed_keys:
            target_A, target_B = -int(speed * 0.4), -speed
            mode_text = "[REVERSE LEFT]"
        elif 's' in self.pressed_keys and 'd' in self.pressed_keys:
            target_A, target_B = -speed, -int(speed * 0.4)
            mode_text = "[REVERSE RIGHT]"
        elif 's' in self.pressed_keys:
            target_A, target_B = -speed, -speed
            mode_text = "[REVERSE]"
        elif 'a' in self.pressed_keys:
            target_A, target_B = -speed, speed
            mode_text = "[SPIN LEFT]"
        elif 'd' in self.pressed_keys:
            target_A, target_B = speed, -speed
            mode_text = "[SPIN RIGHT]"
        else:
            target_A, target_B = 0, 0
            mode_text = "[STOPPED]"

        self.target_motor_A = target_A
        self.target_motor_B = target_B
        self.wasd_status_label.config(text=f"Status: {mode_text}")
        self.update_wasd_button_styles()

    def apply_motor_ramping(self):
        changed = False

        if self.current_motor_A < self.target_motor_A:
            self.current_motor_A = min(float(self.target_motor_A), self.current_motor_A + self.ramp_step)
            changed = True
        elif self.current_motor_A > self.target_motor_A:
            self.current_motor_A = max(float(self.target_motor_A), self.current_motor_A - self.ramp_step)
            changed = True

        if self.current_motor_B < self.target_motor_B:
            self.current_motor_B = min(float(self.target_motor_B), self.current_motor_B + self.ramp_step)
            changed = True
        elif self.current_motor_B > self.target_motor_B:
            self.current_motor_B = max(float(self.target_motor_B), self.current_motor_B - self.ramp_step)
            changed = True

        if changed:
            valA = int(round(self.current_motor_A))
            valB = int(round(self.current_motor_B))

            self.is_ramping_update = True
            self.sliderA.set(valA)
            self.sliderB.set(valB)
            self.is_ramping_update = False

            self.valA_label.config(text=f"Speed: {valA}")
            self.valB_label.config(text=f"Speed: {valB}")

            with state_lock:
                global motor_A_speed, motor_B_speed
                motor_A_speed = valA
                motor_B_speed = valB

            send_direct_motor_cmd('A', valA)
            send_direct_motor_cmd('B', valB)

    def update_wasd_button_styles(self):
        self.btn_w.config(bg="#4CAF50" if 'w' in self.pressed_keys else "#333333")
        self.btn_a.config(bg="#2196F3" if 'a' in self.pressed_keys else "#333333")
        self.btn_s.config(bg="#FF9800" if 's' in self.pressed_keys else "#333333")
        self.btn_d.config(bg="#2196F3" if 'd' in self.pressed_keys else "#333333")

    def process_mlx_raw(self, raw_mlx):
        if raw_mlx is None:
            return None, 0.0, 0.0, 0.0

        min_t = float(np.min(raw_mlx))
        max_t = float(np.max(raw_mlx))
        avg_t = float(np.mean(raw_mlx))

        norm_mlx = cv2.normalize(raw_mlx, None, alpha=0, beta=255, norm_type=cv2.NORM_MINMAX, dtype=cv2.CV_8U)

        heat_bgr = cv2.applyColorMap(norm_mlx, cv2.COLORMAP_JET)

        heat_large = cv2.resize(heat_bgr, (480, 360), interpolation=cv2.INTER_NEAREST)

        return heat_large, min_t, max_t, avg_t

    def update_gui(self):
        global latest_cam_frame, latest_mlx_data, person_count
        global cam_fps, mlx_fps

        now = time.time()

        self.apply_motor_ramping()

        with state_lock:
            cam_frame = latest_cam_frame.copy() if latest_cam_frame is not None else None
            cam_ts = latest_cam_timestamp

        if cam_frame is not None and (now - cam_ts < 3.0):
            annotated_frame = cam_frame.copy()
            detected_persons = 0

            if self.yolo_model is not None:
                results = self.yolo_model(annotated_frame, classes=[0], conf=0.35, verbose=False)
                for r in results:
                    boxes = r.boxes
                    for box in boxes:
                        detected_persons += 1
                        x1, y1, x2, y2 = map(int, box.xyxy[0])
                        conf = float(box.conf[0])
                        cv2.rectangle(annotated_frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
                        label = f"Person {conf:.2f}"
                        (w, h), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.6, 2)
                        cv2.rectangle(annotated_frame, (x1, y1 - h - 10), (x1 + w, y1), (0, 255, 0), -1)
                        cv2.putText(annotated_frame, label, (x1, y1 - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 2)

            person_count = detected_persons

            count_bg_color = (0, 0, 200) if detected_persons > 0 else (50, 50, 50)
            cv2.rectangle(annotated_frame, (10, 10), (220, 45), count_bg_color, -1)
            cv2.putText(annotated_frame, f"Persons Detected: {detected_persons}", (15, 33), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

            rgb_cam = cv2.cvtColor(annotated_frame, cv2.COLOR_BGR2RGB)
            pil_cam = PIL.Image.fromarray(rgb_cam)
            pil_cam = pil_cam.resize((480, 360), PIL.Image.Resampling.LANCZOS)
            imgtk_cam = PIL.ImageTk.PhotoImage(image=pil_cam)
            self.cam_label.imgtk = imgtk_cam
            self.cam_label.configure(image=imgtk_cam, text="")
        else:
            self.cam_label.configure(image="", text="Waiting for ESP32-CAM stream...\n(POST to /upload_cam)")

        with state_lock:
            mlx_data = latest_mlx_data.copy() if latest_mlx_data is not None else None
            mlx_ts = latest_mlx_timestamp

        if mlx_data is not None and (now - mlx_ts < 3.0):
            heat_large, min_t, max_t, avg_t = self.process_mlx_raw(mlx_data)

            if heat_large is not None:
                stats_str = f"Min: {min_t:.1f}C  Max: {max_t:.1f}C  Avg: {avg_t:.1f}C"
                cv2.rectangle(heat_large, (0, 330), (480, 360), (0, 0, 0), -1)
                cv2.putText(heat_large, stats_str, (15, 352), cv2.FONT_HERSHEY_SIMPLEX, 0.65, (255, 255, 255), 2)

                rgb_mlx = cv2.cvtColor(heat_large, cv2.COLOR_BGR2RGB)
                pil_mlx = PIL.Image.fromarray(rgb_mlx)
                imgtk_mlx = PIL.ImageTk.PhotoImage(image=pil_mlx)
                self.mlx_label.imgtk = imgtk_mlx
                self.mlx_label.configure(image=imgtk_mlx, text="")
        else:
            self.mlx_label.configure(image="", text="Waiting for MLX90640 stream...\n(POST to /upload_mlx)")

        esp_ip_str = esp32main_ip if esp32main_ip else "Not Connected"
        self.status_bar.config(
            text=f" Status: Server Active | ESP32 IP: {esp_ip_str} | Cam FPS: {cam_fps:.1f} | MLX FPS: {mlx_fps:.1f} | Persons: {person_count} | Motor A: {motor_A_speed} | Motor B: {motor_B_speed}"
        )

        self.after(33, self.update_gui)


if __name__ == "__main__":
    print("==========================================================")
    print(" Starting ResQBot Control Station & Flask Endpoint Server")
    print("==========================================================")
    
    flask_thread = threading.Thread(target=run_flask, daemon=True)
    flask_thread.start()
    print(f"Server endpoint hosted locally at http://0.0.0.0:5000")
    print(f"Access from local devices via http://{get_local_ip()}:5000")

    app = MainApp()
    app.mainloop()
