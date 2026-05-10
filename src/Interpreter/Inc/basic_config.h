#ifndef BASIC_CONFIG_H
#define BASIC_CONFIG_H

/*
 * Embedded profile for the MY-BASIC core.
 *
 * Keep the upstream interpreter code intact where possible, but remove direct
 * dependency on desktop file/stdin behavior from this firmware build.
 */
#ifndef MB_DISABLE_LOAD_FILE
#define MB_DISABLE_LOAD_FILE
#endif

#ifndef MB_DISABLE_DEFAULT_INPUT
#define MB_DISABLE_DEFAULT_INPUT
#endif

#ifndef MB_MANUAL_REAL_FORMATTING
#define MB_MANUAL_REAL_FORMATTING
#endif

#endif
