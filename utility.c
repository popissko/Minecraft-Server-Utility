#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>      // For _getch()
#include <curl/curl.h>
#include "cJSON.h" 
#include <direct.h>     // For _mkdir() and _chdir()
#include <windows.h>    // For GetModuleFileNameA() and Sleep()

// Function prototypes
void draw_line(int a);
void edit_server_properties(void);
void start_server(void);

// Global variable to store the absolute path to the certificate file
char g_cacert_path[MAX_PATH];

// Structure to hold downloaded data in memory
struct MemoryStruct {
    char *memory;
    size_t size;
};

// libcurl callback to write downloaded data into the MemoryStruct
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        printf("Error: Not enough memory (realloc failed).\n");
        return 0;
    }
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

// Performs an HTTP GET request and returns the response in a string
char* perform_http_get(const char* url) {
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    if(curl_handle) {
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "MinecraftServerDownloader/1.0");
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_CAINFO, g_cacert_path);

        res = curl_easy_perform(curl_handle);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(chunk.memory);
            curl_easy_cleanup(curl_handle);
            curl_global_cleanup();
            return NULL;
        }
        curl_easy_cleanup(curl_handle);
    }
    curl_global_cleanup();
    return chunk.memory;
}

// libcurl callback to write downloaded data directly to a file
static size_t WriteToFileCallback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

// Downloads a file from a URL and saves it to a specified path
int download_file(const char* url, const char* outfilename) {
    CURL *curl_handle;
    FILE *pagefile;
    CURLcode res = CURLE_OK;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    if (curl_handle) {
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteToFileCallback);
        
        pagefile = fopen(outfilename, "wb");
        if (pagefile) {
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
            curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "MinecraftServerDownloader/1.0");
            curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl_handle, CURLOPT_CAINFO, g_cacert_path);

            res = curl_easy_perform(curl_handle);
            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }
            fclose(pagefile);
        } else {
            fprintf(stderr, "Error: Could not open or create file '%s'.\n", outfilename);
            res = CURLE_WRITE_ERROR;
        }
        curl_easy_cleanup(curl_handle);
    }
    curl_global_cleanup();
    return (res == CURLE_OK) ? 1 : 0;
}

