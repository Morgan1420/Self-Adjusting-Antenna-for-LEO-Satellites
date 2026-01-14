from time import time, sleep
from math import degrees
from datetime import datetime
import os
import sys
import helpersGlobalstar


#Position Parameters
[lat, lon, alt] = [41.393552, 2.009787, 0.022] #avenir  [41.4989041,2.1132944,130.0] #uab 
min_elevation = -90 # 75 para directiva in degrees

#Recording parameters
record_length = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5  # seconds (from CLI argument or default)
files_per_satellite = 1 # number of recordings per satellite
total_file_count = 1 # num total of recordings

#TLE file parameters
TLE_FILE_MAX_HOURS = 3

#Function to check if the TLE file is older than TLE_FILE_MAX_HOURS
def check_tle_file_age(tle_filename: str):
    if os.path.getmtime(tle_filename) > datetime.timestamp(datetime.now()) + TLE_FILE_MAX_HOURS * 3600:
        return True
    return False


# Function to read parameters from "capture_conf.txt"
def read_conf_script(script_path):
    params = {}
    with open(script_path, 'r') as file:
        for line in file:
            if line.startswith('set'):
                parts = line.split()
                if len(parts) == 4:
                    key = parts[1]
                    value = parts[3]
                    # Handle unit suffixes
                    if value.endswith('M'):
                        value = float(value[:-1]) * 1e6
                    elif value.endswith('K'):
                        value = float(value[:-1]) * 1e3
                    params[key] = value
    return params

# Satellite parameters 
# Read satellite parameters from "capture_conf.txt"
# conf_params = read_conf_script('capture_conf.txt')
script_dir = os.path.dirname(os.path.abspath(__file__))
conf_params = read_conf_script(os.path.join(script_dir, 'capture_conf.txt'))
center_freq = conf_params['frequency']
bandwidth = conf_params['bandwidth']
fs = conf_params['samplerate']
agc = conf_params['agc']
gain = int(conf_params['gain'])
biastee = conf_params['biastee']

def main_loop(total_file_count: int):
    
    # Create directory to store the data
    currentDir = os.path.dirname(os.path.abspath(__file__))
    directory= os.path.join(currentDir, "data", datetime.now().strftime('%Y%m%d%H%M%S'))
    os.makedirs(directory, exist_ok=True)
    
    
    #BladeRF capture of the satellite
    record_time = helpersGlobalstar.bladeRFdataGlobalstar(record_length, fs, directory)

    

if __name__ == "__main__":
    main_loop(total_file_count)