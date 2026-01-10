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
    <script src="../WebPage_Scripts/free_control.js" defer></script>
</head>
<body>
    <div id="title-section">
        <a href="../index.php" class="home-button"><img src="../WebPage_Style/images/home_icon.png" alt="Go back"></a>
        <h1>Controlador Lliure</h1>
        <a href="./" class="small-button"></a> <!-- Placeholder <a> -->
    </div>

    <div id="console">
        <div id="console-buttons-section">
            <div class="horizontal-button-panel">
                <div class="button-element">
                    <div class="button-label">Test</div>
                    <div class="button-button-and-led">
                        <button id="button-test" class="button-button" title="Run test"><i><b>T</b></i></button>
                        <div id="led-test" class="led"></div>
                    </div>
                </div>
                <div class="button-element">
                    <div class="button-label">Calibrate</div>
                    <div class="button-button-and-led">
                        <button id="button-calibrate" class="button-button" title="Run calibration"><i><b>C</b></i></button>
                        <div id="led-calibrate" class="led"></div>
                    </div>
                </div>
            </div>

            <div class="horizontal-button-panel">
                <div class="button-element">
                    <div class="button-label">Undo</div>
                    <div class="button-button-and-led">
                        <button id="button-undo" class="button-button" title="Undo last movement"><i><b>B</b></i></button>
                        <div id="led-undo" class="led"></div>
                    </div>
                </div>
                <div class="button-element">
                    <div class="button-label">Undo all</div>
                    <div class="button-button-and-led">
                        <button id="button-undo-all" class="button-button" title="Return to origin"><i><b>O</b></i></button>
                        <div id="led-undo-all" class="led"></div>
                    </div>
                </div>
            </div>
            
        </div>
        
        <div id="console-display-section">
            <div id="console-display">
                <div class="display-row">
                    <span class="display-label">AZ</span>
                    <span id="display-azimuth" class="display-value"> --.-&deg;</span>
                </div>
                <div class="display-row">
                    <span class="display-label">EL</span>
                    <span id="display-elevation" class="display-value"> --.-&deg;</span>
                </div>
            </div>
            <div id="force-zero-ninety">
                <button id="button-force-zero-ninety">Force 0-90</button>
            </div>
        </div>
        <div id="console-arrows-section">
            <div id="move-arrows-section">
                <div class="move-arrows-element">
                    <div class="button-label">Azimuth</div>
                    <div class="arrow-button-pair">
                        <button id="arrow-left" class="arrow-button" title="Move Left">&#9664;</button>
                        <button id="arrow-right" class="arrow-button" title="Move Right">&#9658;</button>
                    </div>
                </div>
                <div id="led-move" class="led"></div>
                <div class="move-arrows-element">
                    <div class="button-label">Elevation</div>
                    <div class="arrow-button-pair">
                        <button id="arrow-up" class="arrow-button" title="Move Up">&#9650;</button>
                        <button id="arrow-down" class="arrow-button" title="Move Down">&#9660;</button>
                    </div>
                </div>
            </div>
            <div id="move-step-selection-section">
                <div class="step-slider-label">Step Size</div>
                <input type="range" id="step-size" min="1" max="20" value="1" step="1">
                <div id="step-size-value">1</div>
            </div>
        </div>
    </div>

</body>
</html>
