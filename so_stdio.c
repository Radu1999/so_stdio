#include "so_stdio.h"
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUF_SIZE 4096

typedef enum { READ, WRITE, CLEAN } operation;

struct _so_file {
	int fd;
	unsigned char buffer[BUF_SIZE];
	int read_cursor;
	int write_cursor;
	operation last_op;
	int eof;
	int has_error;
	pid_t child_pid;
};

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *file = malloc(sizeof(SO_FILE));

	if (file == NULL)
		return NULL;

	file->write_cursor = 0;
	file->read_cursor = 0;
	file->last_op = CLEAN;
	file->eof = 0;
	file->has_error = 0;

	int open_mode = 0;

	if (!strcmp(mode, "r"))
		open_mode = O_RDONLY;
	else if (!strcmp(mode, "r+"))
		open_mode = O_RDWR;
	else if (!strcmp(mode, "w"))
		open_mode = O_WRONLY | O_CREAT | O_TRUNC;
	else if (!strcmp(mode, "w+"))
		open_mode = O_RDWR | O_CREAT | O_TRUNC;
	else if (!strcmp(mode, "a"))
		open_mode = O_APPEND | O_CREAT | O_WRONLY;
	else if (!strcmp(mode, "a+"))
		open_mode = O_APPEND | O_RDWR;
	else {
		free(file);
		return NULL;
	}
	file->fd = open(pathname, open_mode, 0644);
	if (file->fd == -1) {
		free(file);
		return NULL;
	}
	return file;
}

int so_fclose(SO_FILE *stream)
{
	if (stream == NULL)
		return SO_EOF;
	int fflush_ret = 0;

	if (stream->last_op == WRITE)
		fflush_ret = so_fflush(stream);
	int r = close(stream->fd);

	if (r < 0 || fflush_ret < 0) {
		stream->has_error = 1;
		free(stream);
		return SO_EOF;
	}
	free(stream);
	return 0;
}

int so_fgetc(SO_FILE *stream)
{
	if (stream == NULL)
		return SO_EOF;
	stream->last_op = READ;
	if (stream->write_cursor == 0 ||
	    stream->read_cursor == stream->write_cursor) {
		stream->write_cursor =
		    read(stream->fd, stream->buffer, BUF_SIZE);
		stream->read_cursor = 0;
		if (stream->write_cursor <= 0) {
			if (stream->write_cursor == 0)
				stream->eof = 1;
			stream->has_error = 1;
			return SO_EOF;
		}
	}
	return stream->buffer[stream->read_cursor++];
}

int so_fputc(int c, SO_FILE *stream)
{
	if (stream == NULL)
		return SO_EOF;
	stream->last_op = WRITE;
	if (stream->write_cursor < BUF_SIZE)
		stream->buffer[stream->write_cursor++] = c;
	if (stream->write_cursor == BUF_SIZE) {
		int r = so_fflush(stream);

		if (r < 0) {
			stream->has_error = 1;
			return SO_EOF;
		}
	}
	return c;
}

int so_fflush(SO_FILE *stream)
{
	if (stream == NULL)
		return SO_EOF;

	int count = stream->write_cursor;

	size_t bytes_written = 0;

	while (bytes_written < count) {
		ssize_t bytes_written_now =
		    write(stream->fd, stream->buffer + bytes_written,
			  count - bytes_written);

		if (bytes_written_now < 0) {
			stream->has_error = 1;
			return SO_EOF;
		}
		bytes_written += bytes_written_now;
	}

	stream->write_cursor = 0;
	stream->last_op = CLEAN;
	stream->read_cursor = 0;

	return 0;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	if (stream == NULL)
		return 0;
	size_t total = size * nmemb;
	int count = 0;

	for (int i = 1; i <= total; i++, ptr++) {
		int elem = so_fgetc(stream);

		if (elem == SO_EOF)
			break;
		if (i % size == 0)
			count++;
		*(unsigned char *)ptr = elem;
	}
	return count;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	if (stream == NULL)
		return 0;
	size_t total = size * nmemb;

	for (int i = 0; i < total; i++, ptr++) {
		int elem = *(unsigned char *)ptr;
		int r = so_fputc(elem, stream);

		if (r == SO_EOF)
			return 0;
	}
	return nmemb;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	if (stream == NULL) {
		return -1;
	}
	if (stream->last_op == WRITE) {
		int r = so_fflush(stream);

		if (r < 0)
			return -1;
	}
	if (stream->last_op == READ) {
		stream->read_cursor = 0;
		stream->write_cursor = 0;
	}
	stream->last_op = CLEAN;
	int r = lseek(stream->fd, offset, whence);

	if (r < 0) {
		stream->has_error = 1;
		return -1;
	}

	return 0;
}

