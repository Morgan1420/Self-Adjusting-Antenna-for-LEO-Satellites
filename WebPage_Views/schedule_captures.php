<?php

// Define the paths to the comms file and executable
$file_path = '../WebPage_Executables/comm_files/Generate_available_list_of_SV.txt';
$executable_path = '../WebPage_Executables/Generate_available_list_of_SV';

// Check if the C executable exists before trying to run it
if (!file_exists($executable_path) || !is_executable($executable_path)) {
    die("Error: C executable not found or not executable. Please check the path and permissions.");
}

// Handle the form submission
if ($_SERVER["REQUEST_METHOD"] == "POST") {

    // 1. Access the data using the 'name' attributes
    $excludeVisible = $_POST['excludeVisible'];
    $excludeDTC = $_POST['excludeDTC'];
    $elevationMask = $_POST['elevationMask'];
    $sortBy = $_POST['sortBy'];
    $SV_to_display = $_POST['SV-to-display'];

    echo "Raw Exclude Visible: " . $excludeVisible . "<br>";
    echo "Raw Exclude DTC: " . $excludeDTC . "<br>";
    echo "Raw Elevation Mask: " . $elevationMask . "<br>";
    echo "Raw Sort By: " . $sortBy . "<br>";
    echo "Raw SV to Display: " . $SV_to_display . "<br>";

    // 2. Validate and sanitize the inputs
    $excludeVisible = ($excludeVisible === '1') ? 1 : 0;
    $excludeDTC = ($excludeDTC === '1') ? 1 : 0;
    $elevationMask = filter_var($elevationMask, FILTER_VALIDATE_INT, [
        'options' => ['min_range' => 0, 'max_range' => 90]
    ]);
    $sortBy = filter_var($sortBy, FILTER_VALIDATE_INT, [
        'options' => ['min_range' => 1, 'max_range' => 3]
    ]);
    $SV_to_display = filter_var($SV_to_display, FILTER_VALIDATE_INT, [
        'options' => ['min_range' => 1, 'max_range' => 50]
    ]);

    // 3. Check for validation errors
    if ($elevationMask === false || $sortBy === false || $SV_to_display === false) {
        die("Error: Invalid input values. Please ensure all fields are filled out correctly.");
    }

    // 4. Prepare the command with arguments
    $command = escapeshellcmd("{$executable_path} {$excludeVisible} {$excludeDTC} {$elevationMask} {$sortBy} {$SV_to_display}");
    echo "Prepared Command: " . $command . "<br>";

    // 5. Execute the command
    exec("{$command} > {$file_path} 2>&1", $output, $return_var);
    if ($return_var !== 0) {
        die("Error: C program execution failed. Please check the command and file permissions.");
    }
} else {
    // If not a POST request, just run the executable with default parameters
    exec("{$executable_path} > {$file_path} 2>&1");

    // Fill the variables with default values
    $excludeVisible = 0;
    $excludeDTC = 0;
    $elevationMask = 10;
    $sortBy = 1;
    $SV_to_display = 10;
}


// Now, read the newly created file
if (!file_exists($file_path) || !is_readable($file_path)) {
    die("Error: Data file was not created successfully. Please check the C program's output and file permissions.");
}

// Read the file content into an array of lines
$sv_array = file($file_path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
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
    <form action="/WebPage_Views/schedule_captures.php" method="post">
        <label for='excludeVisible'>Exclude SV already visible</label>
        <input type='checkbox' id='excludeVisible' name='excludeVisible' value='1' <?php if ($excludeVisible) echo 'checked'; ?>>
        <label for='excludeDTC'>Exclude DTC SVs</label>
        <input type='checkbox' id='excludeDTC' name='excludeDTC' value='1' <?php if ($excludeDTC) echo 'checked'; ?>>
        <label for="elevationMask">Elevation mask:</label>
        <input type="number" id="elevationMask" name="elevationMask" min="0" max="90" value="<?php echo htmlspecialchars($elevationMask); ?>"> degrees
        <label for="sortBy">Sort by:</label>
        <select name="sortBy" id="sortBy">
            <option value="1" <?php if ($sortBy == 1) echo 'selected'; ?>>Max Elevation</option>
            <option value="2" <?php if ($sortBy == 2) echo 'selected'; ?>>Starting Time</option>
            <option value="3" <?php if ($sortBy == 3) echo 'selected'; ?>>Longer View Window</option>
        </select>
        <label for="SV-to-display">Number of SVs to display (1-50):</label>
        <input type="number" id="SV-to-display" name="SV-to-display" min="1" max="50" value="<?php echo htmlspecialchars($SV_to_display); ?>">
        <input type="submit" value="Apply filters">
    </form>
    <hr class="dotted">
    <!-- SV list -->


    <!-- Display the list -->
    <form>

        <?php
        foreach ($sv_array as $sv) {
            echo "<input type='radio' name='satellite' value='" . htmlspecialchars($sv) . "' id='" . htmlspecialchars($sv) . "'>";
            echo "<label for='" . htmlspecialchars($sv) . "'>" . htmlspecialchars($sv) . "</label><br>";
        }
        ?>

    </form>


</body>

</html>