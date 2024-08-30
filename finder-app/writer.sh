
writefile=$1
writestr=$2

if [ "$#" -ne 2 ]; then
	echo "Error: two arguments required for this command: <directory_path> <write_string>"
	exit 1
fi

if ! mkdir -p "$(dirname "$writefile")"; then
	echo "Error: could not create the directory for $writefile"
	exit 1
fi

echo "$writestr" > "$writefile" || { echo "Error: could not write to file"; exit 1; }
echo "Success! Wrote to $writefile"
