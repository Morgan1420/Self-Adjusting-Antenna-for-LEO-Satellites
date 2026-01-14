import requests
import subprocess
from time import sleep 
from typing import Dict
from math import degrees
from datetime import datetime
import helpers
import pytz
import os

GLOBALSTAR_GEN2_SATELLITES = {#satellites launched from 2010 onwards
    'GLOBALSTAR M073',
    'GLOBALSTAR M074',
    'GLOBALSTAR M075',
    'GLOBALSTAR M076',
    'GLOBALSTAR M077',
    'GLOBALSTAR M078',
    'GLOBALSTAR M079',
    'GLOBALSTAR M080',
    'GLOBALSTAR M081',
    'GLOBALSTAR M082',
    'GLOBALSTAR M083',
    'GLOBALSTAR M084',
    'GLOBALSTAR M085',
    'GLOBALSTAR M086',
    'GLOBALSTAR M087',
    'GLOBALSTAR M088',
    'GLOBALSTAR M089',
    'GLOBALSTAR M090',
    'GLOBALSTAR M091',
    'GLOBALSTAR M092',
    'GLOBALSTAR M093',
    'GLOBALSTAR M094',
    'GLOBALSTAR M095',
    'GLOBALSTAR M096',
    'GLOBALSTAR M097',
}

#Download latest Globalstar TLE
def download_tle_Globalstar(tle_filename:str) -> str:
	url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=globalstar&FORMAT=tle"
	r = requests.get(url, allow_redirects=True)
	tle = r.content.decode("utf-8")

	with open("Globalstar.tle", 'w') as fh:
		fh.write(tle)
	
	helpers.clean_tle_file(tle_filename)

#Save the TLE data of active Orbcomm satellites to a file.
def tle_of_active_Globalstar(TLE_path: str) -> None:

    tle_active = "TLE.tle"
    with open(TLE_path, 'r') as inTLE, open(tle_active, 'w') as outTLE:
        for svid in inTLE:
            svid = svid.strip()
            line1 = inTLE.readline()
            line2 = inTLE.readline()
        
            if svid.strip() in GLOBALSTAR_GEN2_SATELLITES:
                outTLE.write(f"{svid}\n{line1}{line2}")
            #Write the TLE lines to the output file

def bladeRFdataGlobalstar(record_length: int, fs: int, directory: str):

    nSamples=record_length*fs #number of samples to record

    # Start an interactive bladeRF CLI session
    process = subprocess.Popen("bladeRF-cli -i", shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        process.stdin.write("run conf_globalstar.script;\n")  # Send command
        process.stdin.flush()  # Ensure the command is sent immediately
        sleep(4)   # esperar 3 segons perque es configuri bé la blade

        filename = os.path.join(directory, 'captura.bin')

        record_time = datetime.now()
        
        process.stdin.write("rx config file=" + filename + " n="+f"{int(nSamples):d}"+"; rx start\n")  
        process.stdin.flush() 
        sleep(record_length+6)

        process.stdin.write("exit;\n")  
        process.stdin.flush() 

        # Capture final output after all commands are executed
        stdout, stderr = process.communicate()
        if stderr:
            print(f"Error: {stderr}")

        # Output the results
        #print("Output:\n", stdout)
        #print("Error:\n", stderr)

        local_tz = pytz.timezone("Europe/Madrid") #CHANGE IF DIFEERENT TIMEZONE
        record_time = local_tz.localize(record_time).astimezone(pytz.utc)

        return record_time
    except Exception as e:
        process.kill()
        raise e

