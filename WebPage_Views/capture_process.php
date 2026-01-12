<?php
// capture_process.php
// Behavior:
// - Initial POST (from schedule_captures.php): render a page with a loading animation and JS
//   that will call this same file with action=run to execute the fruits_write executable.
// - AJAX POST with action=run: run the executable and return JSON containing the output.


// Define the paths to the comms file and executable
$file_path = '/../Executables/comm_files/process_antenna_movements.txt';
$executable_path = '/../Executables/process_antenna_movements';

// Helper: safe fetch of POST values
function post_val($k) {
  // A post_val function to safely get POST values without warnings
  return isset($_POST[$k]) ? $_POST[$k] : null;
}

// If this is an AJAX run request, execute the executable and return JSON
if ($_SERVER['REQUEST_METHOD'] === 'POST' && post_val('action') === 'run') {
  // 0. Set JSON response header
  header('Content-Type: application/json; charset=utf-8');

  // 1. Get the satellite name (accept either 'satellite' or legacy 'safe_name')
  $sat = null;
  if (isset($_POST['satellite'])) {
    $sat = trim($_POST['satellite']);
  } elseif (isset($_POST['safe_name'])) {
    $sat = trim($_POST['safe_name']);
  } else {
    $sat = '';
  }
  $satellite_safe = $sat !== '' ? htmlspecialchars($sat, ENT_QUOTES, 'UTF-8') : null;

  // 1b. Constellation letter (may be posted from schedule_captures.php). Default to 'S'.
  $const_letter = null;
  if (post_val('constellation') !== null) {
    $const_letter = trim(post_val('constellation'));
  } elseif (isset($_POST['constellation'])) {
    $const_letter = trim($_POST['constellation']);
  }
  if ($const_letter === null || $const_letter === '') $const_letter = 'S';

  // 2. Build executable path (relative to this file) and check it exists and it's executable
  $exec_path = realpath(__DIR__ . $executable_path);
  if ($exec_path === false || !file_exists($exec_path)) {
    // Executable file not found
    echo json_encode(['success' => false, 'error' => 'Executable not found at expected location.']);
    exit;
  }
  if (!is_executable($exec_path)) {
    // Executable not executable
    echo json_encode(['success' => false, 'error' => 'Executable is not executable (check permissions).']);
    exit;
  }

  // 3. Execute the program, capture stdout and stderr
  // 3.1. Prepare descriptors to read and write data to and from the process
  $descriptorspec = [
    0 => ['pipe', 'r'], // stdin (pipe to write)
    1 => ['pipe', 'w'], // stdout (pipe to read)
    2 => ['pipe', 'w']  // stderr (pipe to read errors)
  ];

  // 3.2. Start a new process that will execute the executable with two arguments: satellite and constellation letter
  // Use escapeshellarg for arguments to avoid injection and preserve spacing
  $cmd = escapeshellcmd($exec_path) . ' ' . escapeshellarg($sat) . ' ' . escapeshellarg($const_letter);
  $proc = proc_open($cmd, $descriptorspec, $pipes);
  if (!is_resource($proc)) {
    echo json_encode(['success' => false, 'error' => 'Failed to start executable (proc_open).']);
    exit;
  }

  // 3.3. close stdin immediately, we don't need to send any input
  fclose($pipes[0]);

  // 3.4. Read stdout and stderr
  $stdout = stream_get_contents($pipes[1]);
  fclose($pipes[1]);

  $stderr = stream_get_contents($pipes[2]);
  fclose($pipes[2]);

  // 3.5. Get the exit status
  $status = proc_close($proc);

  // 4. Return JSON with the results
  echo json_encode([
    'success' => true,
    'satellite' => $satellite_safe,
    'exit_code' => $status,
    'stdout' => $stdout,
    'stderr' => $stderr,
  ]);
  
  exit;
}

// Otherwise render the initial page (POST from the schedule form or direct GET)
$satellite = null;
if (isset($_POST['satellite'])) {
  $satellite = trim($_POST['satellite']);
} elseif (isset($_POST['safe_name'])) {
  $satellite = trim($_POST['safe_name']);
} elseif (isset($_GET['satellite'])) {
  $satellite = trim($_GET['satellite']);
}
$satellite_safe = $satellite !== null ? htmlspecialchars($satellite, ENT_QUOTES, 'UTF-8') : '';
?>

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Capture Process</title>
    <link rel="stylesheet" href="../WebPage_Style/basic_style.css">
    <style>
        /* Simple centered loading spinner */
        .loader {
            border: 8px solid #f3f3f3;
            border-top: 8px solid #444;
            border-radius: 50%;
            width: 60px;
            height: 60px;
            animation: spin 1s linear infinite;
            margin: 2em auto;
        }
        @keyframes spin { 100% { transform: rotate(360deg); } }
        .output-box { white-space: pre-wrap; background:#f8f8f8; padding:1em; border:1px solid #ddd; margin-top:1em; }
    </style>
</head>
<body>
    <a href="../index.php" class="small-button">Home</a>
    <h1>Capture Process</h1>
    <hr>

   
        <p>Selected satellite: <strong><?php echo $satellite_safe; ?></strong></p>

        <div id="loader" class="loader" aria-hidden="false"></div>
        <p id="status">Scheduling capture and running executable... Please wait.</p>
        <div id="result" class="output-box" style="display:none"></div>

        <script>
        // On load, call the server to run the executable and show output
        (function(){
            const status = document.getElementById('status');
            const loader = document.getElementById('loader');
            const result = document.getElementById('result');

      fetch(window.location.href, {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: new URLSearchParams({ action: 'run', satellite: <?php echo json_encode($satellite_safe ?? ''); ?>, constellation: <?php echo json_encode(isset($_POST['constellation']) ? $_POST['constellation'] : (isset($_GET['constellation']) ? $_GET['constellation'] : 'S')); ?> })
      })
            .then(resp => resp.json())
            .then(data => {
                loader.style.display = 'none';
                if (!data.success) {
                    status.textContent = 'Execution failed: ' + (data.error || 'unknown error');
                    result.style.display = 'block';
                    result.textContent = data.error || JSON.stringify(data);
                    return;
                }
                status.textContent = 'Execution finished (exit code ' + data.exit_code + ').';
                let out = '';
                if (data.stdout) out += 'STDOUT:\n' + data.stdout + '\n';
                if (data.stderr) out += '\nSTDERR:\n' + data.stderr + '\n';
                result.style.display = 'block';
                result.textContent = out || '(no output)';
            })
            .catch(err => {
                loader.style.display = 'none';
                status.textContent = 'Request failed: ' + err;
            });
        })();
        </script>
    
</body>
</html>



