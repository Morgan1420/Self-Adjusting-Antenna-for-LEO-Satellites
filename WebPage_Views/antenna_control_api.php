<?php
/**
 * antenna_control_api.php
 * 
 * Backend API for antenna control via moveAntenna executable.
 * Expects POST with:
 *   - action: C (calibrate), T (top), B (bottom), L (left), R (right), O (original)
 * 
 * Returns JSON with status and current position.
 */

header('Content-Type: application/json');

$json_path = '../Executables/comm_files/antennaStatus.json';
$executable_path = '../Executables/moveAntenna';

// Validate inputs
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(400);
    echo json_encode(['error' => 'Only POST requests allowed']);
    exit;
}

$action = isset($_POST['action']) ? trim($_POST['action']) : null;

if (!$action || strlen($action) !== 1) {
    http_response_code(400);
    echo json_encode(['error' => 'Missing or invalid action parameter']);
    exit;
}

// Validate action is one of the allowed values
$valid_actions = ['C', 'T', 'B', 'L', 'R', 'O'];
if (!in_array(strtoupper($action), $valid_actions)) {
    http_response_code(400);
    echo json_encode(['error' => 'Invalid action. Must be one of: ' . implode(', ', $valid_actions)]);
    exit;
}

// Check if JSON file exists; if not, create it
if (!file_exists($json_path)) {
    $default_json = <<<JSON
{
  "currentAz": 0,
  "currentEl": 0,
  "LastMovements": [
  ]
}
JSON;
    if (!@file_put_contents($json_path, $default_json)) {
        http_response_code(500);
        echo json_encode(['error' => 'Failed to create antenna status file']);
        exit;
    }
}

// Check if executable exists and is executable
if (!file_exists($executable_path) || !is_executable($executable_path)) {
    http_response_code(500);
    echo json_encode(['error' => 'moveAntenna executable not found or not executable']);
    exit;
}

// Execute the moveAntenna command
$command = escapeshellarg($executable_path) . ' ' . escapeshellarg($json_path) . ' ' . escapeshellarg($action);
$output = array();
$return_var = 0;

exec($command . ' 2>&1', $output, $return_var);

// Read the updated JSON to return current position
$json_content = @file_get_contents($json_path);
$json_data = null;
if ($json_content) {
    $json_data = json_decode($json_content, true);
}

if ($return_var !== 0) {
    http_response_code(500);
    echo json_encode([
        'error' => 'moveAntenna command failed',
        'return_code' => $return_var,
        'output' => implode("\n", $output),
        'currentAz' => $json_data['currentAz'] ?? null,
        'currentEl' => $json_data['currentEl'] ?? null
    ]);
    exit;
}

// Success response
http_response_code(200);
echo json_encode([
    'success' => true,
    'action' => strtoupper($action),
    'currentAz' => $json_data['currentAz'] ?? 0,
    'currentEl' => $json_data['currentEl'] ?? 0,
    'message' => implode("\n", $output)
]);
?>
