<?php
// index.php

?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Self-Adjusting Antenna for LEO Satellites</title>
    <link rel="stylesheet" href="WebPage_Style/basic_style.css">
</head>
<body>
    
    <div id="title-bar">
        <h1 id="project-title">Self-Adjusting Antenna for LEO Satellites</h1>
        <img src="WebPage_Style/images/IEEC_logo.png" alt="IEEC Logo" style="height:100px; width:250px;">
        <img src="WebPage_Style/images/UAB_logo.png" alt="UAB Logo" style="height:100px; width:250px;">
    </div>
    <hr>
    <p>(1) Antenna setup and configuration</p>
    <div id="index-buttons">
        <a href="WebPage_Views/hw_calculator.php" class="big-button">Hardware Calculator</a>
        <a href="WebPage_Views/test_movements.php" class="big-button">Test Antenna Movements</a>
        <a href="WebPage_Views/free_control.php" class="big-button">Free Control</a>
    </div>
    
    <hr>
    <p>(2) Schedule captures for:</p>
    <div id="index-buttons">
        <form action="WebPage_Views/schedule_captures.php" method="post" style="display:inline-block; margin:0;">
            <input type="hidden" name="constellation" value="S">
            <button type="submit" class="big-button">Starlink</button>
        </form>
        <form action="WebPage_Views/schedule_captures.php" method="post" style="display:inline-block; margin:0;">
            <input type="hidden" name="constellation" value="O">
            <button type="submit" class="big-button">Orbcom</button>
        </form>
        <form action="WebPage_Views/schedule_captures.php" method="post" style="display:inline-block; margin:0;">
            <input type="hidden" name="constellation" value="I">
            <button type="submit" class="big-button">Iridium</button>
        </form>
    </div>
    <hr>
    <p>In collaboration with:</p>
    
</body>
</html>