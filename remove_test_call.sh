#!/bin/bash
# Script to remove the test_places_and_wrap() call from TICC.ino after testing

echo "Removing test_places_and_wrap() call from TICC.ino..."

# Remove the test function call and its comments
sed -i '/Test PLACES and WRAP options (remove this call after testing)/d' TICC/TICC.ino
sed -i '/TODO: Remove this line after testing PLACES and WRAP functionality/d' TICC/TICC.ino
sed -i '/test_places_and_wrap();/d' TICC/TICC.ino

echo "Test function call removed. The test function remains in print.cpp for future reference."
