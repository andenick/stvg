#!/bin/bash
# Clean legacy market scenario JSONs from Inputs/
# Run from project root: bash Technical/StatisticalEngine/scripts/clean_inputs.sh
set -e

cd "$(dirname "$0")/../../.."  # navigate to STVG root

echo "Moving 1,906 legacy market_scenario_*.json files from Inputs/ to Archive/..."
mkdir -p Archive/Legacy_Market_Scenarios
mv Inputs/market_scenario_*.json Archive/Legacy_Market_Scenarios/
mv Inputs/simulation_results.json Archive/Legacy_Market_Scenarios/
echo "Done. Inputs/ now has $(ls Inputs/ | wc -l) entries."
echo "Archive/Legacy_Market_Scenarios/ has $(ls Archive/Legacy_Market_Scenarios/ | wc -l) files."
