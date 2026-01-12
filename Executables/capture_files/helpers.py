import ephem
import os

# Function that cleans the TLE file
def clean_tle_file(file_path):
    with open(file_path, 'r') as infile:
        lines = infile.readlines()
        
    # Keep only non-empty lines
    cleaned_lines = [line for line in lines if line.strip() != ""]

    # Write cleaned lines back to the same file
    with open(file_path, 'w') as outfile:
        outfile.writelines(cleaned_lines)

# Function that sets the observer 
def setObserver(latitude:int, longitude:int, elevation:int):
	observer = ephem.Observer()
	observer.lat = str(latitude)
	observer.lon = str(longitude)
	observer.elevation = elevation

	print(f"Observer set to: Latitude = {observer.lat}, Longitude = {observer.lon}, Elevation = {observer.elevation}m")
	return observer

def tle_of_sat(TLE_path: str, directory: str, sat):

    # Ensure the directory exists
    os.makedirs(directory, exist_ok=True)

    #Name of the file where the TLE will be saved
    output_file = os.path.join(directory, "TLE.tle")
    
    with open(TLE_path, 'r') as inTLE, open(output_file, 'w') as outTLE:
        for svid in inTLE:
            svid = svid.strip()
            line1 = inTLE.readline()
            line2 = inTLE.readline()
        
            if svid in sat:
                outTLE.write(svid + '\n')
                outTLE.write(line1)
                outTLE.write(line2)
        
        