<?php
// free_control.php
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <title>Free Control</title>
    <link rel="stylesheet" href="../WebPage_Style/basic_style.css">
    <link rel="stylesheet" href="../WebPage_Style/free_control.css">
    
</head>
<body>
    <a href="../index.php" class="small-button">Back</a>
    <h1>Free Control</h1>

    <div class="console-wrap">
        <div class="console-grid">
            <!-- Left: Calibration -->
            <div class="cal-block">
                <span class="cal-label">Calibration</span>
                <button id="calibrate" class="cal-button" title="Run calibration">Calibrate</button>
            </div>

            <!-- Center: D-Pad -->
            <div class="pad-block">
                <div class="dpad" role="group" aria-label="Antenna directional controls">
                    <div class="empty"></div>
                    <button id="up" aria-label="Move up">&#9650;</button>
                    <div class="empty"></div>

                    <button id="left" aria-label="Move left">&#9664;</button>
                    <div class="empty"></div>
                    <button id="right" aria-label="Move right">&#9658;</button>

                    <div class="empty"></div>
                    <button id="down" aria-label="Move down">&#9660;</button>
                    <div class="empty"></div>
                </div>
            </div>

            <!-- Right: Elevation / Azimuth inputs -->
            <div class="controls-block">
                <div class="control-field">
                    <label for="elevation">Elevation (deg)</label>
                    <input id="elevation" name="elevation" type="number" placeholder="e.g. 45" min="-90" max="90">
                </div>
                <div class="control-field">
                    <label for="azimuth">Azimuth (deg)</label>
                    <input id="azimuth" name="azimuth" type="number" placeholder="e.g. 180" min="0" max="360">
                </div>
            </div>
        </div>
    </div>

    <script>
        // Map button IDs to antenna control actions
        const buttonActions = {
            'up': 'T',      // Top/Up
            'down': 'B',    // Bottom/Down
            'left': 'L',    // Left
            'right': 'R',   // Right
            'calibrate': 'C' // Calibrate
        };

        let isExecuting = false; // Flag to prevent concurrent requests

        // Helper function to disable all control buttons
        function disableControls() {
            Object.keys(buttonActions).forEach(id => {
                const btn = document.getElementById(id);
                if (btn) btn.disabled = true;
            });
            isExecuting = true;
        }

        // Helper function to enable all control buttons
        function enableControls() {
            Object.keys(buttonActions).forEach(id => {
                const btn = document.getElementById(id);
                if (btn) btn.disabled = false;
            });
            isExecuting = false;
        }

        // Helper function to update display with current position
        function updatePositionDisplay(az, el) {
            const azInput = document.getElementById('azimuth');
            const elInput = document.getElementById('elevation');
            if (azInput) azInput.value = az;
            if (elInput) elInput.value = el;
        }

        // Send antenna control command via AJAX
        async function sendAntennaCommand(action) {
            if (isExecuting) {
                console.warn('Command already executing, please wait...');
                return;
            }

            disableControls();

            try {
                const formData = new URLSearchParams();
                formData.append('action', action);

                const response = await fetch('./antenna_control_api.php', {
                    method: 'POST',
                    body: formData
                });

                const result = await response.json();

                if (response.ok && result.success) {
                    console.log('Command executed:', result);
                    updatePositionDisplay(result.currentAz, result.currentEl);
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

        // Attach click handlers to all control buttons
        Object.entries(buttonActions).forEach(([id, action]) => {
            const btn = document.getElementById(id);
            if (btn) {
                btn.addEventListener('click', () => {
                    sendAntennaCommand(action);
                });
            }
        });

        // Load initial position on page load
        window.addEventListener('load', async () => {
            try {
                const response = await fetch('./antenna_control_api.php?get_status=1');
                // Since we only support POST, this won't work; instead read from a status endpoint
                // For now, just set placeholders
                console.log('Page loaded, antenna status endpoint not yet implemented for GET');
            } catch (err) {
                console.warn('Could not load initial status:', err);
            }
        });
    </script>
