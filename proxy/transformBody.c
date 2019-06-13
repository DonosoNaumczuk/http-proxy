#include <stdint.h>
#include <buffer.h>
#include <httpProxyADT.h>
#include <transformBody.h>
#include <configuration.h>
#include <handleParsers.h>
#include <http.h>

// TODO REMOVE HEADER
#include <stdio.h>
#include <errno.h>

void transformBodyInit(const unsigned state, struct selector_key *key) {
	struct transformBody *transformBody = getTransformBodyState(GET_DATA(key));
	transformBody->commandStatus		= executeTransformCommand(key);
	printf("arrived to transform body state\n");
	buffer_reset(getReadBuffer(GET_DATA(key)));
}

unsigned transformBodyRead(struct selector_key *key) {
	httpADT_t state						= GET_DATA(key);
	struct transformBody *transformBody = getTransformBodyState(state);
	unsigned ret;
	if (transformBody->commandStatus != TRANSFORM_COMMAND_OK) {
		ret = standardOriginRead(key);
	}
	else if (key->fd == transformBody->readFromTransformFd) {
		ret = readFromTransform(key);
	}
	else if (key->fd == getOriginFd(state)) {
		ret = readFromOrigin(key);
	}
	else {
		ret = ERROR;
	}
	return ret;
}

unsigned transformBodyWrite(struct selector_key *key) {
	httpADT_t state						= GET_DATA(key);
	struct transformBody *transformBody = getTransformBodyState(state);
	unsigned ret;
	if (transformBody->commandStatus != TRANSFORM_COMMAND_OK) {
		ret = standardClientWrite(key);
	}
	else if (key->fd == transformBody->writeToTransformFd) {
		ret = writeToTransform(key);
	}
	else if (key->fd == getClientFd(state)) {
		ret = writeToClient(key);
	}
	else {
		ret = ERROR;
	}
	return ret;
}

unsigned standardOriginRead(struct selector_key *key) {
	buffer *writeBuffer = getWriteBuffer(GET_DATA(key));
	uint8_t *pointer;
	size_t count;
	ssize_t bytesRead;
	unsigned ret;

	// if there is no space to read should write what i already read
	if (!buffer_can_write(writeBuffer)) {
		// set interest no op on fd an write on origin fd
		return setStandardFdInterests(key);
	}

	pointer   = buffer_write_ptr(writeBuffer, &count);
	bytesRead = recv(key->fd, pointer, count, 0);

	if (bytesRead > 0) {
		buffer_write_adv(writeBuffer, bytesRead);
		ret = setStandardFdInterests(key);
	}
	else if (bytesRead == 0) {
		// if response is not chunked or is last chunk
		ret = DONE; // should send what is left on buffer TODO
	}
	else {
		ret = ERROR;
	}

	return ret;
}

unsigned readFromTransform(struct selector_key *key) {
	buffer *buffer = getReadBuffer(GET_DATA(key));
	uint8_t *pointer;
	size_t count;
	ssize_t bytesRead;
	unsigned ret;

	// if there is no space to read should write what i already read
	if (!buffer_can_write(buffer)) {
		// set interest no op on fd an write on origin fd
		return setFdInterestsWithTransformerCommand(key);
	}

	pointer   = buffer_write_ptr(buffer, &count);
	bytesRead = read(key->fd, pointer, count);

	if (bytesRead > 0) {
		buffer_write_adv(buffer, bytesRead);
		ret = setFdInterestsWithTransformerCommand(key);
	}
	else if (bytesRead == 0) {
		// if response is not chunked or is last chunk
		ret = DONE; // should send what is left on buffer TODO
	}
	else {
		ret = ERROR;
	}

	return ret;
}

unsigned readFromOrigin(struct selector_key *key) {
	buffer *writeBuffer = getWriteBuffer(GET_DATA(key));
	uint8_t *pointer;
	size_t count;
	ssize_t bytesRead;
	unsigned ret;

	// if there is no space to read should write what i already read
	if (!buffer_can_write(writeBuffer)) {
		// set interest no op on fd an write on origin fd
		return setFdInterestsWithTransformerCommand(key);
	}

	pointer   = buffer_write_ptr(writeBuffer, &count);
	bytesRead = recv(key->fd, pointer, count, 0);

	if (bytesRead > 0) {
		buffer_write_adv(writeBuffer, bytesRead);
		ret = setFdInterestsWithTransformerCommand(key);
	}
	else if (bytesRead == 0) {
		// if response is not chunked or is last chunk
		ret = DONE; // should send what is left on buffer TODO
	}
	else {
		ret = ERROR;
	}

	return ret;
}

unsigned standardClientWrite(struct selector_key *key) {
	buffer *writeBuffer = getWriteBuffer(GET_DATA(key));
	unsigned ret		= TRANSFORM_BODY;
	uint8_t *pointer;
	size_t count;
	ssize_t bytesRead;

	// if everything is read on buffer
	if (!buffer_can_read(writeBuffer)) {
		// set interest no op on fd an read on client fd
		return setStandardFdInterests(key);
	}

	pointer   = buffer_read_ptr(writeBuffer, &count);
	bytesRead = send(key->fd, pointer, count, 0);

	if (bytesRead > 0) {
		buffer_read_adv(writeBuffer, bytesRead);
		ret = setStandardFdInterests(key);
	}
	else {
		ret = ERROR;
	}

	return ret;
}

