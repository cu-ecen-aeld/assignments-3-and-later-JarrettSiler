
#the two arguments
filesdir=$1
searchstr=$2

#are there two arguments?
if  [ "$#" -ne 2 ]; then
	echo "Error: two arguments are required for find: <directory_path> <search_string>"
	exit 1
fi

#filesdir does not represent a directory on the filesystem
if [ ! -d "$filesdir" ]; then
	echo "Error: $filesdir does not exist."
	exit 1
fi

# Count the number of files in the directory and subdirectories and the number of lines in these files that contain the searchstr
num_files=$(find "$filesdir" -type f | wc -l)
num_lines=$(grep -r "$searchstr"  "$filesdir" | wc -l)

echo "The number of files are $num_files and the number of matching lines are $num_lines"
