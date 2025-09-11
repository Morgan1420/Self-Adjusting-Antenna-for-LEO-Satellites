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

<!-- SV list -->

<?php
// Define the path to your data file
$file_path = '../WebPage_Executables/comm_files/fruits_comm.txt';
// Define the path to your executable
$executable_path = '../WebPage_Executables/fruits_write';

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
$fruits_array = file($file_path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);

// Display the list
echo "<h2>List of Fruits</h2>";
echo "<ul>";
foreach ($fruits_array as $fruit) {
    echo "<li>" . htmlspecialchars($fruit) . "</li>";
}
echo "</ul>";
?>

</body>
</html>