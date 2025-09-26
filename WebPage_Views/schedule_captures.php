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

<a href="../index.php" class="small-button">Go back</a>
<h1>Schedule a Capture</h1>
<hr>

<p>Select one of the following satellites:</p>


<hr class="dotted">
<b>Filter Satellites</b>
<form>
    <label for='excludeVisible'>Exclude SV already visible</label>
    <input type='checkbox' id='excludeVisible' placeholder='Exclude SV already visible'>
    <label for='excludeDTC'>Exclude DTC SVs</label>
    <input type='checkbox' id='excludeDTC' placeholder='Exclude DTC SVs'>
    <label for="elevationMask">Elevation mask:</label>
    <select id="elevationMask">
        <option value="none">No elevation mask</option>
        <option value="10">Minimum 10° elevation</option>
        <option value="20">Minimum 20° elevation</option>
        <option value="30">Minimum 30° elevation</option>
    </select>
    <label for="sortBy">Sort by:</label>
    <select name="sortBy" id="sortBy">
        <option value="max-elevation">Max Elevation</option>
        <option value="starting-time">Starting Time</option>
        <option value="longer-view-window">Longer View Window</option>
    </select>
    <input type="submit" value="Apply filters">
</form>
<hr class="dotted">
<!-- SV list -->

<?php
// Define the path to your data file
$file_path = '../WebPage_Executables/comm_files/Generate_available_list_of_SV.txt';
// Define the path to your executable
$executable_path = '../WebPage_Executables/Generate_available_list_of_SV_1';

// Check if the C executable exists before trying to run it
if (!file_exists($executable_path) || !is_executable($executable_path)) {
    die("Error: C executable not found or not executable. Please check the path and permissions.");
}

// Execute the C program and redirect its output to the .txt file
// The `> /dev/null 2>&1` redirects output to the trash to prevent it from showing on the page.
exec("{$executable_path} > {$file_path}");

// Now, read the newly created file
if (!file_exists($file_path) || !is_readable($file_path)) {
    die("Error: Data file was not created successfully. Please check the C program's output and file permissions.");
}

// Read the file content into an array of lines
$sv_array = file($file_path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);

// Display the list
echo "<form>";
foreach ($sv_array as $sv) {
    echo "<input type='radio' name='satellite' value='" . htmlspecialchars($sv) . "' id='" . htmlspecialchars($sv) . "'>";
    echo "<label for='" . htmlspecialchars($sv) . "'>" . htmlspecialchars($sv) . "</label><br>";
}
echo "</form>";
?>

</body>
</html>