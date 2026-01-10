let isExecuting = false; // Flag to prevent concurrent requests

// Map button IDs to their commands (non-movement buttons)
const buttonCommands = {
    'button-test': 'T',
    'button-calibrate': 'C',
    'button-undo': 'B',
    'button-undo-all': 'O',
    'button-force-zero-ninety': 'F'
};

// Map arrow button IDs to their movement commands
const movementCommands = {
    'arrow-left': 'L',
    'arrow-right': 'R',
    'arrow-up': 'U',
    'arrow-down': 'D'
};

// Helper function to disable all control buttons
function disableControls() {
    // Disable all standard buttons
    Object.keys(buttonCommands).forEach(id => {
        const btn = document.getElementById(id);
        if (btn) btn.disabled = true;
    });
    // Disable all movement buttons
    Object.keys(movementCommands).forEach(id => {
        const btn = document.getElementById(id);
        if (btn) btn.disabled = true;
    });
    isExecuting = true;
}

// Helper function to enable all control buttons
function enableControls() {
    // Enable all standard buttons
    Object.keys(buttonCommands).forEach(id => {
        const btn = document.getElementById(id);
        if (btn) btn.disabled = false;
    });
    // Enable all movement buttons
    Object.keys(movementCommands).forEach(id => {
        const btn = document.getElementById(id);
        if (btn) btn.disabled = false;
    });
    isExecuting = false;
}

// Helper function to update display with current position
function updatePositionDisplay(az, el) {
    const displayAz = document.getElementById('display-azimuth');
    const displayEl = document.getElementById('display-elevation');
    if (displayAz) displayAz.innerHTML = ` ${az.toFixed(1)}&deg;`;
    if (displayEl) displayEl.innerHTML = ` ${el.toFixed(1)}&deg;`;
}

// Send antenna control command via AJAX
async function sendAntennaCommand(command, step = null) {
    if (isExecuting) {
        console.warn('Command already executing, please wait...');
        return;
    }

    disableControls();

    try {
        const formData = new URLSearchParams();
        formData.append('action', command);
        if (step !== null) {
            formData.append('step', step);
        }

        const response = await fetch('./antenna_control_api.php', {
            method: 'POST',
            body: formData
        });

        const result = await response.json();

        if (response.ok && result.success) {
            console.log('Command executed:', result);
            if (result.currentAz !== undefined && result.currentEl !== undefined) {
                updatePositionDisplay(result.currentAz, result.currentEl);
            }
        } else {
            console.error('Command failed:', result.error || result);
            alert('Error: ' + (result.error || 'Unknown error'));
        }
    } catch (err) {
        console.error('Network error:', err);
        alert('Network error: ' + err.message);
    } finally {
        enableControls();
    }
}

// Attach click handlers to all standard control buttons
Object.entries(buttonCommands).forEach(([id, command]) => {
    const btn = document.getElementById(id);
    if (btn) {
        btn.addEventListener('click', async () => {
            // Map button ID to LED ID
            const ledId = id.replace('button-', 'led-');
            const led = document.getElementById(ledId);
            
            if (led) {
                led.style.backgroundColor = 'lime'; // Turn on LED
            }
            
            await sendAntennaCommand(command);
            
            if (led) {
                led.style.backgroundColor = ''; // Turn off LED
            }
        });
    }
});

// Attach click handlers to arrow buttons (movement commands with step parameter)
Object.entries(movementCommands).forEach(([id, command]) => {
    const btn = document.getElementById(id);
    if (btn) {
        btn.addEventListener('click', async () => {
            const ledMove = document.getElementById('led-move');
            const stepSize = document.getElementById('step-size');
            const step = stepSize ? stepSize.value : 1;
            
            ledMove.style.backgroundColor = 'lime'; // Bright green
            await sendAntennaCommand(command, step);
            ledMove.style.backgroundColor = ''; // Turn off
        });
    }
});

// Update step-size-value display when slider changes
const stepSizeSlider = document.getElementById('step-size');
const stepSizeValue = document.getElementById('step-size-value');

if (stepSizeSlider && stepSizeValue) {
    stepSizeSlider.addEventListener('input', () => {
        stepSizeValue.innerHTML = stepSizeSlider.value;
    });
}

// Load initial position on page load
window.addEventListener('load', async () => {
    try {
        const response = await fetch('./antenna_control_api.php?get_status=1');
        const result = await response.json();
        if (result.success && result.currentAz !== undefined && result.currentEl !== undefined) {
            updatePositionDisplay(result.currentAz, result.currentEl);
        }
    } catch (err) {
        console.warn('Could not load initial status:', err);
    }
});