int so_fileno(SO_FILE *stream)
{
	if (stream == NULL)
		return SO_EOF;
	return stream->fd;
}

int so_feof(SO_FILE *stream)
{
	if (stream == NULL)
		return SO_EOF;
	return stream->eof;
}

int so_ferror(SO_FILE *stream)
{
	if (stream == NULL)
		return SO_EOF;
	return stream->has_error;
}

long so_ftell(SO_FILE *stream)
{
	int r = lseek(stream->fd, 0, SEEK_CUR);

	if (r < 0) {
		stream->has_error = 1;
		return -1;
	}
	if (stream->last_op == READ && stream->read_cursor != 0)
		return r - BUF_SIZE + stream->read_cursor;
	if (stream->last_op == WRITE)
		return r + stream->write_cursor;
	return r;
}

int execute_command(char *command)
{

	char *args[10];
	int args_len = 0;
	char delim[] = " \t\n";
	char *token = strtok((char *)command, delim);
	int next_is_out = 0;
	int next_is_in = 0;
	char *outfile = NULL;
	char *infile = NULL;

	while (token != NULL) {
		if (!strcmp(token, ">"))
			next_is_out = 1;
		else if (!strcmp(token, "<"))
			next_is_in = 1;
		else if (next_is_out) {
			outfile = strdup(token);
			next_is_out = 0;
		} else if (next_is_in) {
			infile = strdup(token);
			next_is_in = 0;
		} else
			args[args_len++] = strdup(token);
		token = strtok(NULL, delim);
	}
	args[args_len++] = NULL;
	if (outfile != NULL) {
		SO_FILE *f = so_fopen(outfile, "w");

		dup2(f->fd, STDOUT_FILENO);
	}
	if (infile != NULL) {
		SO_FILE *f = so_fopen(infile, "w");
		
		dup2(f->fd, STDIN_FILENO);
	}
	int ret = execvp(args[0], (char *const *)args);

	if (ret < 0)
		return -1;
	return 0;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	int fds[2];
	int rc = pipe(fds);

	if (rc < 0)
		return NULL;

	pid_t pid;

	pid = fork();

	int child = 0;
	int parrent = 1;

	if (!strcmp(type, "r")) {
		child = 1;
		parrent = 0;
	}

	if (pid == -1) {
		close(fds[0]);
		close(fds[1]);
		return NULL;
	} else if (pid == 0) {
		close(fds[parrent]);
		if (!strcmp(type, "w")) {
			int rc = dup2(fds[child], STDIN_FILENO);

			if (rc < 0)
				return NULL;
		}
		if (!strcmp(type, "r")) {
			int rc = dup2(fds[child], STDOUT_FILENO);

			if (rc < 0)
				return NULL;
		}
		execute_command((char *)command);

	} else {
		close(fds[child]);
		SO_FILE *file = malloc(sizeof(SO_FILE));

		if (file == NULL)
			return NULL;
		file->write_cursor = 0;
		file->read_cursor = 0;
		file->last_op = CLEAN;
		file->eof = 0;
		file->has_error = 0;
		file->fd = fds[parrent];
		file->child_pid = pid;
		return file;
	}
}

int so_pclose(SO_FILE *stream)
{
	pid_t child_pid = stream->child_pid;

	so_fclose(stream);
	int rc = waitpid(child_pid, NULL, 0);

	if (rc < 0)
		return -1;
	return 0;
}
