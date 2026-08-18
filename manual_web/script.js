document.addEventListener('DOMContentLoaded', () => {
  // Corrected Component Hotspot Coordinates Mapped Directly to perfboard_view.png (2064x2178)
  const components = [
    {
      id: 'rocker-switch',
      num: 1,
      name: 'Power & Charge Mode Rocker Switch',
      category: 'power',
      badge: 'badge-power',
      type: 'DPDT / SPST Main Power Toggle Switch',
      x: 26.2,
      y: 36.3,
      description: 'Primary power circuit isolator. Toggles system operation between battery power mode and isolated charging mode.',
      operation: 'Position I = SYSTEM POWER ON\nPowers ESP32-Main, ESP32-CAM, OLED, MLX Thermal, Ultrasonic Sensor, Buzzer, and L9110 Motor Driver.\n\nPosition O = CHARGE MODE\nCompletely isolates system logic rails; enables safe 3S Type-C charging.',
      pinouts: [
        { name: 'Common (Middle Terminal)', target: '3S Battery Positive Terminal (BAT+)' },
        { name: 'Terminal I (POWER ON)', target: 'LM2596 & Step-down Buck Regulators IN+' },
        { name: 'Terminal O (CHARGE MODE)', target: '3S 2A Type-C Charger BAT+ Input' }
      ],
      warning: 'Do not connect the USB Type-C charger while the switch is in Position I (ON). Always switch to Position O (CHARGE) before charging.'
    },
    {
      id: 'charger-module',
      num: 2,
      name: '3S 2A Type-C Battery Charger Module',
      category: 'power',
      badge: 'badge-power',
      type: '3-Series Lithium Charging Circuit (11.1V - 12.6V)',
      x: 23.7,
      y: 68.0,
      description: 'Dedicated 3S lithium battery management module with Type-C input and status LED indicators.',
      operation: 'Plugging in USB Type-C power with switch in Position O initiates battery charging.\n\nSolid RED LED = Battery actively charging.\nSolid BLUE LED = Battery fully charged.',
      pinouts: [
        { name: 'USB Type-C Input', target: '5V / 2A+ External USB Power Adapter' },
        { name: 'BAT+ / BAT-', target: 'Connected to 3S 1300mAh Battery Terminals' },
        { name: 'OUT+ / OUT-', target: 'Switched 3S Output to Rocker Switch' }
      ],
      warning: 'Observe strict polarity on BAT+ (Red) and BAT- (Black). Reverse polarity will destroy the charging controller.'
    },
    {
      id: 'lm2596-buck',
      num: 3,
      name: 'LM2596 Step-Down Buck Regulator',
      category: 'power',
      badge: 'badge-power',
      type: 'DC-DC Step-Down Converter (3S to 5.0V DC)',
      x: 42.6,
      y: 64.3,
      description: 'Steps down high 3S battery voltage (~12.6V max) to a stable 5.0V system power rail for logic and sensor circuits.',
      operation: 'Adjust the brass multi-turn potentiometer with a screwdriver until OUT+ outputs exactly 5.00V DC under load.',
      pinouts: [
        { name: 'IN+ / IN-', target: 'Switched 3S Battery Power Line from Switch' },
        { name: 'OUT+ (5V)', target: 'ESP32 5V Vin, OLED VDD, L9110 VCC, Buzzer VCC' },
        { name: 'OUT- (GND)', target: 'Common Ground Bus across perfboard' }
      ],
      warning: 'Measure OUT+ voltage with a multimeter BEFORE connecting microcontrollers. Output higher than 5.5V will damage logic ICs.'
    },
    {
      id: 'mini-buck',
      num: 4,
      name: 'Mini Step-Down Power Module',
      category: 'power',
      badge: 'badge-power',
      type: 'Miniature DC-DC Step-Down Voltage Regulator',
      x: 43.6,
      y: 36.5,
      description: 'Secondary power regulation rail providing clean voltage filtering for camera and sensitive analog sensor circuits.',
      operation: 'Filters motor noise and voltage ripples to maintain camera stream stability and accurate sensor reads.',
      pinouts: [
        { name: 'IN+ / IN-', target: 'Primary 5V System Rail' },
        { name: 'OUT+ / OUT-', target: 'Regulated ESP32-CAM & MLX Power Lines' }
      ],
      warning: 'Maintain common ground connection across all power step-down modules.'
    },
    {
      id: 'esp32-cam',
      num: 5,
      name: 'ESP32-CAM AI-Thinker Module',
      category: 'mcu',
      badge: 'badge-mcu',
      type: 'ESP32-S Microcontroller + OV2640 Camera + Flash LED',
      x: 6.8,
      y: 41.8,
      description: 'Captures real-time RGB video stream and streams JPEG frames via HTTP POST to Python server at port 5000/upload_cam.',
      operation: 'Boots up in SoftAP mode (ResQBot-CAM-AP). Access http://192.168.4.1 to configure WiFi SSID, Password, and Server IP address.',
      pinouts: [
        { name: '5V / GND', target: '5V System Power Rail / Common Ground' },
        { name: 'GPIO 4', target: 'Onboard Ultra-Bright Flash LED' },
        { name: 'U0T / U0R', target: 'Serial UART Programming Interface' }
      ],
      warning: 'Requires minimum 500mA current during camera init. Insufficient current causes continuous boot loops.'
    },
    {
      id: 'esp32-main',
      num: 6,
      name: 'ESP32-Main Controller (WROOM-32D)',
      category: 'mcu',
      badge: 'badge-mcu',
      type: 'ESP32 Dual-Core Microcontroller (240MHz)',
      x: 60.1,
      y: 62.4,
      description: 'Central controller running esp32main.ino. Parses MLX thermal UART data, renders OLED status, handles motor PWM, and triggers buzzer.',
      operation: 'Runs SoftAP (ResQBot-Main-AP) for setup. Transmits thermal array to /upload_mlx and receives PWM speed targets from server.',
      pinouts: [
        { name: 'GPIO 16 (RX2) / 17 (TX2)', target: 'MLX90640 UART Interface @ 115200 Baud' },
        { name: 'GPIO 21 (SDA) / 22 (SCL)', target: 'SSD1306 OLED Display I2C Bus' },
        { name: 'GPIO 32, 33, 25, 26', target: 'L9110S Motor Driver PWM Control Channels' },
        { name: 'GPIO 27', target: 'Active Piezo Alarm Buzzer Control' }
      ],
      warning: 'Do not feed voltage exceeding 3.3V into GPIO input pins.'
    },
    {
      id: 'mlx-thermal',
      num: 7,
      name: 'MLX90640 Thermal Array Camera Sensor',
      category: 'sensor',
      badge: 'badge-sensor',
      type: '32x24 IR Thermal Sensor Matrix (768 Pixels)',
      x: 70.7,
      y: 22.0,
      description: 'Measures 768 thermal points and streams hex packet frames over UART (0x5A 0x5A header) at 115200 baud.',
      operation: 'Requires 1.5s boot delay on startup. Streams continuous thermal matrix to Python app for color map JET rendering.',
      pinouts: [
        { name: 'VIN / GND', target: '3.3V/5V Power Rail / Common Ground' },
        { name: 'TX / RX', target: 'Connected to ESP32-Main GPIO 16 (RX2) / GPIO 17 (TX2)' }
      ],
      warning: 'Do not touch the optical germanium lens window with uninsulated fingers.'
    },
    {
      id: 'ultrasonic-sensor',
      num: 8,
      name: 'Ultrasonic Distance Sensor',
      category: 'sensor',
      badge: 'badge-sensor',
      type: 'HC-SR04 Proximity Sensor Module',
      x: 89.1,
      y: 13.8,
      description: 'Emits 40kHz ultrasonic sound bursts to measure physical distance and detect forward obstacles.',
      operation: 'Calculates object distance from Echo pulse width. Sends obstacle telemetry to ESP32-Main for collision handling.',
      pinouts: [
        { name: 'VCC / GND', target: '5V Power Rail / Common Ground' },
        { name: 'Trig / Echo', target: 'ESP32 GPIO Distance Telemetry Lines' }
      ],
      warning: 'Use a voltage divider on 5V Echo pulse if connecting directly to 3.3V GPIO input pins.'
    },
    {
      id: 'oled-display',
      num: 9,
      name: 'SSD1306 0.96" OLED Display (128x64)',
      category: 'display',
      badge: 'badge-display',
      type: 'Monochrome Graphic I2C OLED Display',
      x: 76.1,
      y: 49.6,
      description: 'Provides live onboard status readouts: WiFi State, IP Address, Python App Connection, MLX Health, and Motor Speeds.',
      operation: 'Initialized automatically at I2C address 0x3C or 0x3D. Updates telemetry readout every 250ms.',
      pinouts: [
        { name: 'GND / VDD', target: 'Common Ground / 5V System Power Rail' },
        { name: 'SCK / SDA', target: 'ESP32-Main GPIO 22 (SCL) / GPIO 21 (SDA)' }
      ],
      warning: 'Verify SDA and SCL pin labels on your breakout board; some displays reverse GND and VDD order.'
    },
    {
      id: 'motor-driver',
      num: 10,
      name: 'L9110S Dual Motor Driver Module',
      category: 'motor',
      badge: 'badge-motor',
      type: 'Dual H-Bridge Motor Driver (800mA per channel)',
      x: 76.6,
      y: 68.4,
      description: 'Drives dual DC motor propellers with PWM speed ramping control (-255 reverse to +255 forward).',
      operation: 'Receives PWM signals from ESP32-Main channels 0-3. Software ramping prevents rapid current spikes.',
      pinouts: [
        { name: 'VCC / GND', target: '5V System Power Rail / Common Ground' },
        { name: 'A-IA / A-IB', target: 'ESP32 GPIO 32 / GPIO 33 (Motor A Channel)' },
        { name: 'B-IA / B-IB', target: 'ESP32 GPIO 25 / GPIO 26 (Motor B Channel)' },
        { name: 'MOTOR A / MOTOR B', target: 'Screw terminal outputs connected to DC Motors' }
      ],
      warning: 'Ensure screw terminals are fully tightened. Loose wires cause high voltage spikes during motor direction reversals.'
    },
    {
      id: 'active-buzzer',
      num: 11,
      name: 'Active Piezo Alarm Buzzer',
      category: 'sensor',
      badge: 'badge-sensor',
      type: '5V Active Piezo Tone Generator',
      x: 59.1,
      y: 37.4,
      description: 'Sounds an audible warning beep when YOLO AI detection identifies a person in the camera feed.',
      operation: 'Driven by GPIO 27 HIGH pulse. Beeps for 150ms whenever person_count > 0 in server response.',
      pinouts: [
        { name: 'VCC (+)', target: 'ESP32 GPIO 27 (Digital Output)' },
        { name: 'GND (-)', target: 'Common Ground Rail' }
      ],
      warning: 'Requires an active buzzer with built-in oscillator. Passive buzzers require PWM square wave signal.'
    },
    {
      id: 'battery-pack',
      num: 12,
      name: '3S 1300mAh LiPo/Li-ion Battery Pack',
      category: 'power',
      badge: 'badge-power',
      type: '3-Series Lithium Battery Pack (~11.1V - 12.6V)',
      x: 65.9,
      y: 90.9,
      description: 'Main DC energy source providing high-current output to drive regulators and dual propulsion motors.',
      operation: 'Connects directly to 3S charger module input terminals. Delivers clean power when Rocker Switch is set to Position I.',
      pinouts: [
        { name: 'BAT+ (Red)', target: 'Switched Positive Rail via Rocker Switch' },
        { name: 'BAT- (Black)', target: 'System Common Ground Rail' }
      ],
      warning: 'Do not allow bare positive and ground leads to touch. Wrap all battery joints in heat-shrink tubing.'
    }
  ];

  // Render Hotspots
  const imageWrapper = document.getElementById('board-wrapper');
  if (imageWrapper) {
    components.forEach(comp => {
      const hs = document.createElement('div');
      hs.className = `hotspot hotspot-${comp.category}`;
      hs.style.left = `${comp.x}%`;
      hs.style.top = `${comp.y}%`;
      hs.dataset.id = comp.id;

      hs.innerHTML = `
        <div class="hotspot-pulse"></div>
        <div class="hotspot-inner">${comp.num}</div>
        <div class="hotspot-tooltip">${comp.name}</div>
      `;

      hs.addEventListener('click', () => selectComponent(comp, hs));
      imageWrapper.appendChild(hs);
    });
  }

  // Select Component Handler
  function selectComponent(comp, hsElement) {
    document.querySelectorAll('.hotspot').forEach(el => el.classList.remove('active'));
    if (hsElement) hsElement.classList.add('active');

    const sidebar = document.getElementById('component-sidebar');
    if (!sidebar) return;

    sidebar.innerHTML = `
      <span class="card-header-badge ${comp.badge}">${comp.category.toUpperCase()}</span>
      <h2 class="card-title">#${comp.num}. ${comp.name}</h2>
      <div class="card-subtitle">${comp.type}</div>
      <p class="card-desc">${comp.description}</p>
      
      <div class="info-group">
        <div class="info-label">Operating Protocol</div>
        <div class="info-value" style="white-space: pre-line;">${comp.operation}</div>
      </div>

      <div class="info-group">
        <div class="info-label">Wiring & Pinout Connections</div>
        <ul class="pinout-list">
          ${comp.pinouts.map(p => `
            <li class="pinout-item">
              <span class="pin-name">${p.name}</span>
              <span class="pin-target">${p.target}</span>
            </li>
          `).join('')}
        </ul>
      </div>

      ${comp.warning ? `
        <div class="callout callout-warning">
          <div class="callout-title">PRECAUTION</div>
          <div class="callout-body">${comp.warning}</div>
        </div>
      ` : ''}
    `;
  }

  // Zoom & Pan Image Controls
  let zoomLevel = 1.0;
  let hotspotsVisible = true;

  const btnZoomIn = document.getElementById('zoom-in');
  const btnZoomOut = document.getElementById('zoom-out');
  const btnZoomReset = document.getElementById('zoom-reset');
  const btnToggleHs = document.getElementById('toggle-hotspots');

  if (imageWrapper) {
    if (btnZoomIn) {
      btnZoomIn.addEventListener('click', () => {
        zoomLevel = Math.min(zoomLevel + 0.25, 2.2);
        imageWrapper.style.transform = `scale(${zoomLevel})`;
      });
    }

    if (btnZoomOut) {
      btnZoomOut.addEventListener('click', () => {
        zoomLevel = Math.max(zoomLevel - 0.25, 1.0);
        imageWrapper.style.transform = `scale(${zoomLevel})`;
      });
    }

    if (btnZoomReset) {
      btnZoomReset.addEventListener('click', () => {
        zoomLevel = 1.0;
        imageWrapper.style.transform = `scale(1.0)`;
      });
    }

    if (btnToggleHs) {
      btnToggleHs.addEventListener('click', () => {
        hotspotsVisible = !hotspotsVisible;
        document.querySelectorAll('.hotspot').forEach(el => {
          el.style.display = hotspotsVisible ? 'flex' : 'none';
        });
        btnToggleHs.classList.toggle('active', !hotspotsVisible);
        btnToggleHs.textContent = hotspotsVisible ? 'Hotspots: ON' : 'Hotspots: OFF';
      });
    }
  }

  // Navigation Tab Manager
  const navTabs = document.querySelectorAll('.nav-tab');
  const tabContents = document.querySelectorAll('.tab-content');

  navTabs.forEach(tab => {
    tab.addEventListener('click', () => {
      const targetTab = tab.dataset.tab;

      navTabs.forEach(t => t.classList.remove('active'));
      tabContents.forEach(c => c.classList.remove('active'));

      tab.classList.add('active');
      const targetElement = document.getElementById(`tab-${targetTab}`);
      if (targetElement) {
        targetElement.classList.add('active');
      }
    });
  });

  // Interactive WASD Steering Simulator & Motor LED Meters
  const keys = {
    w: document.getElementById('key-w'),
    a: document.getElementById('key-a'),
    s: document.getElementById('key-s'),
    d: document.getElementById('key-d')
  };

  const statusDisplay = document.getElementById('wasd-sim-status');
  const meterValA = document.getElementById('meter-val-a');
  const meterValB = document.getElementById('meter-val-b');
  const meterBarA = document.getElementById('meter-bar-a');
  const meterBarB = document.getElementById('meter-bar-b');

  function updateWASDStatus(pressedSet) {
    let speedA = 0;
    let speedB = 0;
    let modeText = 'STOPPED';

    if (pressedSet.has('w') && pressedSet.has('a')) {
      speedA = 102; speedB = 255; modeText = 'FORWARD LEFT';
    } else if (pressedSet.has('w') && pressedSet.has('d')) {
      speedA = 255; speedB = 102; modeText = 'FORWARD RIGHT';
    } else if (pressedSet.has('w')) {
      speedA = 255; speedB = 255; modeText = 'FULL FORWARD';
    } else if (pressedSet.has('s') && pressedSet.has('a')) {
      speedA = -102; speedB = -255; modeText = 'REVERSE LEFT';
    } else if (pressedSet.has('s') && pressedSet.has('d')) {
      speedA = -255; speedB = -102; modeText = 'REVERSE RIGHT';
    } else if (pressedSet.has('s')) {
      speedA = -255; speedB = -255; modeText = 'FULL REVERSE';
    } else if (pressedSet.has('a')) {
      speedA = -255; speedB = 255; modeText = 'SPIN LEFT';
    } else if (pressedSet.has('d')) {
      speedA = 255; speedB = -255; modeText = 'SPIN RIGHT';
    }

    if (statusDisplay) {
      statusDisplay.textContent = `STEERING: [${modeText}]`;
    }

    if (meterValA) meterValA.textContent = `${speedA}`;
    if (meterValB) meterValB.textContent = `${speedB}`;

    if (meterBarA) {
      const pctA = Math.abs(speedA) / 255 * 100;
      meterBarA.style.width = `${pctA}%`;
    }

    if (meterBarB) {
      const pctB = Math.abs(speedB) / 255 * 100;
      meterBarB.style.width = `${pctB}%`;
    }
  }

  const activeKeys = new Set();

  // Attach mouse/touch click events to on-screen WASD key caps
  Object.keys(keys).forEach(k => {
    const btn = keys[k];
    if (btn) {
      btn.addEventListener('mousedown', () => {
        activeKeys.add(k);
        btn.classList.add('pressed');
        updateWASDStatus(activeKeys);
      });

      btn.addEventListener('mouseup', () => {
        activeKeys.delete(k);
        btn.classList.remove('pressed');
        updateWASDStatus(activeKeys);
      });

      btn.addEventListener('mouseleave', () => {
        if (activeKeys.has(k)) {
          activeKeys.delete(k);
          btn.classList.remove('pressed');
          updateWASDStatus(activeKeys);
        }
      });
    }
  });

  window.addEventListener('keydown', (e) => {
    const k = e.key.toLowerCase();
    if (['w', 'a', 's', 'd'].includes(k)) {
      activeKeys.add(k);
      if (keys[k]) keys[k].classList.add('pressed');
      updateWASDStatus(activeKeys);
    }
  });

  window.addEventListener('keyup', (e) => {
    const k = e.key.toLowerCase();
    if (['w', 'a', 's', 'd'].includes(k)) {
      activeKeys.delete(k);
      if (keys[k]) keys[k].classList.remove('pressed');
      updateWASDStatus(activeKeys);
    }
  });

  // Troubleshooting Category Filter & Search
  const searchInput = document.getElementById('trouble-search');
  const troubleItems = document.querySelectorAll('.trouble-item');
  const filterPills = document.querySelectorAll('.filter-pill');

  let currentCategory = 'all';

  function filterTroubleshooting() {
    const query = searchInput ? searchInput.value.toLowerCase().trim() : '';

    troubleItems.forEach(item => {
      const itemCat = item.dataset.category || 'all';
      const text = item.textContent.toLowerCase();

      const matchesCat = (currentCategory === 'all' || itemCat === currentCategory);
      const matchesSearch = (!query || text.includes(query));

      if (matchesCat && matchesSearch) {
        item.style.display = 'block';
      } else {
        item.style.display = 'none';
      }
    });
  }

  filterPills.forEach(pill => {
    pill.addEventListener('click', () => {
      filterPills.forEach(p => p.classList.remove('active'));
      pill.classList.add('active');
      currentCategory = pill.dataset.filter;
      filterTroubleshooting();
    });
  });

  if (searchInput) {
    searchInput.addEventListener('input', filterTroubleshooting);
  }

  // Initial Component Select
  selectComponent(components[0]);
});


