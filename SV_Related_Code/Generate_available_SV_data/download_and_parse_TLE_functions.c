#include "utils.h"
#include "download_and_parse_TLE_functions.h"

// Callback function for cURL to write received data into memory
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        // out of memory!
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

// Function to download TLE data from a URL
char* download_tles(const char* url) {
    CURL *curl_handle;
    CURLcode res;
    MemoryStruct chunk;

    chunk.data = malloc(1);  // will be grown as needed by the realloc above
    chunk.size = 0;    // no data at this point

    curl_global_init(CURL_GLOBAL_ALL);

    curl_handle = curl_easy_init();
    if(curl_handle) {
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl_handle);

        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(chunk.data);
            chunk.data = NULL;
        }

        curl_easy_cleanup(curl_handle);
    }

    curl_global_cleanup();

    return chunk.data;
}

// Function to parse the TLE data and populate an array of TLE structs
int parse_tle_string(const char* data, TLE** tles) {
    if (!data) {
        return 0;
    }

    int count = 0;
    char* data_copy = strdup(data); // Create a copy as strtok_r modifies the string
    if (!data_copy) return 0;

    char* context;
    char* line = strtok_r(data_copy, "\n", &context);

    // Skip the first line, which is the header
    if (line) {
        line = strtok_r(NULL, "\n", &context);
    }

    // Now, parse the TLEs, which are always three lines
    while (line) {
        char* name = line;
        char* line1 = strtok_r(NULL, "\n", &context);
        char* line2 = strtok_r(NULL, "\n", &context);
        
        // Check for a complete 3-line entry
        if (!name || !line1 || !line2) {
            fprintf(stderr, "Incomplete TLE entry detected. Stopping parsing.\n");
            break;
        }

        count++;
        *tles = realloc(*tles, count * sizeof(TLE));
        if (!*tles) {
            fprintf(stderr, "Memory allocation failed for TLE array.\n");
            free(data_copy);
            return 0;
        }

        TLE* current_tle = &((*tles)[count - 1]);
        
        // Copy the data into the struct
        strcpy(current_tle->line1, line1);
        strcpy(current_tle->line2, line2);

        // Move to the next entry
        line = strtok_r(NULL, "\n", &context);
    }

    free(data_copy);
    return count;
}