# Credential Processing and Categorization Tool

A tool for monitoring, processing, and categorizing credential data from text files.

## Overview

This application continuously monitors a directory for text files containing credential data (URL, username, password), processes the data to standardize formats, removes duplicates, categorizes the credentials based on the URL domain, and saves the results to a JSON output file.

## Features

- **Continuous monitoring**: Checks for new files every 5 seconds
- **Automatic processing**: Handles new files as they appear
- **Format normalization**: Standardizes credential formats for consistency
- **Deduplication**: Ensures no duplicate credentials are stored
- **Categorization**: Classifies credentials based on URL domain
- **Metadata tracking**: Records source and timestamp for each credential
- **Append-only output**: Adds new data without reprocessing existing entries

## Requirements

- C++17 or later
- nlohmann/json library
  - ```
    sudo apt install nlohmann/json3-dev
- Filesystem support

## Installation

1. Clone the repository:
   ```
   git clone https://github.com/vasatryan/leaked-cred-parser.git
   cd leaked-cred-parser
   ```

2. Build the project:
   ```
   g++ ./parser.cpp -o credential-processor
   ```

## Usage

1. Prepare your category.json file with domain categories:
   ```json
   {
     "social": {
       "domains": ["facebook.com", "twitter.com", "instagram.com"]
     },
     "email": {
       "domains": ["gmail.com", "outlook.com", "yahoo.com"]
     },
     // Add more categories as needed
   }
   ```

2. Run the application:
   ```
   ./credential-processor
   ```

3. The application will:
   - Monitor the directory specified in the code (`../directory-of-your-extracted-data/` by default)
   - Process new text files as they appear
   - Output results to `../all-parsed-data.json`

4. To stop the application, press Ctrl+C.

## Input Format

The input text files should contain credential data in various formats. The application normalizes these formats to:
```
domain.com:username:password
```

## Output Format

The output JSON file contains one JSON object per line, with the following structure:
```json
{
  "id": 1,
  "url": "example.com",
  "username": "user123",
  "password": "pass456",
  "source": "sourcefolder",
  "timestamp": "2023-01-01 12:00:00",
  "category": "other"
}
```

## Configuration

Edit the following values in the code to customize:
- Input directory path: `fs::path inputDir = "../directory-of-your-extracted-data/";`
- Output file path: `std::string outputFilePath = "../all-parsed-data.json";`
- Category file path: `std::string categoryFilePath = "./category.json";`

## Documentation
For detailed documentation on how each function works, please refer to the [Code Documentation](https://docs.google.com/document/d/16TG8v_r4vPXyysPP5hklWF24SYUV5RiY_LrSpzx8Y9c/edit?usp=sharing) file.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
