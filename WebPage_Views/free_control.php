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
                <button class="cal-button" title="Run calibration">Calibrate</button>
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

</body>
</html>
