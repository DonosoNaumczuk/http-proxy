#include <handleParsers.h>
#include <connectToOrigin.h>

static int parse(struct parseRequest *parseRequest, buffer *input, int *flag,
				 int (*parseChar)(struct parseRequest *, char));
static int handleMethod(struct parseRequest *parseRequest, buffer *readBuffer,
						unsigned *ret);
static int handleTarget(struct selector_key *key,
						struct parseRequest *parseRequest, buffer *readBuffer,
						unsigned *ret);
static int handleVersion(struct selector_key *key,
						 struct parseRequest *parseRequest, buffer *readBuffer,
						 unsigned *ret);
static int handleHeader(struct selector_key *key,
						struct parseRequest *parseRequest, buffer *readBuffer,
						unsigned *ret);
static int parseMethodCharWrapper(struct parseRequest *parseRequest,
								  char letter);
static int parseTargetCharWrapper(struct parseRequest *parseRequest,
								  char letter);
static int parseVersionCharWrapper(struct parseRequest *parseRequest,
								   char letter);
static int parseHeaderCharWrapper(struct parseRequest *parseRequest,
								  char letter);
static unsigned parseProcess(struct selector_key *key, buffer *readBuffer,
							 size_t bytesRead);

void parseInit(const unsigned state, struct selector_key *key) {
	struct parseRequest *parseRequest = getParseRequestState(GET_DATA(key));

	parseRequest->input = getReadBuffer(GET_DATA(key));

	parseMethodInit(&(parseRequest->methodParser));
	parseTargetInit(&(parseRequest->targetParser));
	parseVersionInit(&(parseRequest->versionParser));
	parseHeaderInit(&(parseRequest->headerParser));
}

void parseDestroy(const unsigned state, struct selector_key *key) {
	struct parseRequest *parseRequest = getParseRequestState(GET_DATA(key));
	setRequestMethod(GET_DATA(key), getMethod(&(parseRequest->methodParser)));
	// static :maybe more things
}

unsigned parseRead(struct selector_key *key) {
	buffer *readBuffer = getReadBuffer(GET_DATA(key));
	unsigned ret	   = PARSE;
	uint8_t *pointer;
	size_t count;
	ssize_t bytesRead;

	if (buffer_can_write(readBuffer)) {
		pointer   = buffer_write_ptr(readBuffer, &count);
		bytesRead = recv(key->fd, pointer, count, 0);

		if (bytesRead >= 0) {
			buffer_write_adv(readBuffer, bytesRead);
			ret = parseProcess(key, readBuffer, bytesRead);
		}
		else {
			ret = ERROR;
		}
	}

	return ret;
}

unsigned parseProcess(struct selector_key *key, buffer *readBuffer,
					  size_t bytesRead) {
	struct parseRequest *parseRequest = getParseRequestState(GET_DATA(key));
	unsigned ret					  = PARSE;
	int bytesProcessed				  = 0;
	int bytesFromAParser			  = 0;
	while (bytesRead != bytesProcessed && ret == PARSE) {
		switch (parseRequest->state) {
			case PARSE_METHOD:
				bytesFromAParser = handleMethod(parseRequest, readBuffer, &ret);
				break;
			case PARSE_TARGET:
				bytesFromAParser =
					handleTarget(key, parseRequest, readBuffer, &ret);
				break;
			case PARSE_VERSION:
				bytesFromAParser =
					handleVersion(key, parseRequest, readBuffer, &ret);
				break;
			case PARSE_HEADER:
				bytesFromAParser =
					handleHeader(key, parseRequest, readBuffer, &ret);
				break;
		}
		bytesProcessed = bytesProcessed + bytesFromAParser;
	}
	return ret;
}

int handleMethod(struct parseRequest *parseRequest, buffer *readBuffer,
				 unsigned *ret) {
	int hasToContinue	= 0;
	int bytesFromAParser = parse(parseRequest, readBuffer, &hasToContinue,
								 &parseMethodCharWrapper);
	if (!hasToContinue) {
		parseRequest->state = PARSE_TARGET;
	}
	return bytesFromAParser;
}

int handleTarget(struct selector_key *key, struct parseRequest *parseRequest,
				 buffer *readBuffer, unsigned *ret) {
	int hasToContinue	= 0;
	int bytesFromAParser = parse(parseRequest, readBuffer, &hasToContinue,
								 &parseTargetCharWrapper);
	if (!hasToContinue) {
		parseRequest->state = PARSE_VERSION;
		char *host			= getHost(&(parseRequest->targetParser));
		if (host != NULL && host[0] != 0) {
			printf("TARGET Host:%s\n", host);   // TODO: remove
			setOriginHost(GET_DATA(key), host); // set host
			int port = getPortTarget(&(parseRequest->targetParser));
			// TODO:set port
			printf("TARGET Port:%d\n", port); // TODO: remove
		}
	}
	return bytesFromAParser;
}

int handleVersion(struct selector_key *key, struct parseRequest *parseRequest,
				  buffer *readBuffer, unsigned *ret) {
	int hasToContinue	= 0;
	int bytesFromAParser = parse(parseRequest, readBuffer, &hasToContinue,
								 &parseVersionCharWrapper);
	if (!hasToContinue) {
		if (getVersionState(&parseRequest->versionParser) == ERROR_V) {
			*ret = ERROR; // TODO: check
		}
		else if (getOriginHost(GET_DATA(key)) == NULL ||
				 getOriginHost(GET_DATA(key))[0] == 0) {
			parseRequest->state = PARSE_HEADER;
			printf("VERSION:%d.%d\n",
				   getVersionVersionParser(&parseRequest->versionParser)[0],
				   getVersionVersionParser(
					   &parseRequest
							->versionParser)[1]); // TODO:remove and set verison
		}
		else {
			*ret = DONE; // TODO: put connect and resolve state
		}
	}
	return bytesFromAParser;
}

int handleHeader(struct selector_key *key, struct parseRequest *parseRequest,
				 buffer *readBuffer, unsigned *ret) {
	int hasToContinue	= 0;
	int bytesFromAParser = parse(parseRequest, readBuffer, &hasToContinue,
								 &parseHeaderCharWrapper);
	if (!hasToContinue) {
		if (hasFoundHostHeaderParser(
				&parseRequest->headerParser)) { // TODO: drop error
			*ret	   = DONE; // TODO: put connect and resolve state
			char *host = getHostHeaderParser(&(parseRequest->headerParser));
			printf("Header Host:%s\n", host); // TODO: remove
		}
		else {
			*ret = ERROR;
		}
	}
	return bytesFromAParser;
}

int parse(struct parseRequest *parseRequest, buffer *input, int *flag,
		  int (*parseChar)(struct parseRequest *, char)) {
	uint8_t letter;
	int ret = 0;
	*flag   = 1;

	do {
		letter = buffer_read(input);

		if (letter) {
			*flag = (*parseChar)(parseRequest, letter);
			ret++;
		}
	} while ((*flag) && letter);

	return ret;
}

int parseMethodCharWrapper(struct parseRequest *parseRequest, char letter) {
	return parseMethodChar(&parseRequest->methodParser, letter);
}

int parseTargetCharWrapper(struct parseRequest *parseRequest, char letter) {
	return parseTargetChar(&parseRequest->targetParser, letter);
}

int parseVersionCharWrapper(struct parseRequest *parseRequest, char letter) {
	return parseVersionChar(&parseRequest->versionParser, letter);
}

int parseHeaderCharWrapper(struct parseRequest *parseRequest, char letter) {
	return parseHeaderChar(&parseRequest->headerParser, letter);
}
