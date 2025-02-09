#ifndef CORE_INTERNAL_H
#define CORE_INTERNAL_H

#include <glib.h>
#include "../include/venv_analyzer.h"

// Internal core analyzer functions
bool venv_analyzer_check_conflicts(VenvAnalyzer* analyzer);
const char* venv_analyzer_get_last_error(void);

#endif // CORE_INTERNAL_H
