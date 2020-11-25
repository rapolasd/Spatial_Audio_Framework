#!/bin/bash
set -e

#A script to do some extra configurations that are not yet supported natively by doxygen

echo "Running doxygen post-script"

# Make the default detail level=4 for the "files" tab
sed -i.bak 's/(document).ready(function() { init_search(); });/(document).ready(function() { init_search(); toggleLevel(4); });/' html/files.html && rm html/files.html.bak

#sed -i '' 's+(document).ready(function() { init_search(); });+(document).ready(function() { init_search(); toggleLevel(4); });+' html/files.html

echo "Done!"

set +e