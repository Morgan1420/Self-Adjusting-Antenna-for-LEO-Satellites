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
        <img src="WebPage_Style/images/IEEC_logo.png" alt="IEEC Logo" style="height:100px; width:250px;">
        <h1 id="project-title">Antena Rotatòria Automàtica</h1>
        <img src="WebPage_Style/images/UAB_logo.png" alt="UAB Logo" style="height:100px; width:250px;">
    </div>

    <div id="introduction-section" class="section">
        <div class="section-image-container-right">
            <div class="text">
                <p>Aquesta pàgina ens serveix d'interfície per tal de controlar una antena rotatòria automàtica dissenyada per seguir satèl·lits i facilitar-ne la seva captura del senyal.</p> 
                <p>Concretament el software permet fer dues coses:</p>
                <ul>
                    <li><b>Programar captures</b> de satèl·lits de diferents constel·lacions</li>
                    <li><b>Controlar lliurement</b> l'antena rotatòria per tal de fer proves de moviment i calibratge.</li>
                </ul>
                </div>
            <div class="image">
                <img src="WebPage_Style/images/antenna_photo.jpg" alt="Foto de l'antena rotatòria">
            </div>
        </div>
        <a href="#hardware-mount-instructions-section" style="color: red; text-decoration: none; font-weight: bold; padding-block: 1em;">IMPORTANT: Abans de començar qualsevol captura llegiu les instruccions de muntatge de l'antena rotatòria a la secció corresponent.</a>
    </div>
    <div id="schedule-section" class="section">
        <h2>Programació de captures</h2>
        <p>La principal funcionalitat de l'antena és capturar dades de diferents satèl·lits de diferents constel·lacions, és per això que a continuació trobarem un seguit d'eines per a identificar un satèl·lit concret i ordenar a l'antena que el segueixi.   </p>
        <div class="section-buttons-container">
            <form action="WebPage_Views/schedule_captures.php" method="post" style="display:inline-block; margin:0;" class="button-box">
                <input type="hidden" name="constellation" value="S">
                <button type="submit" class="big-button">Starlink</button>
            </form>
            <form action="WebPage_Views/schedule_captures.php" method="post" style="display:inline-block; margin:0;" class="button-box">
                <input type="hidden" name="constellation" value="O">
                <button type="submit" class="big-button">Orbcom</button>
            </form>
            <form action="WebPage_Views/schedule_captures.php" method="post" style="display:inline-block; margin:0;" class="button-box">
                <input type="hidden" name="constellation" value="I">
                <button type="submit" class="big-button">Iridium</button>
            </form>
            <form action="WebPage_Views/schedule_captures.php" method="post" style="display:inline-block; margin:0;" class="button-box">
                <input type="hidden" name="constellation" value="G">
                <button type="submit" class="big-button">Globalstar</button>
            </form>
        </div>
    </div>
    <div id="free-control-section" class="section">
        <h2>Control lliure de l'antena</h2>
        <p>Ja sigui per calibrar l'antena o per demostrar com funciona podem fer servir el següent controlador. </p>
        <div class="section-buttons-container">
            <a href="WebPage_Views/free_control.php" class="link-as-button">Controlador lliure</a>
        </div>
        
    </div>
    <div id="hardware-mount-instructions-section" class="section">
        <h2>Instruccions muntatge </h2>
        // Encara cal fer
    </div>
    

    
</body>
</html>