<?php

// Define the paths to the comms file and executable
$file_path = '../Executables/comm_files/Generate_available_list_of_SV.txt';
$executable_path = '../Executables/Generate_available_list_of_SV';

// Check if the C executable exists before trying to run it
if (!file_exists($executable_path) || !is_executable($executable_path)) {
    die("Error: C executable not found or not executable. Please check the path and permissions.");
}

// Determine selected constellation (may be posted from index.php)
$constellation = null;
if (isset($_POST['constellation'])) {
    $constellation = trim($_POST['constellation']);
} elseif (isset($_GET['constellation'])) {
    $constellation = trim($_GET['constellation']);
}
if ($constellation === null || $constellation === '') $constellation = 'S';

// Handle the filter form submission
if ($_SERVER["REQUEST_METHOD"] == "POST" && (isset($_POST['excludeVisible']) || isset($_POST['elevationMask']) || isset($_POST['SV-to-display']) || isset($_POST['sortBy']))) {

    // 1. Retrieve the data sent from the form
    $excludeVisible = $_POST['excludeVisible'];
    $excludeDTC = $_POST['excludeDTC'];
    $elevationMask = $_POST['elevationMask'];
    $sortBy = $_POST['sortBy'];
    $SV_to_display = $_POST['SV-to-display'];

    // 2. Validate the inputs (and sanitize if necessary)
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
    if ($elevationMask === false || $sortBy === false || $SV_to_display === false) {
        die("Error: Invalid input values. Please ensure all fields are filled out correctly.");
    }

    // 4. Prepare the command with arguments
    $command = escapeshellcmd("{$executable_path} {$excludeVisible} {$excludeDTC} {$elevationMask} {$sortBy} {$SV_to_display}");

    // 5. Execute the command
    exec("{$command} > {$file_path} 2>&1", $output, $return_var);
    if ($return_var !== 0) {
        die("Error: C program execution failed. Please check the command and file permissions.");
    }
} else {
    // If not a POST filter request, just run the executable with default parameters
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

// Parse each line into structured fields: name, start, end, max_elevation
$sv_objects = [];
// Regex to parse lines like: "Satellite: SAT_NAME, Start: START_TIME, End: END_TIME, Max Elevation: XX.X deg"
$re = '/^Satellite:\s*(.+?),\s*Start:\s*([^,]+),\s*End:\s*([^,]+),\s*Max Elevation:\s*([0-9.+-]+)\s*deg/i';
foreach ($sv_array as $line) {
    if (preg_match($re, $line, $m)) {
        $sv_objects[] = [
            'raw' => $line,
            'name' => trim($m[1]),
            'start' => trim($m[2]),
            'end' => trim($m[3]),
            'max_elevation' => floatval($m[4]),
        ];
    } else {
        // fallback: keep whole line as name if it doesn't match
        $sv_objects[] = [
            'raw' => $line,
            'name' => trim($line),
            'start' => null,
            'end' => null,
            'max_elevation' => null,
        ];
    }
}
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
    <p>Constellation: <strong><?php
        $map = ['S' => 'Starlink', 'O' => 'Orbcomm', 'I' => 'Iridium'];
        echo isset($map[$constellation]) ? htmlspecialchars($map[$constellation]) : htmlspecialchars($constellation);
    ?></strong></p>
    <hr>

    <p>Select one of the following satellites:</p>


    <hr class="dotted">
    <b>Filter Satellites</b>

    <!-- Form to filter satellites -->
    <form action="/WebPage_Views/schedule_captures.php" method="post">
        <input type="hidden" name="constellation" value="<?php echo htmlspecialchars($constellation); ?>">
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

    <!-- Form to schedule captures -->
    <form id="schedule-form" action="/WebPage_Views/capture_process.php" method="post">
        <input type="hidden" name="constellation" value="<?php echo htmlspecialchars($constellation); ?>">
        <?php
        $idx = 0;
        foreach ($sv_objects as $obj) {
            $idx++;
            $safe_name = htmlspecialchars($obj['name'], ENT_QUOTES, 'UTF-8');
            $safe_start = htmlspecialchars($obj['start'] ?? '', ENT_QUOTES, 'UTF-8');
            $safe_end = htmlspecialchars($obj['end'] ?? '', ENT_QUOTES, 'UTF-8');
            $safe_max = isset($obj['max_elevation']) ? htmlspecialchars((string)$obj['max_elevation'], ENT_QUOTES, 'UTF-8') : '';

            // radio with data attributes for start/end/max
            echo "<input type='radio' name='satellite' value='{$safe_name}' id='sv_{$idx}' data-start='{$safe_start}' data-end='{$safe_end}' data-max='{$safe_max}'>";
            echo "<label for='sv_{$idx}'>" . $safe_name;
            if ($safe_start !== '') {
                echo " — <small>From {$safe_start} to {$safe_end}, max {$safe_max}°</small>";
            }
            echo "</label><br>\n";
        }
        ?>

        <!-- Hidden fields that will be populated with the chosen satellite's details on submit -->
        <input type="hidden" name="start" id="selected_start" value="">
        <input type="hidden" name="end" id="selected_end" value="">
        <input type="hidden" name="max_elevation" id="selected_max" value="">

        <input type="submit" value="Schedule Capture">
    </form>

    <script>
        // Before submitting, copy the selected radio's data-* attributes into hidden inputs
        document.getElementById('schedule-form').addEventListener('submit', function (e) {
            const checked = document.querySelector('input[name="satellite"]:checked');
            if (!checked) {
                e.preventDefault();
                alert('Please select a satellite first.');
                return false;
            }
            document.getElementById('selected_start').value = checked.dataset.start || '';
            document.getElementById('selected_end').value = checked.dataset.end || '';
            document.getElementById('selected_max').value = checked.dataset.max || '';
            return true;
        });
    </script>


</body>

</html>