// Main function for setting up the Minecraft server
void setup_server(){
    system("cls");
    printf("\n     Minecraft Server Setup\n    ");
    
    draw_line(65);

    const char* dir_path = "C:\\MinecraftServer";
    if (_mkdir(dir_path) == 0) {
        printf("     > Directory '%s' created.\n", dir_path);
    } else {
        printf("     > Directory '%s' already exists.\n", dir_path);
        printf("\n     > PLEASE BACK UP EXISTING FOLDER AND CHANGE THE NAME!\n\n");
    }

    if (_chdir(dir_path) != 0) {
        printf("     Error: Failed to change directory to '%s'.\n", dir_path);
        printf("\n     Press any key to return to the main menu...");
        _getch();
        return;
    }
    printf("     > Working directory set to: '%s'\n", dir_path);

    char version[20];
    printf("     > Please enter the Minecraft version to install (e.g., 1.20.4):\n\n\n");
    printf("     >");
    scanf("%19s", version);
    while (getchar() != '\n'); // Clear input buffer

    printf("     > Fetching latest build information for version '%s'...\n", version);
    char api_url[256];
    snprintf(api_url, sizeof(api_url), "https://api.papermc.io/v2/projects/paper/versions/%s", version);
    
    char* json_response = perform_http_get(api_url);
    if (!json_response) {
        printf("     Error: Could not retrieve version info. Make sure the version is correct.\n");
        printf("\n     Press any key to return to the main menu...");
        _getch();
        return;
    }

    cJSON *json = cJSON_Parse(json_response);
    int latest_build = -1;
    if (json) {
        cJSON *builds_array = cJSON_GetObjectItemCaseSensitive(json, "builds");
        if (cJSON_IsArray(builds_array)) {
            int array_size = cJSON_GetArraySize(builds_array);
            if (array_size > 0) {
                latest_build = cJSON_GetArrayItem(builds_array, array_size - 1)->valueint;
            }
        }
    }
    cJSON_Delete(json);
    free(json_response);

    if (latest_build == -1) {
        printf("     Error: No valid build found for version '%s'.\n", version);
        printf("\n     Press any key to return to the main menu...");
        _getch();
        return;
    }
    printf("     > Latest build found: %d\n", latest_build);

    char download_url[512];
    char jar_filename[128];
    snprintf(jar_filename, sizeof(jar_filename), "paper-%s-%d.jar", version, latest_build);
    snprintf(download_url, sizeof(download_url), "https://api.papermc.io/v2/projects/paper/versions/%s/builds/%d/downloads/%s", version, latest_build, jar_filename);
    
    printf("     > Downloading server file... (This may take a while depending on your internet speed)\n");
    printf("     > URL: %s\n", download_url);

    if (download_file(download_url, "server.jar")) {
        printf("     > 'server.jar' has been downloaded successfully.\n");
    } else {
        printf("     Error: Failed to download the server file.\n");
        printf("\n     Press any key to return to the main menu...");
        _getch();
        return;
    }

    printf("     > Accepting EULA...\n");
    FILE* eula_file = fopen("eula.txt", "w");
    if (eula_file) {
        fprintf(eula_file, "eula=true\n");
        fclose(eula_file);
    }
    
    // Get RAM and startup flag preferences from user
    int ram_gb = 0;
    while(ram_gb <= 0){
        printf("\n     > How many GB of RAM should the server use? (e.g., 2, 4): ");
        scanf("%d", &ram_gb);
        while (getchar() != '\n'); // Clear input buffer
        if (ram_gb <= 0) {
            printf("     > Invalid input. Please enter a positive number.\n");
        }
    }
    
    char use_aikar_flags;
    printf("     > Use Aikar's optimized flags for better performance? (Y/N): ");
    use_aikar_flags = _getch();
    printf("%c\n", use_aikar_flags);

    printf("     > Creating startup script (start.bat)...\n");
    FILE* start_script = fopen("start.bat", "w");
    if (start_script) {
        fprintf(start_script, "@echo off\n");
        
        // Generate the startup command based on user's choice
        if(use_aikar_flags == 'y' || use_aikar_flags == 'Y') {
            const char* aikars_flags = "-XX:+UseG1GC -XX:+ParallelRefProcEnabled -XX:MaxGCPauseMillis=200 -XX:+UnlockExperimentalVMOptions -XX:+DisableExplicitGC -XX:+AlwaysPreTouch -XX:G1NewSizePercent=30 -XX:G1MaxNewSizePercent=40 -XX:G1HeapRegionSize=8M -XX:G1ReservePercent=20 -XX:G1HeapWastePercent=5 -XX:G1MixedGCCountTarget=4 -XX:InitiatingHeapOccupancyPercent=15 -XX:G1MixedGCLiveThresholdPercent=90 -XX:G1RSetUpdatingPauseTimePercent=5 -XX:SurvivorRatio=32 -XX:+PerfDisableSharedMem -XX:MaxTenuringThreshold=1 -Dusing.aikars.flags=https://mcflags.emc.gs -Daikars.new.flags=true";
            fprintf(start_script, "java -Xms%dG -Xmx%dG %s -jar server.jar nogui\n", ram_gb, ram_gb, aikars_flags);
            printf("     > Using Aikar's optimized flags with %dGB RAM.\n", ram_gb);
        } else {
            fprintf(start_script, "java -Xms%dG -Xmx%dG -jar server.jar nogui\n", ram_gb, ram_gb);
            printf("     > Using standard flags with %dGB RAM.\n", ram_gb);
        }

        fprintf(start_script, "pause\n");
        fclose(start_script);
    }
    
    draw_line(40);
    printf("\n     Setup Complete!\n");
    printf("     To start your server, use option [4] from the main menu\n");
    printf("     or double-click the 'start.bat' file in 'C:\\MinecraftServer'.\n");
    printf("\n     Press any key to return to the main menu...");
    _getch();
}

// Opens the server.properties file in the default text editor
void edit_server_properties() {
    system("cls");
    printf("\n     Edit Server Properties\n");
    draw_line(40);

    const char* properties_path = "C:\\MinecraftServer\\server.properties";

    FILE *file = fopen(properties_path, "r");
    if (file == NULL) {
        printf("\n     'server.properties' not found.\n");
        printf("     Please run the 'Setup a New Minecraft Server' option [2] and then\n");
        printf("     start the server once with option [4] to generate the file.\n");
    } else {
        fclose(file);
        
        printf("\n     Attempting to open '%s' in your default text editor...\n\n", properties_path);
        
        char command[300];
        snprintf(command, sizeof(command), "start %s", properties_path);
        system(command);

        printf("     After you save your changes in the text editor, you can close it.\n");
        printf("     The changes will take effect the next time you start the server.\n");
    }

    printf("\n     Press any key to return to the main menu...");
    _getch();
}