unsigned writeToTransform(struct selector_key *key) {
	buffer *buffer = getWriteBuffer(GET_DATA(key));
	unsigned ret   = TRANSFORM_BODY;
	uint8_t *pointer;
	size_t count;
	ssize_t bytesRead;

	// if everything is read on buffer
	if (!buffer_can_read(buffer)) {
		// set interest no op on fd an read on client fd
		return setFdInterestsWithTransformerCommand(key);
	}

	pointer   = buffer_read_ptr(buffer, &count);
	bytesRead = write(key->fd, pointer, count);

	if (bytesRead > 0) {
		buffer_read_adv(buffer, bytesRead);
		ret = setFdInterestsWithTransformerCommand(key);
	}
	else {
		printf("error:\n%s\n", strerror(errno));
		ret = ERROR;
	}

	return ret;
}

unsigned writeToClient(struct selector_key *key) {
	buffer *buffer = getReadBuffer(GET_DATA(key));
	unsigned ret   = TRANSFORM_BODY;
	uint8_t *pointer;
	size_t count;
	ssize_t bytesRead;

	// if everything is read on buffer
	if (!buffer_can_read(buffer)) {
		// set interest no op on fd an read on client fd
		return setFdInterestsWithTransformerCommand(key);
	}

	pointer   = buffer_read_ptr(buffer, &count);
	bytesRead = send(key->fd, pointer, count, 0);

	if (bytesRead > 0) {
		buffer_read_adv(buffer, bytesRead);
		ret = setFdInterestsWithTransformerCommand(key);
	}
	else {
		ret = ERROR;
	}

	return ret;
}

unsigned setStandardFdInterests(struct selector_key *key) {
	httpADT_t state		= GET_DATA(key);
	buffer *writeBuffer = getWriteBuffer(GET_DATA(key));
	;
	unsigned ret	   = TRANSFORM_BODY;
	int clientInterest = OP_NOOP;
	int originInterest = OP_NOOP;

	if (buffer_can_read(writeBuffer)) {
		clientInterest |= OP_WRITE;
	}

	if (buffer_can_write(writeBuffer)) {
		originInterest |= OP_READ;
	}

	if (SELECTOR_SUCCESS !=
			selector_set_interest(key->s, getClientFd(state), clientInterest) ||
		SELECTOR_SUCCESS !=
			selector_set_interest(key->s, getOriginFd(state), originInterest)) {
		return ERROR;
	}

	return ret;
}

unsigned setFdInterestsWithTransformerCommand(struct selector_key *key) {
	httpADT_t state						= GET_DATA(key);
	struct transformBody *transformBody = getTransformBodyState(GET_DATA(key));
	buffer *readBuffer					= getReadBuffer(GET_DATA(key));
	buffer *writeBuffer					= getWriteBuffer(GET_DATA(key));

	unsigned ret			   = TRANSFORM_BODY;
	int clientInterest		   = OP_NOOP;
	int originInterest		   = OP_NOOP;
	int transformReadInterest  = OP_NOOP;
	int transformWriteInterest = OP_NOOP;

	if (buffer_can_read(writeBuffer)) {
		transformWriteInterest |= OP_WRITE;
	}

	if (buffer_can_read(readBuffer)) {
		clientInterest |= OP_WRITE;
	}

	if (buffer_can_write(readBuffer)) {
		transformReadInterest |= OP_READ;
	}

	if (buffer_can_write(writeBuffer)) {
		originInterest |= OP_READ;
	}

	if (SELECTOR_SUCCESS !=
			selector_set_interest(key->s, getClientFd(state), clientInterest) ||
		SELECTOR_SUCCESS !=
			selector_set_interest(key->s, getOriginFd(state), originInterest) ||
		SELECTOR_SUCCESS !=
			selector_set_interest(key->s, transformBody->readFromTransformFd,
								  transformReadInterest) ||
		SELECTOR_SUCCESS !=
			selector_set_interest(key->s, transformBody->writeToTransformFd,
								  transformWriteInterest)) {
		return ERROR;
	}

	return ret;
}

int executeTransformCommand(struct selector_key *key) {
	httpADT_t state						= GET_DATA(key);
	struct transformBody *transformBody = getTransformBodyState(state);
	int inputPipe[]						= {-1, -1};
	int outputPipe[]					= {-1, -1};
	int errorFd =
		-1; // TODO open /dev/null or the setted in config and dup2 stderr
	char *commandPath = getCommand(getConfiguration());
	pid_t commandPid;

	if (pipe(inputPipe) == -1 || pipe(outputPipe) == -1) {
		return PIPE_CREATION_ERROR;
	}

	commandPid = fork();
	if (commandPid == -1) {
		return FORK_ERROR;
	}
	else if (commandPid == 0) {
		dup2(inputPipe[0], 0);  // setting pipe as stdin
		dup2(outputPipe[1], 1); // setting pipe as stdout
		// close(inputPipe[0]);	// closing unused copy of pipe
		// close(outputPipe[1]);   // closing unused copy o pipe
		close(inputPipe[1]);  // closing write end of input pipe
		close(outputPipe[0]); // closing read end of output pipe

		if (execl("/bin/sh", "sh", "-c", commandPath, (char *) 0) == -1) {
			// closing other pipes end
			printf("In son\n");
			close(inputPipe[0]);
			close(outputPipe[1]);
			printf("Exec error\n");
			exit(1);
			// return EXEC_ERROR;
		}
	}
	else {
		// In father process
		close(inputPipe[0]);  // closing read end of input pipe
		close(outputPipe[1]); // closing write end of output pipe
		transformBody->writeToTransformFd  = inputPipe[1];
		transformBody->readFromTransformFd = outputPipe[0];
		if (SELECTOR_SUCCESS != selector_register(key->s, inputPipe[1],
												  getHttpHandler(), OP_WRITE,
												  state) ||
			SELECTOR_SUCCESS != selector_register(key->s, outputPipe[0],
												  getHttpHandler(), OP_READ,
												  state)) {
			return SELECT_ERROR;
		}
		incrementReferences(state);
		incrementReferences(state);
	}

	return TRANSFORM_COMMAND_OK;
}