#ifndef VENV_TYPES_H
#define VENV_TYPES_H

#include "../include/venv_analyzer.h"

// Only keep the enum definitions and any other non-duplicate types
typedef enum {
    COL_NAME,
    COL_VERSION,
    COL_SIZE,
    COL_CONFLICTS,
    N_COLUMNS
} PackageListColumns;

#endif // VENV_TYPES_H