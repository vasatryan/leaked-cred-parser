#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <set>
#include <thread>
#include <csignal>
#include <iomanip>
#include <algorithm>
#include <regex>

namespace fs = std::filesystem;
using json = nlohmann::json;

void reorder_string(std::string& line, size_t pos) {
    std::string first_part = line.substr(0, pos);
    std::string second_part = line.substr(pos + 1);
    second_part[second_part.length() - 1] = ':';
    line = second_part + first_part;
}

bool changePosition(std::string &line, size_t pos) {
    size_t pos_slash = line.find('/', pos);
    size_t pos_dot = line.find('.', pos);
    if (pos_slash == std::string::npos || pos_dot == std::string::npos) {
        return false;
    }

    reorder_string(line, pos);
    return true;
}

void normalize_format(std::string &input) {
    std::string tmp = input;
    for (char &c : input) {
        if (c == '|' || c == ' ') {
            c = ':';
        }
    }

    auto pos = input.rfind(':');
    if (changePosition(input, pos)) {
        reorder_string(tmp, pos);
    }

    size_t sec_pos = std::string::npos;
    sec_pos = input.find(':');
    sec_pos = input.find(':',sec_pos+1);

    for (size_t i = sec_pos+1; i < input.length(); ++i) {
        input[i] = tmp[i];
    }
}

void removeProtocol(std::string &url) {
    size_t pos = url.find("http://");
    if (pos != std::string::npos) {
        url.erase(pos, 7);
    } else {
        pos = url.find("https://");
        if (pos != std::string::npos) {
            url.erase(pos, 8);
        }
    }
}

void trim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

bool tokenize(const std::string &line, std::string &url, std::string &username, std::string &password) {
    size_t first_colon = line.find(':');
    if (first_colon == std::string::npos) {
        return false;
    }

    size_t second_colon = line.find(':', first_colon + 1);
    if (second_colon == std::string::npos) {
        return false;
    }

    url = line.substr(0, first_colon);
    username = line.substr(first_colon + 1, second_colon - first_colon - 1);
    password = line.substr(second_colon + 1, line.length() - 1);

    return true;
}

std::string getFileModificationTime(const fs::path& filePath) {
    try {
        auto ftime = fs::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());

        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        std::tm* tmtime = std::localtime(&cftime);

        std::ostringstream oss;
        oss << std::put_time(tmtime, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    } catch (const std::exception& e) {
        std::cerr << "Error getting file time: " << e.what() << std::endl;
        return "unknown";
    }
}

bool shouldProcessDirectory(const fs::path& directory) {
    int fileCount = 0;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (fs::is_regular_file(entry)) {
            fileCount++;
            if (fileCount > 1) {
                return false;
            }
        }
    }
    return fileCount == 1;
}

void processFile(const fs::path &filePath,
                std::unordered_map<std::string, std::pair<std::string, std::string>> &uniqueLinesWithMetadata) {
    std::ifstream inputFile(filePath);
    if (!inputFile) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return;
    }

    std::string source = filePath.parent_path().filename().string();
    std::string timestamp = getFileModificationTime(filePath);

    std::string line;
    while (std::getline(inputFile, line)) {
        removeProtocol(line);
        normalize_format(line);
        trim(line);

        if (!line.empty()) {
            if (uniqueLinesWithMetadata.find(line) == uniqueLinesWithMetadata.end()) {
                uniqueLinesWithMetadata[line] = {source, timestamp};
            }
        }
    }

    inputFile.close();
}

void processDirectory(const fs::path &directory,
                     std::unordered_map<std::string, std::pair<std::string, std::string>> &uniqueLinesWithMetadata) {
    for (const auto &entry : fs::directory_iterator(directory)) {
        if (fs::is_directory(entry)) {
            if (shouldProcessDirectory(entry.path())) {
                processDirectory(entry.path(), uniqueLinesWithMetadata);
            }
        }
        else if (fs::is_regular_file(entry) && entry.path().extension() == ".txt") {
            processFile(entry.path(), uniqueLinesWithMetadata);
        }
    }
}

// Function to find the category of a URL
std::string findCategory(const std::string &url,const std::string &username, const json &categories) {
    for (const auto &category : categories.items()) {
        const std::string &categoryName = category.key();
        const auto &domains = category.value()["domains"]; // Access the "domains" array
        for (const auto &domain : domains) {
            if (url.find(domain.get<std::string>()) != std::string::npos) {
                return categoryName;
            }
        }
    }

    std::regex domainRegex("(([a-zA-Z0-9.-]+\\.[a-zA-Z]{2,})(\\/.*)?)");
    if (!std::regex_search(url, domainRegex) || username.find("http") != std::string::npos) {
        return "uncategorized";
    }

    return "other"; // Default category if no match is found
}

