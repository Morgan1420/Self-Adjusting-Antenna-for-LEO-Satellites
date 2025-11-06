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
<h1>Finished Captures</h1>

<?php
// Allow pressing buttons to invoke the ArduinoTest executable (A = on, B = off)
$exec_result = null;
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['cmd'])) {
    $cmd = $_POST['cmd'] === 'A' ? 'A' : 'B';
    $exec_path = realpath(__DIR__ . '/../Executables/ArduinoTest');
    if ($exec_path === false || !file_exists($exec_path)) {
        $exec_result = "Executable not found at expected location.";
    } elseif (!is_executable($exec_path)) {
        $exec_result = "Executable is not executable (check permissions).";
    } else {
        // Build safe command
        $full_cmd = escapeshellcmd($exec_path) . ' ' . escapeshellarg($cmd);
        // Use proc_open to capture stdout/stderr
        $descriptorspec = [
            0 => ['pipe', 'r'],
            1 => ['pipe', 'w'],
            2 => ['pipe', 'w']
        ];
        $proc = proc_open($full_cmd, $descriptorspec, $pipes);
        if (is_resource($proc)) {
            fclose($pipes[0]);
            $stdout = stream_get_contents($pipes[1]);
            fclose($pipes[1]);
            $stderr = stream_get_contents($pipes[2]);
            fclose($pipes[2]);
            $exit = proc_close($proc);
            $exec_result = "Exit: $exit\n";
            if ($stdout !== '') $exec_result .= "STDOUT:\n" . $stdout . "\n";
            if ($stderr !== '') $exec_result .= "STDERR:\n" . $stderr . "\n";
        } else {
            $exec_result = "Failed to start executable.";
        }
    }
}
?>

<form method="post">
    <button type="submit" name="cmd" value="A" class="big-button">Toggle LED On (A)</button>
    <button type="submit" name="cmd" value="B" class="big-button">Toggle LED Off (B)</button>
</form>

<?php if ($exec_result !== null): ?>
    <h3>Command result</h3>
    <pre style="background:#f8f8f8;padding:1em;border:1px solid #ddd;white-space:pre-wrap;"><?php echo htmlspecialchars($exec_result); ?></pre>
<?php endif; ?>

</body>
</html>