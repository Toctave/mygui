#pragma once

typedef enum log_severity_e
{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
} log_severity_e;

void log_init();
void log_terminate();

void log_message(log_severity_e severity, const char* fmt, ...);
void log_continue(const char* fmt, ...);

#define log_debug(...) log_message(DEBUG, __VA_ARGS__)
#define log_info(...) log_message(INFO, __VA_ARGS__)
#define log_warning(...) log_message(WARNING, __VA_ARGS__)
#define log_error(...) log_message(ERROR, __VA_ARGS__)