int loadExistingData(const std::string& outputFilePath, 
                   std::unordered_map<std::string, std::pair<std::string, std::string>>& existingData,
                   std::set<std::string>& writtenLines) {
    std::ifstream inputFile(outputFilePath);
    if (!inputFile) {
        return 0; // File doesn't exist yet or can't be opened
    }

    int maxId = 0;
    std::string line;
    while (std::getline(inputFile, line)) {
        try {
            json entry = json::parse(line);
            
            // Recreate the original line format to use as key
            std::string url = entry["url"];
            std::string username = entry["username"];
            std::string password = entry["password"];
            std::string reconstructedLine = url + ":" + username + ":" + password;
            
            // Store in our set of already written lines
            writtenLines.insert(reconstructedLine);
            
            // Track the highest ID
            if (entry["id"] > maxId) {
                maxId = entry["id"];
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing JSON line: " << e.what() << std::endl;
        }
    }
    
    return maxId;
}

// Modify the main function to use append mode
int main() {
    fs::path inputDir = "../directory-of-your-extracted-data/";
    std::string outputFilePath = "./all-parsed-data.json";
    std::string categoryFilePath = "./category.json";

    // Load categories from category.json
    json categories;
    try {
        std::ifstream categoryFile(categoryFilePath);
        if (!categoryFile) {
            std::cerr << "Error opening category file: " << categoryFilePath << std::endl;
            return 1;
        }
        categoryFile >> categories;
    } catch (const std::exception &e) {
        std::cerr << "Error loading category file: " << e.what() << std::endl;
        return 1;
    }

    std::signal(SIGINT, [](int sig) {
        std::cout << "\nReceived SIGINT signal. Shutting down..." << std::endl;
        exit(0);
    });

    std::set<fs::path> processedFiles;
    std::unordered_map<std::string, std::pair<std::string, std::string>> uniqueLinesWithMetadata;
    
    // Keep track of which lines have already been written to the JSON file
    std::set<std::string> writtenLines;
    
    // Load existing data and get the next ID to use
    int nextId = loadExistingData(outputFilePath, uniqueLinesWithMetadata, writtenLines) + 1;

    std::cout << "Monitoring directory: " << inputDir << std::endl;
    std::cout << "Press Ctrl+C to stop monitoring" << std::endl;

    while (true) {
        bool newFilesFound = false;

        for (const auto &entry : fs::directory_iterator(inputDir)) {
            if (fs::is_directory(entry)) {
                // Use shouldProcessDirectory to check if the directory should be processed
                if (shouldProcessDirectory(entry.path())) {
                    for (const auto &dirEntry : fs::directory_iterator(entry.path())) {
                        if (fs::is_regular_file(dirEntry) && dirEntry.path().extension() == ".txt") {
                            if (processedFiles.find(dirEntry.path()) == processedFiles.end()) {
                                std::cout << "Processing new file: " << dirEntry.path().filename() << std::endl;
                                processFile(dirEntry.path(), uniqueLinesWithMetadata);
                                processedFiles.insert(dirEntry.path());
                                newFilesFound = true;
                            }
                        }
                    }
                }
            } else if (fs::is_regular_file(entry) && entry.path().extension() == ".txt") {
                if (processedFiles.find(entry.path()) == processedFiles.end()) {
                    std::cout << "Processing new file: " << entry.path().filename() << std::endl;
                    processFile(entry.path(), uniqueLinesWithMetadata);
                    processedFiles.insert(entry.path());
                    newFilesFound = true;
                }
            }
        }

        if (newFilesFound) {
            // Open file in append mode instead of truncate mode
            std::ofstream outputFile(outputFilePath, std::ios::app);
            if (!outputFile) {
                std::cerr << "Error opening output file for appending: " << outputFilePath << std::endl;
                continue;
            }

            int newEntriesCount = 0;
            
            for (const auto &entry : uniqueLinesWithMetadata) {
                const std::string &line = entry.first;
                
                // Skip lines we've already written to the file
                if (writtenLines.find(line) != writtenLines.end()) {
                    continue;
                }
                
                const std::string &source = entry.second.first;
                const std::string &timestamp = entry.second.second;

                std::string url, username, password;
                if (tokenize(line, url, username, password)) {
                    // Find the category of the URL
                    std::string category = findCategory(url, username,categories);

                    // Add the entry to the JSON file
                    json jsonEntry = {
                        {"id", nextId++},
                        {"url", url},
                        {"username", username},
                        {"password", password},
                        {"source", source},
                        {"timestamp", timestamp},
                        {"category", category}
                    };
                    outputFile << jsonEntry.dump() << std::endl;
                    
                    // Mark as written
                    writtenLines.insert(line);
                    newEntriesCount++;
                }
            }

            outputFile.close();
            std::cout << "Output updated! " << newEntriesCount << " new entries appended to: " << outputFilePath << std::endl;
            std::cout << "Total entries in file: " << writtenLines.size() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}