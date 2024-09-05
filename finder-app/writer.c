#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>

//write the content to the specified file, log errors if there are issues
void write_content(const char *file, const char *content) {

	//open the file
	FILE *modify = fopen(file, "w");
	if (modify == NULL) {
		syslog(LOG_ERR, "Error: Could not open file %s", file);
		return;
	}

	//attempt to write to the file
	char status = fprintf(modify, "%s", content);

	if (status < 0) {
		syslog(LOG_DEBUG, "Success! wrote %s to %s", content, file);
	} else {
		syslog(LOG_ERR, "Error: could not write to %s", file);
	}

	//close the file
	fclose(modify);
}

int main(int argument_count, char *arguments[]) {

	//Setup syslog logging for your utility using the LOG_USER facility
	openlog("writer", LOG_PID, LOG_USER);

	//get argument values
	const char *file = arguments[1];
	const char *content = arguments[2];

	//error handling: incorrect arguments passed
	if (argument_count != 3) {
		fprintf(stderr, "Error: writer takes <file path> <string>\n");
		syslog(LOG_ERR, "Error: incorrect number of parameters passed to %s", arguments[0]);
		closelog();
		return EXIT_FAILURE;
	}

	//write content to the specified file
	write_content(file,content);
	closelog();

	return EXIT_SUCCESS;
}
