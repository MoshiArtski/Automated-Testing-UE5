# Automated-Testing-UE5
Automation Tools for UE

# Unreal Engine Asset Usage Test

This Unreal Engine script is designed for an automated test (`FAssetUsageTest`) to check the usage of assets within an Unreal Engine project. It generates a report detailing each asset's usage, along with additional metadata.

## Features

- Scans all assets in the project.
- Determines if each asset is used in the project.
- Outputs a CSV file with asset details, including name, path, type, usage status, referencers, and source metadata (based on metadata tag 'Source').
- Logs unused assets and reports them in the test output.

## Requirements

- Unreal Engine 4.x/5.x (the script is compatible with both versions).
- The script assumes you have access to the Unreal Engine Editor and its subsystems.

## Installation

1. Place the `FAssetUsageTest.cpp` file in your project's `Source` directory, preferably under a specific folder for tests.
2. Ensure your project's build configurations include the automation test module.

## Running the Test

To run the `FAssetUsageTest`:

1. Open your project in Unreal Engine Editor.
2. Go to `Window > Test Automation`.
3. Find `Project.AssetUsageTest` under the relevant category.
4. Run the test.

## Output

The test generates a CSV file in the project's `Log` directory, named `AssetUsageReport.csv`. This file contains the following columns:

- Asset Name
- Asset Path
- Asset Type
- Is Used (Yes/No)
- Referenced By
- Source Tag

## Contributing

Feel free to contribute to this script by submitting pull requests or reporting issues on this repository.

# Unreal Engine Level Frame Rate Test

This Unreal Engine script, `FLoadLevelAndCheckFrameRateTest`, automates the process of loading levels/maps in the engine and measuring their frame rates. It's designed to help ensure performance standards across different levels in a project.

## Features

- Automatically loads each level in the project.
- Measures and logs frame rates, draw call statistics, and thread times.
- Determines if levels meet a specified performance criterion (e.g., average frame rate).
- Outputs detailed performance metrics for further analysis.

## Requirements

- Unreal Engine 4.x/5.x.
- Access to the Unreal Engine Editor and relevant automation testing frameworks.

## Installation

1. Place the `FLoadLevelAndCheckFrameRateTest.cpp` file in the `Source` directory of your project.
2. Include the script in your build configurations to ensure it's compiled with the project.

## Running the Test

To run the `FLoadLevelAndCheckFrameRateTest`:

1. Open your project in the Unreal Engine Editor.
2. Navigate to `Window > Test Automation`.
3. Look for `Project.LevelFramerateTest` in the test list.
4. Execute the test to begin the level loading and frame rate measurement process.

## Output

- The script logs the frame rate and other performance metrics for each level to the console and/or a log file.
- It checks if the average frame rate meets a predefined threshold (e.g., 60 FPS).
- If a level does not meet the performance criterion, the script logs an error.

## Contributing

Contributions to this script are welcome. Please submit pull requests or report issues on this repository for any enhancements or bug fixes.

## License

N/A


## License

N/A

