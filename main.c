#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * \def MAX_ENTRIES
 * Maximum possible number of extensions (some sensible bound; as of mid-2026
 * Khronos has 1110, with approximately 60 others). Increase if \c extension
 * errors with \e excess \e elements when compiling.
 */
#define MAX_ENTRIES 1200

/**
 * Container type for calculated hashes.
 */
typedef uint32_t HashLookup[MAX_ENTRIES];

/**
 * All the Khronos extensions (with a \c NULL as the last used entry).
 */
static const char* extension[MAX_ENTRIES] = {
	#include "gl.inl"   // GL extensions
	#include "arb.inl"  // ARB extensions
	#include "es.inl"   // GL ES extensions
	#include "misc.inl" // Anything else
	NULL
};

/**
 * Hash calculated from the full extension string (including any prefix), with
 * indices matching <tt>extension</tt>.
 */
static HashLookup lookup = {0};

/**
 * Hash calculated from the extension with the \c GL_ prefix removed (any other
 * prefix, e.g.: <tt>GLX_</tt>, is not removed).
 *
 * \sa ::lookup
 */
static HashLookup unpref = {0};

/**
 * Searches both the extension and the un-prefixed extension hashes.
 *
 * \param[in] hash 32-bit hash to search for
 * \return either the matching index or \c MAX_ENTRIES if no match is found
 */
static unsigned find(uint32_t const hash) {
	assert(hash);
	for (unsigned n = 0; n < MAX_ENTRIES; n++) {
		if (lookup[n] == hash || unpref[n] == hash) {
			return n;
		}
	}
	return MAX_ENTRIES;
}

/**
 * Hash function adapted from Knuth's TAOCP.
 *
 * \param[in] str start of the data to hash
 * \param[in] len number of bytes
 * \return a 32-bit hash
 */
static uint32_t hash32(const char* str, size_t const len) {
	uint32_t hash = (uint32_t) len;
	for (uint32_t n = hash; n > 0; n--) {
		hash = ((hash << 5) ^ (hash >> 27)) ^ *str++;
	}
	return hash;
}

/**
 * Standalone test of the embedded Khronos extension strings.
 */
int main() {
	for (unsigned n = 0; n < MAX_ENTRIES; n++) {
		const char* ext = extension[n];
		if (ext != NULL) {
			size_t len = strlen(ext);
			if (len) {
				// Sanity test that the extension names were copied correctly
				if (strcspn(ext, " \t") != len) {
					printf("Leading or trailing space for '%s'\n", ext);
					return EXIT_FAILURE;
				}
				// Hash the full extension name
				uint32_t hash = hash32(ext, len);
				unsigned found = find(hash);
				lookup[n] = hash;
				// Any duplicates?
				if (found < MAX_ENTRIES) {
					if (strcmp(ext, extension[n]) == 0) {
					#ifndef NDEBUG
						// Ignore real duplicate strings (probably ES)
						printf("Ignoring duplicate '%s' (%d and %d)\n", ext, n, found);
					#endif
					} else {
						printf("Collision for '%s' at %d (with '%s' at %d)\n", ext, n, extension[n], found);
						return EXIT_FAILURE;
					}
				} else {
					// Try the same but removing the GL_ prefix (but not others)
					if (len > 3 && strncmp(ext, "GL_", 3) == 0) {
						hash = hash32(ext + 3, len - 3);
						found = find(hash);
						unpref[n] = hash;
						if (found < MAX_ENTRIES) {
							printf("Collision for '%s' at %d (with '%s' at %d)\n", ext + 3, n, extension[n], found);
							return EXIT_FAILURE;
						}
					}
				}
			} else {
				printf("Empty string at index %d\n", n);
				return EXIT_FAILURE;
			}
		} else {
			printf("No collisions found (%d extensions tested)\n", n + 1);
			break;
		}
	}
	return EXIT_SUCCESS;
}