// Starts the server by executing the start.bat script
void start_server() {
    system("cls");
    printf("\n     Start Minecraft Server\n");
    draw_line(40);

    const char* bat_path = "C:\\MinecraftServer\\start.bat";

    // Check if start.bat exists before trying to run it
    FILE* file = fopen(bat_path, "r");
    if (file == NULL) {
        printf("\n     'start.bat' not found.\n");
        printf("     Please run the 'Setup a New Minecraft Server' option [2] first.\n");
    } else {
        fclose(file);
        printf("\n     Attempting to start the server in a new window...\n");
        
        // The "start" command runs the script in a new console window,
        // which prevents our main program from locking up.
        system("start \"Minecraft Server\" \"C:\\MinecraftServer\\start.bat\"");
        
        printf("     The server is starting. You can close this utility or\n");
        printf("     leave it open to perform other tasks.\n");
    }

    printf("\n     Press any key to return to the main menu...");
    _getch();
}

// Checks if Java is installed and offers to install it if not found
void check_java_version(){
    system("cls");
    printf("\n");
    printf("    Checking for Java installation...\n\n");

    FILE *pipe = _popen("java -version 2>&1", "r" );
    if (!pipe) {
        printf("    Error: Could not execute command.\n ");
        printf("\n  Press any key to return to the main menu...");
        _getch();
        return;
    }

    char buffer[256];
    int java_found = 0;
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        printf("    > %s", buffer);
        if (strstr(buffer, "java version") || strstr(buffer, "openjdk")) {
            java_found = 1;
        }
    }
    _pclose(pipe);

    if (java_found) {
        printf("\n\n     **Java is already installed**\n");
    } else {
        printf("\n     Java is not found.\n");
        printf("     Would you like to install it? (Y/N): ");
        char choice = _getch();
        printf("%c\n\n", choice);
        if (choice == 'y' || choice == 'Y') {
            printf("     Starting installation...\n");
            printf("     Please accept any User Account Control (UAC) prompts if they appear.\n\n");
            // Use winget to silently install the latest Java JRE
            const char* winget_command = "winget install --id EclipseAdoptium.Temurin.21.JRE --silent --accept-source-agreements --accept-package-agreements";
            system(winget_command);
            printf("\n     Installation attempt finished.\n");
            printf("\n     Please restart this program to verify the new Java installation.\n");
        }
    }
    printf("\n     Press any key to return to the main menu...");
    _getch();
}

// Draws a horizontal line of a specified length
void draw_line(int a) {
    for (int i = 0; i < a; i++) {
        printf("─");
    }
    printf("\n");
}

// Sets the console font and size
void set_console_font(const wchar_t* font_name, short font_size_y) {

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX fontInfo;
    fontInfo.cbSize = sizeof(CONSOLE_FONT_INFOEX);

    if (!GetCurrentConsoleFontEx(hConsole, FALSE, &fontInfo)) {
        return;
    }

    fontInfo.dwFontSize.Y = font_size_y; 
    fontInfo.dwFontSize.X = 0; // Automatically calculate width
    wcscpy(fontInfo.FaceName, font_name); 

    SetCurrentConsoleFontEx(hConsole, FALSE, &fontInfo);
}

// Displays the main menu options to the user
void show_main_menu() {
    system("cls");
    printf("\n\n\n");
    printf("                    ");
    draw_line(40);
    printf("\n");
    printf("                        [1] Check/Install Java\n\n");
    printf("                        [2] Setup a New Minecraft Server\n\n");
    printf("                        [3] Edit Server Properties\n\n");
    printf("                        [4] Start the Server\n\n");
    printf("                        ");
    draw_line(31);
    printf("\n");
    printf("                        [H] Help\n\n");
    printf("                        [0] Exit\n\n");
    printf("                    ");
    draw_line(40);
    printf("\n");
    printf("                  Select an option with your keyboard [1,2,3,4,H,0]\n");
}

int main() {
    // Find the application's directory to locate the certificate file
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    char* last_slash = strrchr(exe_path, '\\');
    if (last_slash) {
        *(last_slash + 1) = '\0';
    }
    snprintf(g_cacert_path, sizeof(g_cacert_path), "%scacert.pem", exe_path);

    // Initialize console settings
    set_console_font(L"Consolas", 20);
    system("chcp 65001 > nul"); // Set console code page to UTF-8
    system("title Minecraft Server Utility v1.1");
    system("mode con: cols=80 lines=30");

    char choice;
    // Main application loop
    while (1) {
        show_main_menu();
        choice = _getch(); // Wait for user input
        switch (choice) {
            case '1': check_java_version(); break;
            case '2': setup_server(); break;
            case '3': edit_server_properties(); break;
            case '4': start_server(); break;
            case 'h':
            case 'H':
                printf("https://github.com/popissko/Minecraft-Server-Utility\n");
                break;
            case '0': return 0; // Exit the program
            default: break; // Do nothing for invalid input
        }
    }
    return 0;
}

