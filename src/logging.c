#include "logging.h"
#include "assert.h"
#include "memory.h"
#include "platform.h"
#include "util.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct log_message_t
{
    log_severity_e severity;
    uint64_t first_line;
    uint64_t line_count;
} log_message_t;

static bool writing_message;
static char* buffer;
static char* cursor;
static uint64_t* line_indices;
static uint64_t line_count;

static log_message_t* messages;
static uint64_t message_count;

#define BUFFER_SIZE Gibi(1)

void log_init(mem_allocator_i* alloc)
{
    buffer = mem_alloc(alloc, BUFFER_SIZE);
    messages = mem_alloc(alloc, BUFFER_SIZE);
    line_indices = mem_alloc(alloc, BUFFER_SIZE);

    cursor = buffer;

    line_indices[0] = 0;
}

static void print_message(const log_message_t* message)
{
    const char* severity_str = "UNKNOWN";
    switch (message->severity)
    {
    case DEBUG:
        severity_str = "DEBUG";
        break;
    case INFO:
        severity_str = "INFO";
        break;
    case WARNING:
        severity_str = "WARNING";
        break;
    case ERROR:
        severity_str = "ERROR";
        break;
    }

    int indent = fprintf(stderr, "[%s] ", severity_str);

    for (uint64_t line = message->first_line;
         line < message->first_line + message->line_count;
         line++)
    {
        char* line_text = &buffer[line_indices[line]];

        int length = line_indices[line + 1] - line_indices[line];

        if (line > message->first_line)
        {
            for (int i = 0; i < indent; i++)
            {
                fprintf(stderr, " ");
            }
        }

        fprintf(stderr, "%.*s", length, line_text);
    }
}

static void endline()
{
    *cursor++ = '\n';
    log_message_t* message = &messages[message_count - 1];
    message->line_count++;

    line_indices[++line_count] = cursor - buffer;
}

static void finish_message()
{
    ASSERT(writing_message);
    // don't add a new line if last line is empty.
    if (cursor - &buffer[line_indices[line_count]] > 0)
    {
        endline();
    }
    print_message(&messages[message_count - 1]);

    writing_message = false;
}

void log_flush()
{
    if (writing_message)
    {
        finish_message();
    }
}

void log_terminate()
{
    log_flush();

    platform_virtual_free(buffer, BUFFER_SIZE);
    platform_virtual_free(messages, BUFFER_SIZE);
    platform_virtual_free(line_indices, BUFFER_SIZE);
}

static void vlog_continue(const char* fmt, va_list args)
{
    int length = vsprintf(cursor, fmt, args);

    for (int i = 0; i < length; i++)
    {
        if (*cursor == '\n')
        {
            endline();
        }
        else
        {
            cursor++;
        }
    }
}

static void vlog_message(log_severity_e severity, const char* fmt, va_list args)
{
    log_flush();

    ASSERT(!writing_message);
    writing_message = true;
    log_message_t* message = &messages[message_count++];

    message->severity = severity;
    message->first_line = line_count;
    message->line_count = 0;

    vlog_continue(fmt, args);
}

void log_message(log_severity_e severity, const char* fmt, ...)
{
    if (severity >= ERROR)
    {
        BREAKPOINT();
    }
    va_list args;
    va_start(args, fmt);

    vlog_message(severity, fmt, args);

    va_end(args);
}

void log_continue(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    vlog_continue(fmt, args);

    va_end(args);
}
